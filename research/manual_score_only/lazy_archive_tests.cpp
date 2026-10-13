#include "pas/manual_score_diagnostics.hpp"
#include <fstream>
#include <iostream>
#include <sstream>

int main(int argc,char** argv)try {
    if(argc!=2)throw std::invalid_argument("fresh software fixture root required");
    namespace fs=std::filesystem;
    using J=nlohmann::json;
    using Archive=pas::SessionArchive;
    const fs::path root=argv[1];
    if(fs::exists(root))throw std::runtime_error("preserve existing fixtures; choose a fresh root");
    unsigned checks=0;
    const auto check=[&](bool pass){++checks;if(!pass)throw std::runtime_error("lazy archive contract failed");};
    for(const auto encoding:{Archive::Encoding::plain,Archive::Encoding::gzip}) {
        const auto absent=root/(encoding==Archive::Encoding::plain?"none-plain":"none-gzip");
        Archive archive(absent,J::object(),16*1024*1024,1024*1024,encoding,Archive::Persistence::none);
        unsigned factories=0;
        for(int i=0;i<20000;++i)pas::emit_manual_diagnostic(archive,1,true,[&]()->J{++factories;throw std::runtime_error("must not construct discarded diagnostics");});
        check(factories==0);check(archive.peak_queue()==0);check(!archive.faulted());check(!fs::exists(absent));
        archive.close();
        bool closed_rejected=false;
        try {pas::emit_manual_diagnostic(archive,0,true,[&]{++factories;return J::object();});}catch(const std::runtime_error&){closed_rejected=true;}
        check(closed_rejected&&factories==0);check(!fs::exists(absent));
    }
    const J value={{"event","synthetic-software-fixture"},{"value",17}};
    {
        Archive direct(root/"direct",{{"fixture","software-only"}});
        direct.event(0,value);direct.close();check(!direct.faulted());
    }
    {
        Archive lazy(root/"lazy",{{"fixture","software-only"}});
        unsigned factories=0;
        pas::emit_manual_diagnostic(lazy,0,false,[&]{++factories;return value;});check(factories==1);
        lazy.close();check(!lazy.faulted());
    }
    const auto bytes=[](const fs::path& p){std::ifstream f(p,std::ios::binary);if(!f)throw std::runtime_error("fixture file missing");std::ostringstream s;s<<f.rdbuf();return s.str();};
    check(bytes(root/"direct"/"standby-0.jsonl")==bytes(root/"lazy"/"standby-0.jsonl"));
    std::cout<<checks<<" lazy diagnostic checks passed; synthetic software fixtures only.\n";
    return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
