#define NOMINMAX
#include <windows.h>
#include <shellapi.h>
#include <nlohmann/json.hpp>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>
#include <chrono>

std::wstring widen(const std::string& value) {
    if(value.empty())return {};
    const int count=MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,value.data(),int(value.size()),nullptr,0);
    if(count<=0)throw std::runtime_error("UTF-8 argument");
    std::wstring result(count,L'\0');
    if(MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,value.data(),int(value.size()),result.data(),count)!=count)
        throw std::runtime_error("argument conversion");
    return result;
}
std::wstring quote(const std::wstring& value) {
    std::wstring result=L"\"";std::size_t slashes=0;
    for(const wchar_t c:value) {
        if(c==L'\\'){++slashes;continue;}
        result.append(slashes*(c==L'"'?2:1),L'\\');slashes=0;
        if(c==L'"')result.push_back(L'\\');result.push_back(c);
    }
    result.append(slashes*2,L'\\');result.push_back(L'"');return result;
}
int WINAPI wWinMain(HINSTANCE,HINSTANCE,PWSTR,int) {
    PROCESS_INFORMATION child{};
    try {
        int argc=0;auto* argv=CommandLineToArgvW(GetCommandLineW(),&argc);
        if(!argv||argc!=2)throw std::runtime_error("one frozen functional specification required");
        const std::filesystem::path spec=argv[1];LocalFree(argv);
        if(!std::filesystem::is_regular_file(spec)||std::filesystem::file_size(spec)>65536)
            throw std::runtime_error("functional specification byte cap");
        std::ifstream input(spec,std::ios::binary);const auto value=nlohmann::json::parse(input);
        if(value.at("schema")!="pas.functional-envelope.v1"||value.at("device_allowed").get<bool>())
            throw std::runtime_error("cold functional scope");
        const auto executable=widen(value.at("executable").get<std::string>());
        if(!std::filesystem::path(executable).is_absolute()||!std::filesystem::is_regular_file(executable))
            throw std::runtime_error("absolute functional executable");
        const auto& arguments=value.at("argv");if(!arguments.is_array()||arguments.size()>32)
            throw std::runtime_error("functional argv bound");
        auto command=quote(executable);
        for(const auto& argument:arguments){const auto arg=argument.get<std::string>();if(arg.size()>4096||arg.find('\0')!=std::string::npos)
            throw std::runtime_error("functional argument bound");command+=L" "+quote(widen(arg));}
        if(command.size()>=32767)throw std::runtime_error("functional command bound");
        const int seconds=value.at("child_seconds").get<int>();if(seconds<1||seconds>1770)throw std::runtime_error("functional deadline");
        STARTUPINFOW startup{};startup.cb=sizeof(startup);startup.dwFlags=STARTF_USESTDHANDLES|STARTF_USESHOWWINDOW;
        startup.wShowWindow=SW_HIDE;startup.hStdInput=GetStdHandle(STD_INPUT_HANDLE);
        startup.hStdOutput=GetStdHandle(STD_OUTPUT_HANDLE);startup.hStdError=GetStdHandle(STD_ERROR_HANDLE);
        std::cout<<"TRACE functional START\n"<<std::flush;
        if(!CreateProcessW(executable.c_str(),command.data(),nullptr,nullptr,TRUE,CREATE_NO_WINDOW,nullptr,nullptr,&startup,&child))
            throw std::runtime_error("functional child creation");
        CloseHandle(child.hThread);child.hThread=nullptr;
        // The unchanged outer Job owns this GUI root and its child. A GUI root
        // has no console host of its own racing image lookup after root exit.
        const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(seconds);
        for(;;){const auto wait=WaitForSingleObject(child.hProcess,100);
            if(wait==WAIT_OBJECT_0)break;if(wait!=WAIT_TIMEOUT)throw std::runtime_error("functional child wait");
            if(std::chrono::steady_clock::now()>=deadline)throw std::runtime_error("functional child deadline");}
        DWORD exit_code=125;if(!GetExitCodeProcess(child.hProcess,&exit_code))throw std::runtime_error("functional child exit");
        CloseHandle(child.hProcess);child.hProcess=nullptr;
        std::cout<<"TRACE functional END exit="<<exit_code<<'\n'<<std::flush;
        Sleep(1000);return int(exit_code);
    }catch(const std::exception& error){
        std::cerr<<error.what()<<'\n';if(child.hThread)CloseHandle(child.hThread);if(child.hProcess)CloseHandle(child.hProcess);
        // Never terminate unrelated processes. A live child forces the outer
        // owned Job to fail its quiescence predicate and perform owned cleanup.
        return 125;
    }
}
