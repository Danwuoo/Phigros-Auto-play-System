#pragma once
#include "../prelive_current/evidence_hash.hpp"
#include "../prelive_current/input_closure.hpp"
#include <map>
#include <set>

// Local, read-only verification of the existing Windows cold build receipts.
// No device APIs, process launch, mutable admission token or enabling route.
namespace pas::windows_qualification {
using J=nlohmann::json;
namespace fs=std::filesystem;
constexpr std::uint64_t MiB=1024*1024;
inline void require(bool value,const char* message) {
    if(!value)throw std::runtime_error(message);
}
inline bool hex(const std::string& value,std::size_t size) {
    return value.size()==size&&value.find_first_not_of("0123456789abcdef")==std::string::npos;
}
inline J read(const fs::path& path,std::uint64_t cap=MiB) {
    require(fs::is_regular_file(path)&&fs::file_size(path)<=cap,"JSON input bound/type");
    std::ifstream input(path,std::ios::binary);
    require(bool(input),"JSON input missing");
    std::vector<std::set<std::string>> objects;
    auto result=J::parse(input,[&](int depth,J::parse_event_t event,J& value) {
        require(depth<=64,"JSON nesting bound");
        if(event==J::parse_event_t::object_start)objects.emplace_back();
        if(event==J::parse_event_t::key) {
            require(!objects.empty()&&objects.back().insert(value.get<std::string>()).second,
                    "duplicate JSON object key");
        }
        if(event==J::parse_event_t::object_end)objects.pop_back();
        return true;
    });
    require(result.is_object(),"JSON object required");
    return result;
}
class Reader final {
    std::map<fs::path,J> visited_;
    std::uint64_t bytes_=0;
public:
    fs::path bound(const J& entry,std::uint64_t cap=1073807360ull) {
        const auto raw=entry.at("path").get<std::string>();
        const fs::path requested(raw);
        require(requested.is_absolute(),"absolute receipt path required");
        for(const auto& part:requested)require(part!=".."&&part!=".","receipt path traversal");
        const auto path=fs::canonical(requested);
        const auto size=entry.at("bytes").get<std::uint64_t>();
        const auto sha=entry.at("sha256").get<std::string>();
        require(hex(sha,64)&&size<=cap&&fs::file_size(path)==size,"receipt size/hash syntax");
        if(const auto found=visited_.find(path);found!=visited_.end()) {
            require(found->second.at("bytes")==size&&found->second.at("sha256")==sha,"conflicting receipt bindings");
            return path;
        }
        require(visited_.size()<8192&&size<=12ull*1024*MiB-bytes_,"verification input capacity");
        require(evidence::sha256(path,cap)==sha,"receipt SHA mismatch");
        bytes_+=size;visited_.emplace(path,entry);return path;
    }
    void entries(const J& rows,std::size_t maximum=8192) {
        require(rows.is_array()&&!rows.empty()&&rows.size()<=maximum,"receipt array capacity");
        std::set<fs::path> paths;
        for(const auto& row:rows)require(paths.insert(bound(row)).second,"duplicate receipt path");
    }
    void finish() const {
        // Catch changes during parsing/hash traversal; validation does not authorize I/O.
        for(const auto& [path,entry]:visited_)
            require(fs::file_size(path)==entry.at("bytes").get<std::uint64_t>()&&
                    evidence::sha256(path)==entry.at("sha256").get<std::string>(),"receipt changed during verification");
    }
    std::size_t count() const {return visited_.size();}
    std::uint64_t bytes() const {return bytes_;}
    J owned(const J& verification_binding,int native,int runner) {
        const auto path=bound(verification_binding,MiB);
        const auto verification=read(path);
        require(!verification.at("pending").get<bool>()&&verification.at("producer_complete").get<bool>()&&
                verification.at("verification_exit")==0&&verification.at("native_exit")==native&&
                verification.at("runner_exit")==runner,"native receipt not verified");
        entries(verification.at("entries"),128);
        const auto stage=verification.at("stage").get<std::string>();
        require(!stage.empty()&&stage.size()<=90&&stage.find_first_not_of("abcdefghijklmnopqrstuvwxyz0123456789-")==std::string::npos,"stage name");
        const auto dir=path.parent_path();
        require(path.filename()==stage+"-verification.json","verification stage/path join");
        const auto result_path=fs::canonical(dir/(stage+"-result.json"));
        require(visited_.contains(result_path),"result not sealed by verification");
        const auto result=read(result_path);
        const auto& facts=result.at("facts");
        require(result.at("stage")==stage&&!result.at("pending").get<bool>()&&
                facts.at("created").get<bool>()&&facts.at("identity_trusted").get<bool>()&&
                facts.at("held_all_signaled").get<bool>()&&facts.at("streams_completed").get<bool>()&&
                facts.at("active_final")==0&&facts.at("exit_code")==native&&facts.at("runner_exit")==runner&&
                facts.at("errors").empty()&&!facts.at("errors_overflow").get<bool>(),"owned process result failure");
        return verification;
    }
    void gate(const J& binding) {
        const auto gate=read(bound(binding,MiB));
        require(gate.at("schema")=="pas.windows-offline-process-gate.v1"&&
                gate.at("file_only_cases")==20&&gate.at("file_only_failed")==0&&
                gate.at("native_controls_verified")==4&&gate.at("wrapper_positive").get<bool>()&&
                gate.at("device_operations")==0&&!gate.at("historical_stop_resumed").get<bool>(),"Windows process gate incomplete");
        const auto diagnostic=read(bound(gate.at("diagnostic"),MiB));
        require(diagnostic.at("failed")==0&&diagnostic.at("cases").size()==20&&
                diagnostic.at("actual_CreateProcess")==0,"diagnostic gate invalid");
        const auto& controls=gate.at("native_controls");
        require(controls.is_array()&&controls.size()==4,"four native controls required");
        const std::map<std::string,std::pair<int,int>> expected{
            {"natural",{0,0}},{"nonzero",{7,7}},{"owned-child",{0,125}},{"probe-1",{0,0}}};
        std::set<std::string> seen;
        for(const auto& control:controls) {
            const auto stage=control.at("stage").get<std::string>();
            require(expected.contains(stage)&&seen.insert(stage).second,"native control set mismatch");
            const auto [native,runner]=expected.at(stage);
            require(control.at("native_exit")==native&&control.at("runner_exit")==runner&&
                    control.at("verification_exit")==0&&control.at("active_final")==0&&
                    control.at("held_all_signaled").get<bool>()&&control.at("streams_completed").get<bool>(),"native control summary mismatch");
            require(owned(control.at("receipt"),native,runner).at("stage")==stage,"native control join");
        }
    }
    J build(const fs::path& path,const std::string& expected_sha,const std::string& commit) {
        require(hex(expected_sha,64)&&hex(commit,40),"external source/seal syntax");
        const J external={{"path",fs::absolute(path).string()},{"bytes",fs::file_size(path)},{"sha256",expected_sha}};
        const auto seal=read(bound(external,4*MiB),4*MiB);
        require(seal.at("schema")=="pas.current-windows-build-binding.v1"&&seal.at("target_commit")==commit,
                "current build/source identity");
        const auto mode=seal.at("mode").get<std::string>();
        require(mode=="release"||mode=="debug"||mode=="asan","build mode");
        require(seal.at("configuration")==((mode=="release")?"Release":"Debug")&&
                !seal.at("windows_cost_qualified").get<bool>()&&!seal.at("live_ready").get<bool>(),"cold receipt scope");
        gate(seal.at("gate"));
        const auto snapshot=read(bound(seal.at("tool_snapshot"),MiB));
        require(snapshot.at("files").is_array()&&!snapshot.at("files").empty()&&snapshot.at("files").size()<=256,"tool snapshot capacity");
        const auto tool_root=fs::canonical(seal.at("tool_snapshot").at("path").get<std::string>()).parent_path()/"bin";
        std::set<fs::path> tools;
        for(const auto& file:snapshot.at("files")) {
            const J copied={{"path",file.at("copy")},{"bytes",file.at("bytes")},{"sha256",file.at("sha256")}};
            const auto copy=bound(copied);
            const auto relative=copy.lexically_relative(tool_root);
            require(!relative.empty()&&!relative.is_absolute()&&*relative.begin()!=".."&&tools.insert(copy).second,"snapshot copy/path mismatch");
        }
        const auto migration=read(bound(seal.at("source_migration"),MiB));
        require(migration.at("schema")=="pas.windows-offline-source-migration.v1"&&migration.at("target_commit")==commit,"source migration identity");
        entries(seal.at("migration_targets"));entries(seal.at("source_files"));
        entries(seal.at("files"));entries(seal.at("build_receipts"));
        const auto build_root=fs::canonical(seal.at("build_root").get<std::string>());
        std::set<fs::path> products;
        for(const auto& file:seal.at("files")) {
            const auto actual=bound(file);const auto relative=actual.lexically_relative(build_root);
            require(!relative.empty()&&!relative.is_absolute()&&*relative.begin()!="..","build artifact containment");
            products.insert(actual);
        }
        for(const auto* name:{"current_chain.exe","prelive_audit.exe","cost_analysis.exe"})
            require(products.contains(fs::canonical(build_root/name)),"required current build artifact missing");
        const auto& closures=seal.at("input_closure_receipts");
        require(closures.is_array()&&closures.size()==2,"configure/build closure pair required");
        std::set<std::string> stages;
        for(const auto& binding:closures) {
            const auto closure=read(bound(binding,MiB));
            const auto state=read(bound(closure.at("state"),MiB));
            const auto stage=closure.at("stage").get<std::string>();
            require((stage.starts_with("configure-prelive-"+mode+"-")||stage==seal.at("build_stage").get<std::string>())&&
                    stages.insert(stage).second,"build closure stage set");
            const auto verification=owned(closure.at("verification"),0,0);
            require(verification.at("stage")==stage&&state.at("status")=="COMPLETE"&&
                    state.at("running").is_null()&&state.at("native_stages_consumed")==1&&
                    state.at("native_launches_known")==1,"build COMPLETE state");
            const auto state_path=fs::canonical(closure.at("state").at("path").get<std::string>());
            const auto directory=state_path.parent_path();
            const auto freeze_path=fs::canonical(directory/"freeze.json");
            const auto contract_path=fs::canonical(directory/"contract.json");
            require(visited_.contains(freeze_path)&&visited_.contains(contract_path)&&
                    state.at("freeze_sha")==visited_.at(freeze_path).at("sha256")&&
                    state.at("contract_sha")==visited_.at(contract_path).at("sha256"),"build state/freeze/contract join");
            bool sealed=false;
            for(const auto& receipt:state.at("receipts"))if(bound(receipt)==bound(closure.at("verification")))sealed=true;
            require(sealed,"build state must seal native verification");
            const auto freeze=read(freeze_path,65536);
            std::map<fs::path,std::string> frozen;
            for(const auto& entry:freeze.at("files"))require(frozen.emplace(bound(entry),entry.at("sha256")).second,"duplicate native frozen input");
            const J process={{"state",closure.at("state")},{"verification",closure.at("verification")}};
            const auto inputs=cost_input_closure::verify(binding,process,stage,frozen);
            for(const auto& [input,sha]:inputs)
                bound({{"path",input.string()},{"bytes",fs::file_size(input)},{"sha256",sha}});
            const auto contract=read(contract_path);
            require(!contract.at("device_allowed").get<bool>(),"build receipt device scope");
        }
        require(stages.contains(seal.at("build_stage").get<std::string>()),"build stage missing");
        finish();
        return {{"schema","pas.windows-cold-qualification-verification.v1"},
            {"cold_build_verified",true},{"mode",mode},{"source_commit",commit},
            {"build_binding_sha256",expected_sha},{"verified_files",count()},{"verified_bytes",bytes()},
            {"live_ready",false},{"device_operations",0},
            {"remaining_gates",J::array({"same-source Windows functional regression", "complete causal pixel prefix",
                "Release cold cost/noise qualification", "production packet/action/wake cost qualification",
                "current device/app/profile/five-contact qualification", "physical Hold semantics and live acceptance"})},
            {"scope","existing Windows cold process/build receipts only; no runtime admission or device authority"}};
    }
};
}
