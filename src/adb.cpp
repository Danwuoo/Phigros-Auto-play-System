#include "pas/adb.hpp"

#include <algorithm>
#include <array>
#include <atomic>
#include <cctype>
#include <chrono>
#include <cstdlib>
#include <fstream>
#include <iterator>
#include <sstream>
#include <stdexcept>
#include <thread>
#include <tuple>
#define NOMINMAX
#include <windows.h>
#include <wincodec.h>
#include <wrl/client.h>

namespace pas {
namespace {
std::wstring wide(const std::string& input) {
    const int size = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, input.data(),
                                         static_cast<int>(input.size()), nullptr, 0);
    if (!size) throw std::invalid_argument("invalid UTF-8 argument");
    std::wstring result(size, L'\0');
    MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, input.data(),
                        static_cast<int>(input.size()), result.data(), size);
    return result;
}

std::wstring quoted(const std::wstring& argument) {
    std::wstring result = L"\"";
    unsigned slashes = 0;
    for (const wchar_t c : argument) {
        if (c == L'\\') { ++slashes; continue; }
        if (c == L'\"') { result.append(slashes * 2 + 1, L'\\'); result += c; slashes = 0; continue; }
        result.append(slashes, L'\\'); slashes = 0; result += c;
    }
    result.append(slashes * 2, L'\\');
    result += L'\"';
    return result;
}

std::string text(const std::vector<std::uint8_t>& bytes) {
    return std::string(bytes.begin(), bytes.end());
}

struct CoInit {
    bool initialized = false;
    CoInit() {
        const auto result = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
        if (FAILED(result) && result != RPC_E_CHANGED_MODE) throw std::runtime_error("COM init failed");
        initialized = SUCCEEDED(result);
    }
    ~CoInit() { if (initialized) CoUninitialize(); }
};

std::tuple<int, int, std::vector<std::uint8_t>> decode_png(const std::vector<std::uint8_t>& png) {
    if (png.size() < 8 || !std::equal(png.begin(), png.begin() + 8,
        std::array<std::uint8_t, 8>{137,80,78,71,13,10,26,10}.begin()))
        throw std::runtime_error("ADB screenshot is not PNG");
    CoInit init;
    using Microsoft::WRL::ComPtr;
    ComPtr<IWICImagingFactory> factory;
    if (FAILED(CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER,
                                IID_PPV_ARGS(&factory)))) throw std::runtime_error("WIC factory failed");
    ComPtr<IWICStream> stream;
    if (FAILED(factory->CreateStream(&stream)) ||
        FAILED(stream->InitializeFromMemory(const_cast<BYTE*>(png.data()), static_cast<DWORD>(png.size()))))
        throw std::runtime_error("WIC stream failed");
    ComPtr<IWICBitmapDecoder> decoder;
    if (FAILED(factory->CreateDecoderFromStream(stream.Get(), nullptr, WICDecodeMetadataCacheOnLoad, &decoder)))
        throw std::runtime_error("WIC PNG decode failed");
    ComPtr<IWICBitmapFrameDecode> frame;
    if (FAILED(decoder->GetFrame(0, &frame))) throw std::runtime_error("WIC PNG frame failed");
    UINT width = 0, height = 0;
    if (FAILED(frame->GetSize(&width, &height)) || !width || !height ||
        static_cast<std::uint64_t>(width) * height * 3 > 16 * 1024 * 1024)
        throw std::runtime_error("WIC PNG dimensions invalid");
    ComPtr<IWICFormatConverter> converter;
    if (FAILED(factory->CreateFormatConverter(&converter)) ||
        FAILED(converter->Initialize(frame.Get(), GUID_WICPixelFormat24bppRGB,
                                     WICBitmapDitherTypeNone, nullptr, 0,
                                     WICBitmapPaletteTypeCustom)))
        throw std::runtime_error("WIC RGB conversion failed");
    std::vector<std::uint8_t> rgb(static_cast<std::size_t>(width) * height * 3);
    if (FAILED(converter->CopyPixels(nullptr, width * 3, static_cast<UINT>(rgb.size()), rgb.data())))
        throw std::runtime_error("WIC pixel copy failed");
    return {static_cast<int>(width), static_cast<int>(height), std::move(rgb)};
}
}

std::filesystem::path find_adb() {
    for (const char* variable : {"ANDROID_SDK_ROOT", "ANDROID_HOME", "LOCALAPPDATA"}) {
        wchar_t value[32768]{};
        const auto length = GetEnvironmentVariableW(wide(variable).c_str(), value,
                                                    static_cast<DWORD>(std::size(value)));
        if (length && length < std::size(value)) {
            auto path = std::filesystem::path(value);
            if (std::string(variable) == "LOCALAPPDATA") path /= "Android/Sdk";
            path /= "platform-tools/adb.exe";
            if (std::filesystem::is_regular_file(path)) return path;
        }
    }
    wchar_t buffer[MAX_PATH]{};
    const DWORD size = SearchPathW(nullptr, L"adb.exe", nullptr, MAX_PATH, buffer, nullptr);
    if (size && size < MAX_PATH) return buffer;
    throw std::runtime_error("adb.exe not found");
}

std::vector<std::uint8_t> adb_call(const std::filesystem::path& adb,
                                   const std::vector<std::string>& arguments, int timeout_ms) {
    if (timeout_ms <= 0 || timeout_ms > 60'000) throw std::invalid_argument("ADB timeout out of range");
    SECURITY_ATTRIBUTES security{sizeof(SECURITY_ATTRIBUTES), nullptr, TRUE};
    HANDLE read_pipe = nullptr, write_pipe = nullptr;
    if (!CreatePipe(&read_pipe, &write_pipe, &security, 0)) throw std::runtime_error("ADB pipe failed");
    SetHandleInformation(read_pipe, HANDLE_FLAG_INHERIT, 0);
    HANDLE null_file = CreateFileW(L"NUL", GENERIC_WRITE, FILE_SHARE_WRITE,
                                   &security, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (null_file == INVALID_HANDLE_VALUE) {
        CloseHandle(read_pipe); CloseHandle(write_pipe); throw std::runtime_error("ADB null sink failed");
    }
    std::wstring command = quoted(adb.wstring());
    for (const auto& arg : arguments) command += L" " + quoted(wide(arg));
    std::vector<wchar_t> mutable_command(command.begin(), command.end());
    mutable_command.push_back(L'\0');
    STARTUPINFOW startup{}; startup.cb = sizeof(startup);
    startup.dwFlags = STARTF_USESTDHANDLES;
    startup.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
    startup.hStdOutput = write_pipe;
    startup.hStdError = null_file;
    PROCESS_INFORMATION process{};
    const BOOL created = CreateProcessW(adb.c_str(), mutable_command.data(), nullptr, nullptr,
                                        TRUE, CREATE_NO_WINDOW, nullptr, nullptr, &startup, &process);
    CloseHandle(write_pipe); CloseHandle(null_file);
    if (!created) { CloseHandle(read_pipe); throw std::runtime_error("ADB launch failed"); }
    std::vector<std::uint8_t> output;
    std::atomic<bool> overflow = false;
    std::thread reader([&] {
        std::array<std::uint8_t, 64 * 1024> buffer{};
        DWORD count = 0;
        while (ReadFile(read_pipe, buffer.data(), static_cast<DWORD>(buffer.size()), &count, nullptr) && count) {
            if (output.size() + count > 32 * 1024 * 1024) { overflow = true; break; }
            output.insert(output.end(), buffer.begin(), buffer.begin() + count);
        }
    });
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeout_ms);
    DWORD wait = WAIT_TIMEOUT;
    while (std::chrono::steady_clock::now() < deadline && !overflow) {
        wait = WaitForSingleObject(process.hProcess, 20);
        if (wait == WAIT_OBJECT_0 || wait == WAIT_FAILED) break;
    }
    const bool timed_out = wait == WAIT_TIMEOUT && !overflow;
    if (wait != WAIT_OBJECT_0 || overflow) TerminateProcess(process.hProcess, 1);
    WaitForSingleObject(process.hProcess, INFINITE);
    reader.join();
    CloseHandle(read_pipe);
    DWORD code = 1;
    GetExitCodeProcess(process.hProcess, &code);
    CloseHandle(process.hThread); CloseHandle(process.hProcess);
    if (wait == WAIT_FAILED) throw std::runtime_error("ADB wait failed");
    if (timed_out) throw std::runtime_error("ADB timed out");
    if (overflow) throw std::runtime_error("ADB output exceeded 32 MiB");
    if (code != 0) throw std::runtime_error("ADB failed with exit code " + std::to_string(code));
    return output;
}

std::string installed_apk_sha256(const std::filesystem::path& adb,
                                 const std::string& serial, const std::string& package) {
    const auto path_bytes = adb_call(adb, {"-s", serial, "shell", "pm", "path", package}, 5000);
    std::string response(path_bytes.begin(), path_bytes.end());
    const auto end = response.find_first_of("\r\n");
    response.resize(end == std::string::npos ? response.size() : end);
    if (response.rfind("package:/data/app/", 0) != 0)
        throw std::runtime_error("native Fixture APK path unavailable");
    const auto path = response.substr(8);
    if (!std::all_of(path.begin(), path.end(), [](unsigned char c) {
            return std::isalnum(c) || c == '/' || c == '_' || c == '-' ||
                   c == '.' || c == '+' || c == '=' || c == '~';
        })) throw std::runtime_error("unexpected Fixture APK path characters");
    const auto hash_bytes = adb_call(adb, {"-s", serial, "shell", "sha256sum", path}, 5000);
    const std::string hash(hash_bytes.begin(), hash_bytes.end());
    if (hash.size() < 65 || hash[64] != ' ' ||
        !std::all_of(hash.begin(), hash.begin() + 64,
                     [](unsigned char c) { return std::isxdigit(c) != 0; }))
        throw std::runtime_error("installed Fixture APK hash unavailable");
    return hash.substr(0, 64);
}

nlohmann::json probe_adb(const std::filesystem::path& adb, const std::string& serial) {
    using json = nlohmann::json;
    const auto devices = text(adb_call(adb, {"devices", "-l"}));
    json list = json::array();
    std::istringstream stream(devices);
    std::string line;
    std::getline(stream, line);
    bool found = false;
    while (std::getline(stream, line)) {
        std::istringstream row(line);
        std::string id, state;
        row >> id >> state;
        if (id.empty()) continue;
        list.push_back({{"serial", id}, {"state", state}});
        if (id == serial && state == "device") found = true;
    }
    json result = {{"adb_path", adb.string()}, {"devices", list},
                   {"capture_candidate", "adb exec-out screencap -p (PNG diagnostic)"},
                   {"touch_candidates", json::array({"adb shell input tap: single tap only", "gRPC independent contacts: Fixture verification required"})}};
    if (serial.empty()) return result;
    if (!found) throw std::runtime_error("selected serial not connected in device state");
    result["selected_serial"] = serial;
    for (const auto& [label, args] : std::vector<std::pair<std::string, std::vector<std::string>>>{
        {"android_release", {"getprop", "ro.build.version.release"}},
        {"android_sdk", {"getprop", "ro.build.version.sdk"}},
        {"model", {"getprop", "ro.product.model"}},
        {"cpu_abi", {"getprop", "ro.product.cpu.abi"}},
        {"wm_size", {"wm", "size"}},
        {"wm_density", {"wm", "density"}}}) {
        auto command = std::vector<std::string>{"-s", serial, "shell"};
        command.insert(command.end(), args.begin(), args.end());
        result[label] = text(adb_call(adb, command));
    }
    return result;
}

Frame capture_adb_png(const Clock& clock, const std::filesystem::path& adb,
                      const std::string& serial, std::uint64_t sequence) {
    const auto png = adb_call(adb, {"-s", serial, "exec-out", "screencap", "-p"}, 15'000);
    const auto captured = clock.now_ns();
    auto [width, height, rgb] = decode_png(png);
    Frame frame;
    frame.sequence = sequence; frame.width = width; frame.height = height; frame.stride = width * 3;
    frame.capture_complete_ns = captured; frame.pixels_ready_ns = clock.now_ns();
    frame.rgb = std::move(rgb);
    return frame;
}

Frame load_diagnostic_png(const std::filesystem::path& path) {
    const auto size=std::filesystem::file_size(path);
    if(size>16*1024*1024) throw std::invalid_argument("diagnostic PNG exceeds capacity");
    std::ifstream file(path,std::ios::binary);
    if(!file) throw std::runtime_error("cannot read diagnostic PNG");
    std::vector<std::uint8_t> png(static_cast<std::size_t>(size));
    if(!file.read(reinterpret_cast<char*>(png.data()),static_cast<std::streamsize>(size)))
        throw std::runtime_error("incomplete diagnostic PNG");
    auto [width,height,rgb]=decode_png(png);
    Frame frame; frame.sequence=1; frame.width=width; frame.height=height; frame.stride=width*3;
    frame.rgb=std::move(rgb); return frame;
}

void launch_android_package(const std::filesystem::path& adb, const std::string& serial,
                            const std::string& package) {
    if (package.empty() || !std::all_of(package.begin(), package.end(), [](unsigned char c) {
            return std::isalnum(c) || c == '.' || c == '_';
        })) throw std::invalid_argument("invalid Android package");
    const auto installed = text(adb_call(adb, {"-s", serial, "shell", "pm", "path", package}));
    if (installed.rfind("package:", 0) != 0) throw std::runtime_error("Android package not installed");
    adb_call(adb, {"-s", serial, "shell", "monkey", "-p", package, "-c",
                   "android.intent.category.LAUNCHER", "1"});
}

} // namespace pas
