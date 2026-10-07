#include "verifier.hpp"
#include <iostream>
using namespace pas::windows_qualification;
int main(int argc,char** argv) {
    if(argc!=5)return 2;
    try {
        const fs::path original=argv[1],root=argv[4];
        require(!fs::exists(root),"fresh test fixture root required");fs::create_directories(root);
        const std::string external_sha=argv[2],commit=argv[3];
        J cases=J::array();
        {Reader reader;const auto result=reader.build(original,external_sha,commit);
            require(result.at("cold_build_verified").get<bool>()&&!result.at("live_ready").get<bool>(),"real build positive control");
            cases.push_back({{"id","real-qualified-cold-build"},{"pass",true},{"verified_files",result.at("verified_files")}});}
        auto negative=[&](const std::string& name,const J& fixture) {
            const auto path=root/(name+".json");
            {std::ofstream output(path,std::ios::binary);output<<fixture.dump(2)<<'\n';require(bool(output),"fixture write");}
            bool rejected=false;std::string reason;
            try {Reader reader;(void)reader.build(path,pas::evidence::sha256(path),commit);}
            catch(const std::exception& error){rejected=true;reason=error.what();}
            cases.push_back({{"id",name},{"pass",rejected},{"reason",reason}});require(rejected,"negative receipt admitted");
        };
        const auto seal=read(original,4*MiB);
        auto fixture=seal;fixture["schema"]="legacy";negative("obsolete-schema",fixture);
        fixture=seal;fixture["target_commit"]=std::string(40,'0');negative("wrong-source-commit",fixture);
        fixture=seal;fixture["live_ready"]=true;negative("caller-live-claim",fixture);
        fixture=seal;fixture["windows_cost_qualified"]=true;negative("AA-cost-as-live-scope",fixture);
        fixture=seal;fixture["configuration"]="Debug";negative("wrong-build-mode",fixture);
        fixture=seal;fixture["gate"]["sha256"]=std::string(64,'0');negative("gate-SHA-tamper",fixture);
        bool wrong_seal=false;try {Reader reader;(void)reader.build(original,std::string(64,'0'),commit);}catch(...){wrong_seal=true;}
        require(wrong_seal,"external seal required");cases.push_back({{"id","wrong-external-seal"},{"pass",true}});
        // No original input is modified. The altered gate receives a fresh local
        // hash so its semantic/control checks, rather than hash syntax, reject it.
        auto gate=read(seal.at("gate").at("path").get<std::string>());
        auto gate_negative=[&](const std::string& name,const J& altered) {
            const auto path=root/(name+"-gate.json");{std::ofstream output(path);output<<altered.dump(2);}
            fixture=seal;fixture["gate"]={{"path",fs::absolute(path).string()},{"bytes",fs::file_size(path)},
                {"sha256",pas::evidence::sha256(path)}};negative(name,fixture);
        };
        auto altered=gate;altered["native_controls"].erase(0);gate_negative("missing-native-control",altered);
        altered=gate;altered["native_controls"][1]=altered["native_controls"][0];gate_negative("duplicate-native-control",altered);
        altered=gate;altered["native_controls"][0]["active_final"]=1;gate_negative("active-process-remaining",altered);
        altered=gate;altered["historical_stop_resumed"]=true;gate_negative("historical-STOP-relabel",altered);
        altered=gate;altered["device_operations"]=1;gate_negative("device-scope-in-cold-gate",altered);
        const auto duplicate=root/"duplicate-key.json";
        {std::ofstream output(duplicate);output<<"{\"live_ready\":true,\"live_ready\":false}";}
        bool duplicate_rejected=false;try{(void)read(duplicate);}catch(...){duplicate_rejected=true;}
        require(duplicate_rejected,"duplicate JSON key admitted");cases.push_back({{"id","duplicate-JSON-key"},{"pass",true}});
        const auto deep=root/"deep.json";{std::ofstream output(deep);output<<std::string(70,'[')<<"0"<<std::string(70,']');}
        bool depth_rejected=false;try{(void)read(deep);}catch(...){depth_rejected=true;}
        require(depth_rejected,"JSON depth admitted");cases.push_back({{"id","JSON-depth-bound"},{"pass",true}});
        J result={{"schema","pas.windows-qualification-controls.v1"},{"pass",true},{"cases",cases},
            {"negative_receipts_grant_live",false},{"device_operations",0},{"originals_modified",false}};
        {std::ofstream output(root/"report.json");output<<result.dump(2)<<'\n';require(bool(output),"report write");}
        std::cout<<"qualification checks="<<cases.size()<<" failed=0\n";return 0;
    }catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
}
