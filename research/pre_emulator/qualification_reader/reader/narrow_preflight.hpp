#pragma once
#include "cooperative_build.hpp"
#include "cost_reader.hpp"
#include "component_delta.hpp"
#include <functional>

// Existing receipt projection, not a new qualification or device format.
// Removed compiler roots and relocated raw are deliberately not traversed.
// Native input-closure metadata is checked; its historical leaves are not
// rehashed as current files. The output states that distinction explicitly.
namespace pas::preflight_graph::narrow {
inline constexpr const char* base_commit="aa9de3c13b92e533b1b0e9cb0312cf81e5d2f9de";
inline bool supplied(const J& input,std::initializer_list<const char*> names){for(const auto* name:names)if(!input.contains(name)||input.at(name).is_null())return false;return true;}
inline bool same_pin(const J& a,const J& b){
    return a.is_object()&&b.is_object()&&same_path_text(fs::path(a.at("path").get<std::string>()),fs::path(b.at("path").get<std::string>()))&&a.at("bytes")==b.at("bytes")&&a.at("sha256")==b.at("sha256");
}
inline void pin_shape(const J& p){
    w::require(p.is_object()&&p.at("path").is_string()&&fs::path(p.at("path").get<std::string>()).is_absolute()&&
        p.at("bytes").is_number_unsigned()&&w::hex(p.at("sha256").get<std::string>(),64),"complete existing receipt binding");
    for(const auto& part:fs::path(p.at("path").get<std::string>()))w::require(part!="."&&part!="..","receipt path traversal");
}
inline bool legacy_priority_header(const J& pin){return pin.at("bytes")==9155&&pin.at("sha256")=="22e2ae774920f223a4f358d800912d86c6fe1976c4452edd8f22d95a1c024cd8";}
inline bool legacy_priority_tests(const J& pin){return pin.at("bytes")==11370&&pin.at("sha256")=="083e294be6fe79d249bf8f89c65c3adedab28cc66beab09b85c5c2406a06a413";}
inline int priority_policy(const J& plan,const J& controls,const J& header,const J& tests){
    pin_shape(header);pin_shape(tests);const bool old_header=legacy_priority_header(header),old_tests=legacy_priority_tests(tests);
    const bool has_plan=plan.contains("consumer_priority_requested"),has_controls=controls.contains("consumer_priority_requested");
    if(!has_plan&&!has_controls){w::require(old_header&&old_tests,"missing priority policy only accepted for exact legacy header/tests");return 1;}
    w::require(has_plan&&has_controls&&plan.at("consumer_priority_requested").is_number_integer()&&controls.at("consumer_priority_requested").is_number_integer()&&plan.at("consumer_priority_requested")==2&&controls.at("consumer_priority_requested")==2,"new priority policy requires matching integer2 plan/controls");
    w::require(!old_header&&!old_tests,"new priority policy cannot mix legacy header/tests");return 2;
}
inline void verify_priority(const J& p,int expected){
    w::require((expected==1||expected==2)&&p.at("requested").is_number_integer()&&p.at("applied_readback").is_number_integer()&&p.at("previous").is_number_integer()&&p.at("restored_readback").is_number_integer()&&p.at("requested")==expected&&p.at("applied_readback")==expected&&p.at("applied").get<bool>()&&p.at("restored").get<bool>()&&p.at("verified").get<bool>()&&p.at("process_class")==32&&p.at("process_class_after_close")==32&&p.at("restored_readback")==p.at("previous")&&p.at("failure")=="none"&&p.at("rollback_failure")=="none"&&!p.at("fallback_attempted").get<bool>(),"actual consumer priority exact requested/readback/restore/NORMAL32 policy");
}
inline bool legacy_tail_header(const J& p){return (p.at("bytes")==4804&&p.at("sha256")=="9e9700107d4f07301ef302351fa645cb705a57b1e049f4166eb70604a116ddc2")||(p.at("bytes")==4998&&p.at("sha256")=="ee2f57c862d0543a5c6855cd43f4a8aeab157b96bc706acf85b6c5a004595c38");}
inline bool legacy_tail_tests(const J& p){return (p.at("bytes")==12026&&p.at("sha256")=="ee3b63b9787b21ebe27bdc3e7b32e571c9a3dd1e7532f5ac1951d8e8932ff7a6")||(p.at("bytes")==12056&&p.at("sha256")=="ffb5f56676b0e0ef23e92b1a1ffe071cbbe644568c5d660d2a8479ff2ddeb058");}
inline std::int64_t active_tail_policy(const J& plan,const J& controls,const J& header,const J& tests){
    pin_shape(header);pin_shape(tests);
    const bool paired=(header.at("bytes")==4804&&header.at("sha256")=="9e9700107d4f07301ef302351fa645cb705a57b1e049f4166eb70604a116ddc2"&&tests.at("bytes")==12026&&tests.at("sha256")=="ee3b63b9787b21ebe27bdc3e7b32e571c9a3dd1e7532f5ac1951d8e8932ff7a6")||(header.at("bytes")==4998&&header.at("sha256")=="ee2f57c862d0543a5c6855cd43f4a8aeab157b96bc706acf85b6c5a004595c38"&&tests.at("bytes")==12056&&tests.at("sha256")=="ffb5f56676b0e0ef23e92b1a1ffe071cbbe644568c5d660d2a8479ff2ddeb058");
    w::require(controls.at("active_tail_ns").is_number_integer(),"integer producer active-tail controls report required");
    if(!plan.contains("producer_active_tail_ns")){w::require(paired&&controls.at("active_tail_ns")==20'000'000,"missing active-tail policy only accepted for an exact paired legacy20ms source");return 20'000'000;}
    w::require(plan.at("producer_active_tail_ns").is_number_integer()&&plan.at("producer_active_tail_ns")==2'000'000&&controls.at("active_tail_ns")==2'000'000&&!legacy_tail_header(header)&&!legacy_tail_tests(tests),"new active-tail policy requires integer2ms plan/report and new compiled header/tests");return 2'000'000;
}
inline void verify_active_tail(const J& report,std::int64_t expected){
    w::require(expected==2'000'000||expected==20'000'000,"known SHA-bound producer active-tail policy");
    if(!report.contains("producer_active_tail_ns")){w::require(expected==20'000'000,"new producer active-tail run scalar missing");return;}
    w::require(report.at("producer_active_tail_ns").is_number_integer()&&report.at("producer_active_tail_ns")==expected,"actual producer active-tail run scalar/type mismatch");
}
inline bool maintenance_path(std::string_view p){
    if(p=="AGENTS.md"||p=="README.md"||p=="research/pre_emulator/PLAN.md")return true;
    if(p.starts_with("research/pre_emulator/")&&p.ends_with(".ps1")&&p.substr(22).find('/')==std::string_view::npos)return true;
    return p.starts_with("docs/")&&(p.ends_with(".md")||p.ends_with(".json"))&&p!="docs/research/zero-miss-20261006/windows/PRELIVE_COST_PROTOCOL_R2_20261007.md";
}
inline void same_parent(const J& current,const J& parent){
    const std::set<std::string> mutable_keys={"source_commit","source_manifest","core_build_plan","changed_paths","candidate_bytes","additional_inputs"};
    const std::set<std::string> added={"compiled_source_commit","maintenance_only_amendment","parent_repair_build","compiled_source_manifest","maintenance_changed_paths","formal_rebuilt","component_rebuilt","amendment_proof"};
    for(const auto& [key,value]:parent.items())w::require(current.contains(key)&&(mutable_keys.contains(key)||current.at(key)==value),("unchanged compiled parent field: "+key).c_str());
    for(const auto& [key,value]:current.items()){(void)value;w::require(parent.contains(key)||added.contains(key),("undeclared maintenance amendment field: "+key).c_str());}
    w::require(current.at("additional_inputs").size()==parent.at("additional_inputs").size(),"same additional input path set");
    const auto& changes=current.at("maintenance_changed_paths");std::set<std::string> unique;
    for(const auto& p:changes){const auto text=p.get<std::string>();w::require(maintenance_path(text)&&unique.insert(text).second,"maintenance-only declared paths");}
    for(std::size_t i=0;i<parent.at("additional_inputs").size();++i){
        const auto& old=parent.at("additional_inputs")[i];const auto& now=current.at("additional_inputs")[i];pin_shape(now);pin_shape(old);
        w::require(same_path_text(old.at("path").get<std::string>(),now.at("path").get<std::string>()),"additional input path changed");
        if(!same_pin(old,now)){
            const auto match=std::any_of(unique.begin(),unique.end(),[&](const auto& relative){return fs::path(now.at("path").get<std::string>()).generic_string().ends_with('/'+relative);});
            w::require(match,"additional SHA change outside declared maintenance paths");
        }
    }
}
struct Native {J evidence,spec,facts;std::map<std::string,J> inputs;};
class Preflight final {
    Reader& r_;fs::path root_;J input_,inventory_,delta_,source_,base_source_,noise_=J::object();
    std::map<std::string,J> processes_,base_sources_,base_files_;
    std::set<std::string> cost_identities_;
    std::string execution_,compiled_,manual_sha_;
    int consumer_priority_requested_=0;
    std::int64_t producer_active_tail_ns_=0;
    J checks_=J::object(),issues_=J::array(),provenance_=J::object();bool rejected_=false,ready_=true,aa_ok_=false,noise_ok_=false;
    CostCampaign aa_,abba_;
    static std::string key(const J& p){pin_shape(p);return fs::path(p.at("path").get<std::string>()).lexically_normal().generic_string();}
    std::string relative(const J& p)const{
        const auto rel=fs::path(p.at("path").get<std::string>()).lexically_relative(root_);
        w::require(!rel.empty()&&!rel.is_absolute()&&*rel.begin()!="..","source member within main");return rel.generic_string();
    }
    J object(const J& pin,std::uint64_t cap=4*w::MiB){pin_shape(pin);return r_.object(pin,cap);}
    const J& required(const char* name)const{w::require(input_.contains(name)&&!input_.at(name).is_null(),("missing input binding: "+std::string(name)).c_str());return input_.at(name);}
    void blocked(const char* section,const std::string& reason){ready_=false;if(!checks_.contains(section))checks_[section]={{"pass",false},{"state","NOT_READY"}};issues_.push_back({{"section",section},{"kind","NOT_READY"},{"reason",reason}});}
    void section(const char* name,const std::function<void()>& action){
        try{action();if(!checks_.contains(name))checks_[name]={{"pass",true}};}
        catch(const std::exception& e){ready_=false;rejected_=true;checks_[name]={{"pass",false}};issues_.push_back({{"section",name},{"kind","REJECT"},{"reason",e.what()}});}
    }
    Native native(const J& p){
        w::require(p.is_object()&&p.size()==8,"eight native process evidence bindings");
        for(const auto* k:{"specification","input_closure","result","verification","freeze","state","contract","command"})r_.bound(p.at(k));
        const auto v=r_.owned(p.at("verification"),0,0,true),state=object(p.at("state")),closure=object(p.at("input_closure"));
        const auto spec=object(p.at("specification")),command=object(p.at("command")),contract=object(p.at("contract")),result=object(p.at("result"));
        const auto stage=v.at("stage");
        w::require(spec.at("stage")==stage&&command.at("stage")==stage&&result.at("stage")==stage&&contract.at("spec").at("name")==stage,"native metadata stage join");
        for(const auto* k:{"exe","argv","native_exit","runner_exit","total_s","stream_cap"})w::require(spec.at(k)==command.at("spec").at(k)&&spec.at(k)==contract.at("spec").at(k),"native frozen/launched command join");
        w::require(spec.at("native_exit")==0&&spec.at("runner_exit")==0&&same_path_text(spec.at("exe").get<std::string>(),result.at("facts").at("root_image").get<std::string>()),"native original image/exit join");
        w::require(state.at("status")=="COMPLETE"&&state.at("running").is_null()&&state.at("native_stages_consumed")==1&&state.at("native_launches_known")==1&&state.at("freeze_sha")==p.at("freeze").at("sha256")&&state.at("contract_sha")==p.at("contract").at("sha256"),"complete sealed native state");
        bool verification_sealed=false;for(const auto& receipt:state.at("receipts"))verification_sealed|=same_pin(receipt,p.at("verification"));w::require(verification_sealed,"native verification sealed by completed state");
        w::require(closure.at("schema")=="pas.current-input-closure-verification.v1"&&closure.at("stage")==stage&&closure.at("status")=="COMPLETE"&&closure.at("pass").get<bool>()&&closure.at("before_verified").get<bool>()&&closure.at("after_verified").get<bool>()&&same_pin(closure.at("verification"),p.at("verification"))&&same_pin(closure.at("state"),p.at("state"))&&!contract.at("device_allowed").get<bool>(),"original completed endpoint-free closure receipt");
        Native n{p,spec,result.at("facts"),{}};const auto root=object(closure.at("root"),32768);
        w::require(root.at("schema")=="pas.current-input-closure.v1"&&root.at("stage")==stage&&root.at("leaf_count")==closure.at("leaf_count")&&root.at("files_count")==closure.at("files_count"),"closure root denominator");
        std::size_t index=0;for(const auto& pin:root.at("leaves")){
            const auto leaf=object(pin,32768);w::require(leaf.at("schema")=="pas.current-input-closure-leaf.v1"&&leaf.at("stage")==stage&&leaf.at("index")==index++&&leaf.at("files").is_array()&&!leaf.at("files").empty()&&leaf.at("files").size()<=64,"ordered original closure leaf");
            for(const auto& f:leaf.at("files"))w::require(n.inputs.emplace(key(f),f).second,"duplicate original closure input");
        }
        w::require(index==root.at("leaves").size()&&index==closure.at("leaf_count")&&n.inputs.size()==closure.at("files_count"),"complete original closure metadata denominator");
        const auto frozen=object(p.at("freeze"),65536);bool root_frozen=false;
        for(const auto& f:frozen.at("files")){pin_shape(f);root_frozen|=same_pin(f,closure.at("root"));const auto [it,inserted]=n.inputs.emplace(key(f),f);w::require(inserted||same_pin(it->second,f),"conflicting closure/freeze input binding");}
        w::require(root_frozen&&n.inputs.contains(key(p.at("specification")))&&same_pin(n.inputs.at(key(p.at("specification"))),p.at("specification")),"closure root/spec frozen before launch");return n;
    }
    Native by_verification(const J& pin){const auto found=processes_.find(key(pin));w::require(found!=processes_.end(),("missing eight-field ProcessEvidence: "+key(pin)).c_str());w::require(same_pin(found->second.at("verification"),pin),"native verification catalogue join");return native(found->second);}
    Native output_process(const J& pin,const J& binary){
        const J* found=nullptr;for(const auto& [k,p]:processes_){(void)k;const auto spec=object(p.at("specification"));const auto& argv=spec.at("argv");
            if(same_path_text(spec.at("exe").get<std::string>(),binary.at("path").get<std::string>())&&!argv.empty()&&argv.back().is_string()&&same_path_text(argv.back().get<std::string>(),pin.at("path").get<std::string>())){w::require(!found,"ambiguous native report output");found=&p;}}
        w::require(found,("missing native process for output: "+pin.at("path").get<std::string>()).c_str());return native(*found);
    }
    J base_exe(const std::string& mode,const char* name){
        for(const auto& row:inventory_.at("cold"))if(row.at("mode")==mode){const auto build=object(row.at("build"));for(const auto& f:build.at("files"))if(fs::path(f.at("path").get<std::string>()).filename()==name)return f;}
        throw std::runtime_error("missing original configuration executable binding");
    }
    void load_base(){
        inventory_=object(required("original_inventory"));
        w::require(inventory_.at("schema")=="pas.main32-stage-inventory.v1"&&inventory_.at("source_commit")==base_commit&&inventory_.at("original_STOPs_preserved").get<bool>()&&!inventory_.at("qualification_issued").get<bool>()&&!inventory_.at("live_ready").get<bool>()&&inventory_.at("device_operations")==0,"original main32 negative/preservation identity");
        const auto snapshot=object(inventory_.at("source_snapshot"));w::require(snapshot.at("source_commit")==base_commit&&!snapshot.at("qualification_issued").get<bool>(),"immutable main32 source snapshot");
        for(const auto& row:snapshot.at("files")){pin_shape(row.at("original"));r_.bound(row.at("copy"));w::require(row.at("copy").at("sha256")==row.at("original").at("sha256")&&row.at("copy").at("bytes")==row.at("original").at("bytes")&&base_sources_.emplace(relative(row.at("original")),row.at("original")).second,"unique byte-identical preserved source");}
        manual_sha_=base_sources_.at("src/manual_session.cpp").at("sha256").get<std::string>();std::set<std::string> modes;
        for(const auto& row:inventory_.at("cold")){
            const auto mode=row.at("mode").get<std::string>();w::require(modes.insert(mode).second,"duplicate old configuration");
            const auto e=object(row.at("evaluation")),b=object(row.at("build"));w::require(e.at("schema")=="pas.cooperative-cold-evaluation.v1"&&e.at("source_commit")==base_commit&&e.at("affected_cold_pass").get<bool>()&&e.at("tests").size()==25&&same_pin(e.at("build"),row.at("build"))&&b.at("schema")=="pas.cooperative-build.v1"&&b.at("source_commit")==base_commit&&b.at("configuration")==mode,"preserved old source/configuration25");
            auto tests=cold_tests();for(const auto& t:e.at("tests")){const auto id=t.at("test").get<std::string>();w::require(tests.erase(id)==1,"exact preserved cold25 set");const auto report=object(t.at("report"));w::require((id=="cost_analysis_tests"||id=="cost_analysis_review_tests")?report.at("pass").get<bool>():report.at("failed")==0,"preserved cold test report PASS");r_.owned(t.at("receipt"),0,0,true);}w::require(tests.empty(),"missing preserved cold group");
            const auto plan=object(b.at("core_build_plan"));w::require(plan.at("schema")=="pas.fresh-core-build-plan.v1"&&plan.at("source_commit")==base_commit&&plan.at("translation_units")==22&&!plan.at("old_donor_objects_used").get<bool>(),"preserved original fresh22 proof");
            const auto manifest=object(b.at("source_manifest"));w::require(manifest.at("source_commit")==base_commit&&!manifest.at("dirty").get<bool>(),"preserved source manifest identity");
            for(const auto& f:manifest.at("files"))w::require(same_pin(f,base_sources_.at(relative(f))),"manifest/immutable source copy join");
            if(mode=="release"){base_source_=manifest;for(const auto& f:b.at("files"))w::require(base_files_.emplace(key(f),f).second,"unique original Release product binding");}
        }
        w::require(modes==std::set<std::string>{"release","debug","asan"},"all three preserved configuration proofs");
        for(const auto& proof:inventory_.at("readonly_graphs")){const auto report=object(proof.at("report"));w::require(report.at("pass").get<bool>()&&report.at("source_commit")==base_commit&&!report.at("qualification_issued").get<bool>()&&report.at("device_operations")==0,"original independent cold graph proof");}
        checks_["base"]={{"pass",true},{"source_commit",base_commit},{"groups_each",25},{"formal_TUs_each",22},{"scope","preserved original proofs; removed graphs not revalidated"}};
    }
    void prefix(){
        const auto p=object(inventory_.at("prefix_receipt")),report=object(p.at("report")),audit=object(p.at("audit"));
        w::require(p.at("schema")=="pas.cooperative-prefix-receipt.v1"&&p.at("source_commit")==base_commit&&p.at("pass").get<bool>()&&p.at("frames")==3063&&p.at("physical_gold")==0&&same_pin(p.at("binary"),base_exe("release","current_chain.exe")),"original prefix source/binary/3063");
        w::require(report.at("schema")=="pas.prelive-current-pixels.v1"&&report.at("processed")==3063&&report.at("frames")==3063&&report.at("release_verified").get<bool>()&&report.at("stop")==""&&report.at("device_endpoints")==0,"entire preserved prefix report");
        w::require(audit.at("schema")=="pas.prelive-independent-integrity.v2"&&audit.at("integrity").get<bool>()&&audit.at("complete_prefix_evidence").get<bool>()&&audit.at("all_input_processed").get<bool>()&&audit.at("metrics_verified").get<bool>()&&audit.at("plan_join_complete").get<bool>()&&audit.at("coordinate_join_complete").get<bool>()&&!audit.at("release_uncertainty_seen").get<bool>()&&audit.at("errors").empty(),"preserved independent prefix audit");
        const auto source=object(p.at("source_manifest"));w::require(source.at("schema")=="pas.current-source-closure.v1"&&source.at("source_commit")==base_commit&&!source.at("dirty").get<bool>(),"original prefix source closure");for(const auto& f:source.at("files"))w::require(same_pin(f,base_sources_.at(relative(f))),"prefix immutable source join");
        const auto producer=by_verification(p.at("producer_receipt")),auditor=by_verification(p.at("auditor_receipt"));
        w::require(producer.spec.at("argv")==J::array({"pixels",p.at("selection").at("path"),p.at("report").at("path")})&&auditor.spec.at("argv")==J::array({p.at("report").at("path"),p.at("audit").at("path")}),"original prefix native commands");
        w::require(same_path_text(producer.spec.at("exe").get<std::string>(),p.at("binary").at("path").get<std::string>())&&same_path_text(auditor.spec.at("exe").get<std::string>(),base_exe("release","prelive_audit.exe").at("path").get<std::string>())&&audit.at("verifier_binary").at("sha256")==base_exe("release","prelive_audit.exe").at("sha256"),"original prefix producer/auditor image joins");
        const auto& raw=audit.at("input_bindings");for(const auto& [name,suffix]:std::array<std::pair<const char*,const char*>,3>{{{"report",""},{"rows",".rows.jsonl"},{"events",".events.jsonl"}}}){const auto& pin=raw.at(name);w::require(same_path_text(pin.at("path").get<std::string>(),p.at("report").at("path").get<std::string>()+suffix)&&auditor.inputs.contains(key(pin))&&same_pin(auditor.inputs.at(key(pin)),pin),"preserved prefix raw binding in original auditor closure");}
        checks_["prefix"]={{"pass",true},{"frames",3063},{"raw_revalidation_performed",false}};
    }
    void production(bool faults){
        const auto status=object(faults?inventory_.at("faults_status"):inventory_.at("production").at("original_status"));
        w::require(status.at("schema")== (faults?"pas.literal-production-boundary-status.v1":"pas.pre-emulator-production-status.v1")&&status.at("source_commit")==base_commit&&!status.at("stopped").get<bool>()&&status.at("failure").is_null()&&status.at("remaining").empty()&&status.at("rows").size()==(faults?4:9),"entire preserved fault/production scope");
        const auto plan=object(status.at("plan"));w::require(plan.at("source_commit")==base_commit&&plan.at("schema")== (faults?"pas.literal-production-boundary-plan.v1":"pas.pre-emulator-production-plan.v1"),"original fault/production plan identity");
        if(faults)w::require(plan.at("scenarios")==J::array({"capture","writer","rpc","slow"})&&plan.at("requested_per_scenario")==1000&&plan.at("warmup")==256&&plan.at("measurement")==744&&plan.at("cadence_ms")==20,"original four fault workloads");else w::require(plan.at("plans").size()==9,"complete original production plan");
        const std::array<const char*,9> ids={"normal-tap","normal-hold","normal-dense","stress-tap","stress-dense","debug-tap","debug-hold","asan-tap","asan-hold"};
        const std::array<const char*,4> fault_ids={"capture","writer","rpc","slow"};std::size_t i=0;
        for(const auto& row:status.at("rows")){
            const auto report=object(row.at("report")),audit=object(row.at("audit"));const auto mode=faults||i<5?"release":i<7?"debug":"asan";
            w::require((faults?row.at("scenario"):row.at("id"))==(faults?fault_ids[i]:ids[i])&&report.at("actual_manual_translation_unit_sha256")==manual_sha_&&report.at("source_build_commit")==base_commit&&report.at("endpoint_free").get<bool>()&&report.at("public_entry_rejected").get<bool>()&&report.at("device_operations")==0&&!report.at("live_ready").get<bool>(),"preserved literal source and planned row order");
            const auto producer=native(row.at("process")),auditor=output_process(row.at("audit"),base_exe(mode,faults?"fault_audit.exe":"production_pipeline_audit.exe"));const auto raw_root=fs::path(row.at("report").at("path").get<std::string>()).parent_path().string();
            w::require(same_path_text(producer.spec.at("exe").get<std::string>(),base_exe(mode,faults?"fault_controls.exe":"production_pipeline_cold.exe").at("path").get<std::string>()),"original configuration-specific producer image");
            if(faults){w::require(report.at("schema")=="pas.literal-production-boundary-controls.v1"&&audit.at("schema")=="pas.literal-production-boundary-audit.v1"&&audit.at("control_pass").get<bool>()&&audit.at("source_commit")==base_commit&&report.at("scenario")==fault_ids[i]&&audit.at("scenario")==fault_ids[i]&&report.at("actual_threw")== (i!=3)&&report.at("expected_fault")== (i!=3)&&report.at("requested")==1000&&report.at("warmup")==256&&report.at("measurement_attempts")==744&&report.at("cadence_ms")==20&&audit.at("requested")==1000&&audit.at("produced")==report.at("produced")&&audit.at("not_produced")==report.at("not_produced")&&audit.at("produced").get<std::uint64_t>()+audit.at("not_produced").get<std::uint64_t>()==1000,"original four literal fault controls");
                w::require(producer.spec.at("argv")==J::array({fault_ids[i],raw_root})&&auditor.spec.at("argv")==J::array({raw_root,row.at("report").at("sha256"),base_commit,manual_sha_,row.at("audit").at("path")}),"original literal fault producer/auditor commands");
                if(i==0)w::require(report.at("produced")==256&&report.at("actual_error")=="endpoint_free_capture_fault_at_attempt_257","original capture fault");
                if(i==2)w::require(report.at("down_attempts")==1&&audit.at("receipt_down")==1&&audit.at("unknown_injection_results")==1&&report.at("release_calls").get<std::uint64_t>()==report.at("releases_before_rpc").get<std::uint64_t>()+1,"original unknown Down once and release");
            }
            else{
                const auto& p=plan.at("plans")[i];const auto n=i<3?2560:i<5?10000:1000,cadence=i>=3&&i<5?8:20;
                const auto scene=std::array<const char*,9>{"tap","hold","dense","tap","dense","tap","hold","tap","hold"}[i];w::require(p.at("scene")==scene&&report.at("scene")==scene&&producer.spec.at("argv")==J::array({scene,std::to_string(n),std::to_string(cadence),raw_root})&&auditor.spec.at("argv")==J::array({"--root",raw_root,"--expected-report-sha256",row.at("report").at("sha256"),"--source-commit",base_commit,"--output",row.at("audit").at("path")}),"original literal production commands/scenes");
                w::require(p.at("id")==ids[i]&&p.at("mode")==mode&&p.at("n")==n&&p.at("cadence")==cadence&&report.at("schema")=="pas.windows-production-pipeline-cold.v1"&&report.at("requested")==n&&report.at("warmup")==256&&report.at("measurement_attempts")==n-256&&report.at("cadence_ms")==cadence&&report.at("production_packet_action_wake_archive_executed").get<bool>()&&!report.at("real_capture_rpc_measured").get<bool>(),"unchanged nine production workloads");
                w::require(audit.at("schema")=="pas.windows-production-pipeline-audit.v5"&&audit.at("integrity_verified").get<bool>()&&audit.at("source_commit")==base_commit&&same_pin(audit.at("report"),row.at("report"))&&audit.at("normal_cost_valid")==row.at("normal_cost_valid"),"preserved full raw production audit");
                if(i<3)w::require(audit.at("normal_cost_valid").get<bool>()&&audit.at("normal_cost_failures").empty(),"three original normal production numerical gates");
            }++i;
        }
        checks_[faults?"faults":"production"]={{"pass",true},{"rows",i},{"raw_revalidation_performed",false}};
    }
    void build(){
        delta_=object(required("narrow_build"));w::require(delta_.at("schema")=="pas.narrow-noise-repair-build.v1"&&delta_.at("configuration")=="release"&&delta_.at("scope")=="noise_and_unrun_abba_only"&&!delta_.at("whole_candidate_qualification").get<bool>()&&!delta_.at("live_ready").get<bool>()&&delta_.at("device_operations")==0&&same_pin(delta_.at("original_main32_inventory"),required("original_inventory")),"declared endpoint-free narrow build scope");
        execution_=delta_.at("source_commit").get<std::string>();compiled_=delta_.value("compiled_source_commit",execution_);w::require(w::hex(execution_,40)&&w::hex(compiled_,40),"exact execution/compiled commit identities");
        const auto base=object(delta_.at("base_release_build"));w::require(base.at("source_commit")==base_commit,"original Release build identity");for(const auto& row:inventory_.at("cold"))if(row.at("mode")=="release")w::require(same_pin(row.at("build"),delta_.at("base_release_build"))&&same_pin(row.at("evaluation"),delta_.at("base_release_evaluation")),"original Release build/evaluation inventory join");
        source_=object(delta_.at("source_manifest"));w::require(source_.at("schema")=="pas.cooperative-source.v1"&&source_.at("source_commit")==execution_&&!source_.at("dirty").get<bool>(),"execution source manifest identity");
        std::map<std::string,J> current;for(const auto& f:source_.at("files")){pin_shape(f);w::require(current.emplace(relative(f),f).second,"unique execution source member");}
        r_.entries(source_.at("files"));
        const auto plan=object(delta_.at("core_build_plan"));
        const bool component_cpu=delta_.value("component_cpu_delta",false);
        w::require(plan.at("schema")=="pas.narrow-noise-repair-core-build-plan.v1"&&plan.at("source_commit")==execution_&&plan.at("base_source_commit")==base_commit,"narrow plan execution/base source join");
        if(component_cpu)component_delta::declaration(delta_,plan,current,base_sources_);
        else{
            w::require(!plan.value("component_cpu_delta",false)&&!delta_.value("formal_rebuilt",false),"legacy unchanged-formal branch cannot claim CPU rebuild");
            for(const auto* field:{"formal_changed_paths","rebuilt_static_inputs","linked_static_inputs","component_controls"})w::require(!delta_.contains(field),"CPU-only fields cannot enter unchanged-formal branch");
            for(const auto& [path,old]:base_sources_)if(path.starts_with("src/")||path.starts_with("include/")||path=="CMakeLists.txt"){w::require(current.contains(path)&&same_pin(current.at(path),old),"formal source changed from original main32");r_.bound(current.at(path));}
        }
        const bool active_tail=component_cpu||plan.at("delta_targets")==J::array({"current_chain","consumer_priority_controls","worker_affinity_controls","producer_wait_tests"});
        const bool affinity=active_tail||plan.at("delta_targets")==J::array({"current_chain","consumer_priority_controls","worker_affinity_controls"});
        if(!component_cpu)w::require(!plan.at("formal_rebuilt").get<bool>()&&(affinity||plan.at("delta_targets")==J::array({"current_chain","consumer_priority_controls"}))&&(active_tail?plan.at("translation_units")==4:affinity?plan.at("translation_units")==3:!plan.contains("translation_units")||plan.at("translation_units")==2),"declared narrow harness targets with unchanged formal source");
        w::require(delta_.at("base_static_inputs").size()==5&&plan.at("base_static_inputs")==delta_.at("base_static_inputs"),"five exact imported static inputs");for(const auto& f:delta_.at("base_static_inputs")){w::require(base_files_.contains(key(f))&&same_pin(f,base_files_.at(key(f))),"original formal/static library binding");r_.bound(f);}
        const auto deps=object(delta_.at("dependency_manifest"));w::require(deps.at("source_commit")==base_commit&&same_pin(delta_.at("dependency_manifest"),base.at("dependency_manifest")),"original dependency manifest provenance");
        if(delta_.contains("selected_dependency_manifest")){const auto selected=object(delta_.at("selected_dependency_manifest"));w::require(selected.at("schema")=="pas.narrow-noise-repair-dependency-selection.v1"&&same_pin(selected.at("original_dependency_manifest"),delta_.at("dependency_manifest")),"actual narrow dependency selection");r_.entries(selected.at("files"));}
        r_.entries(delta_.at("additional_inputs"));const auto authorization=object(delta_.at("tests_scope_authorization"));w::require(authorization.at("schema")=="pas.user-directed-narrow-noise-repair-scope.v1"&&authorization.at("status")=="HUMAN_AUTHORIZED"&&authorization.at("scope")=="noise_and_unrun_abba_only"&&!authorization.at("full_75_rerun_required").get<bool>()&&!authorization.at("production_pass_rerun_required").get<bool>()&&authorization.at("device_operations")==0&&!authorization.at("live_ready").get<bool>(),"preserved user-directed narrow test scope");
        J compiled_manifest=source_,compiled_manifest_pin=delta_.at("source_manifest"),compiled_core_plan_pin=delta_.at("core_build_plan");
        if(delta_.value("maintenance_only_amendment",false)){
            const auto parent=object(delta_.at("parent_repair_build"));same_parent(delta_,parent);w::require(parent.at("source_commit")==compiled_&&!parent.value("maintenance_only_amendment",false)&&same_pin(delta_.at("compiled_source_manifest"),parent.at("source_manifest"))&&!delta_.at("component_rebuilt").get<bool>()&&!delta_.at("formal_rebuilt").get<bool>(),"one preserved compiled parent, no recompilation");
            const auto old=object(parent.at("source_manifest"));compiled_manifest=old;compiled_manifest_pin=parent.at("source_manifest");compiled_core_plan_pin=parent.at("core_build_plan");std::set<std::string> old_paths;for(const auto& f:old.at("files")){const auto path=relative(f);old_paths.insert(path);w::require(current.contains(path)&&(maintenance_path(path)||same_pin(current.at(path),f)),"nonmaintenance compiled source changed");}
            for(const auto& [path,pin]:current){(void)pin;w::require(old_paths.contains(path)||maintenance_path(path),"new nonmaintenance source in preserved compiled execution");}
            auto parent_plan=object(parent.at("core_build_plan")),compiled_plan=plan;for(const auto* k:{"source_commit","compiled_source_commit","component_rebuilt","formal_rebuilt"}){parent_plan.erase(k);compiled_plan.erase(k);}w::require(parent_plan==compiled_plan,"unchanged parent targets/static/compiler provenance");
            const auto proof=object(delta_.at("amendment_proof"));w::require(proof.at("schema")=="pas.narrow-noise-maintenance-amendment.v1"&&proof.at("source_commit")==execution_&&proof.at("compiled_source_commit")==compiled_&&proof.at("before_verified").get<bool>()&&proof.at("after_verified").get<bool>()&&proof.at("native_operations")==0&&!proof.at("controls_rerun").get<bool>()&&same_pin(proof.at("parent_repair_build"),delta_.at("parent_repair_build"))&&same_pin(proof.at("source_manifest"),delta_.at("source_manifest"))&&same_pin(proof.at("core_build_plan"),delta_.at("core_build_plan")),"maintenance-only preservation proof");
        }else w::require(execution_==compiled_,"unamended execution/compiled identity");
        r_.entries(delta_.at("files"));r_.entries(delta_.at("compiled_outputs"));
        std::vector<Native> build_processes;for(const auto& pin:delta_.at("build_receipts"))build_processes.push_back(by_verification(pin));const auto configure_process=by_verification(delta_.at("configure_receipt"));const auto control=by_verification(delta_.at("controls_receipt"));
        const auto report=object(delta_.at("controls_report"));w::require(report.at("schema")=="pas.consumer-priority-tests.v1"&&report.at("assertions")==36&&report.at("checks").size()==36&&report.at("failed")==0&&report.at("pass").get<bool>()&&report.at("device_endpoints")==0&&report.at("device_operations")==0&&!report.at("live_ready").get<bool>()&&!report.at("cost_qualification").get<bool>(),"preserved original controls36");for(const auto& c:report.at("checks"))w::require(c.at("pass").get<bool>(),"consumer priority control failed");
        w::require(std::find(control.spec.at("argv").begin(),control.spec.at("argv").end(),delta_.at("controls_report").at("path"))!=control.spec.at("argv").end(),"native controls output join");
        std::map<std::string,J> compiled_sources;for(const auto& pin:compiled_manifest.at("files"))w::require(compiled_sources.emplace(relative(pin),pin).second,"unique actual compiled source pins");
        const auto& priority_header=compiled_sources.at("research/prelive_current/consumer_priority.hpp");const auto& priority_tests=compiled_sources.at("research/prelive_current/tests/consumer_priority_tests.cpp");const auto& priority_chain=compiled_sources.at("research/prelive_current/chain.cpp");
        consumer_priority_requested_=priority_policy(plan,report,priority_header,priority_tests);
        const auto source_join=[&](const Native& p){for(const auto& pin:std::array<J,4>{compiled_manifest_pin,priority_header,priority_tests,priority_chain})w::require(p.inputs.contains(key(pin))&&same_pin(p.inputs.at(key(pin)),pin),"actual compiled manifest/chain/priority header/tests in native input closure");};
        for(const auto& p:build_processes)source_join(p);source_join(control);
        bool controls_binary=false;for(const auto& pin:delta_.at("files"))if(fs::path(pin.at("path").get<std::string>()).filename()=="consumer_priority_controls.exe")controls_binary|=same_path_text(control.spec.at("exe").get<std::string>(),pin.at("path").get<std::string>());w::require(controls_binary,"actual controls image in sealed narrow runtime files");
        if(affinity){const auto process=by_verification(delta_.at("affinity_controls_receipt"));const auto affinity_report=object(delta_.at("affinity_controls_report"));w::require(affinity_report.at("schema")=="pas.worker-affinity-tests.v1"&&affinity_report.at("assertions")==65&&affinity_report.at("checks").size()==65&&affinity_report.at("failed")==0&&affinity_report.at("pass").get<bool>()&&affinity_report.at("device_operations")==0&&affinity_report.at("device_endpoints")==0&&!affinity_report.at("cost_qualification").get<bool>()&&!affinity_report.at("live_ready").get<bool>()&&process.spec.at("argv")==J::array({delta_.at("affinity_controls_report").at("path")}),"existing worker-affinity controls65/native join");for(const auto& c:affinity_report.at("checks"))w::require(c.at("pass").get<bool>(),"worker-affinity control failed");}
        if(active_tail){
            const auto process=by_verification(delta_.at("wait_controls_receipt"));const auto wait_report=object(delta_.at("wait_controls_report"));
            w::require(plan.at("producer_wait_controls_count")==43&&wait_report.at("schema")=="pas.producer-deadline-wait-tests.v1"&&wait_report.at("assertions")==43&&wait_report.at("checks").size()==43&&wait_report.at("failed")==0&&wait_report.at("pass").get<bool>()&&wait_report.at("device_endpoints")==0&&wait_report.at("device_operations")==0&&!wait_report.at("cost_qualification").get<bool>()&&!wait_report.at("live_ready").get<bool>()&&wait_report.at("active_tail_poll_cap")==16'777'216&&process.spec.at("argv")==J::array({delta_.at("wait_controls_report").at("path")}),"all 43 original/active-tail producer deadline controls/native join");
            const auto& wait_header=compiled_sources.at("research/prelive_current/producer_wait.hpp");const auto& wait_tests=compiled_sources.at("research/prelive_current/tests/producer_wait_tests.cpp");producer_active_tail_ns_=active_tail_policy(plan,wait_report,wait_header,wait_tests);
            const auto wait_source_join=[&](const Native& p){for(const auto& pin:std::array<J,5>{compiled_manifest_pin,compiled_core_plan_pin,priority_chain,wait_header,wait_tests})w::require(p.inputs.contains(key(pin))&&same_pin(p.inputs.at(key(pin)),pin),"actual compiled manifest/coreplan/chain/producer wait header/tests in native input closure");};
            for(const auto& p:build_processes)wait_source_join(p);wait_source_join(process);
            bool wait_binary=false;for(const auto& pin:delta_.at("files"))if(fs::path(pin.at("path").get<std::string>()).filename()=="producer_wait_tests.exe")wait_binary|=same_path_text(process.spec.at("exe").get<std::string>(),pin.at("path").get<std::string>());w::require(wait_binary,"actual producer wait controls image in sealed narrow runtime files");
            for(const auto& c:wait_report.at("checks"))w::require(c.at("pass").get<bool>(),"producer deadline control failed");
        }
        if(component_cpu){
            component_delta::libraries(delta_,plan,base_files_);r_.entries(delta_.at("linked_static_inputs"));r_.entries(delta_.at("rebuilt_static_inputs"));
            const auto find_output=[&](const fs::path& expected)->J{const J* found=nullptr;for(const auto& pin:delta_.at("compiled_outputs"))if(same_path_text(pin.at("path").get<std::string>(),expected)){w::require(!found,"duplicate CPU graph/product output");found=&pin;}w::require(found,"missing sealed CPU graph/product output");return *found;};
            const auto ninja=find_output(fs::path(delta_.at("build_root").get<std::string>())/"build.ninja"),cache=find_output(fs::path(delta_.at("build_root").get<std::string>())/"CMakeCache.txt");
            const auto ninja_path=r_.bound(ninja,4*w::MiB);std::ifstream ninja_file(ninja_path,std::ios::binary);std::ostringstream ninja_text;ninja_text<<ninja_file.rdbuf();w::require(bool(ninja_file)&&ninja_text.str().size()==ninja.at("bytes"),"sealed bounded Ninja graph read");
            const auto graph=component_delta::graph(ninja_text.str(),root_,delta_);component_delta::objects(delta_,graph);
            const auto require_input=[&](const Native& process,const J& pin){w::require(process.inputs.contains(key(pin))&&same_pin(process.inputs.at(key(pin)),pin),"exact fresh CPU source/plan/library/object native input-closure join");};
            const auto source_plan_join=[&](const Native& process){require_input(process,compiled_manifest_pin);require_input(process,compiled_core_plan_pin);for(const auto& [path,pin]:compiled_sources){(void)path;require_input(process,pin);}};
            w::require(build_processes.size()==1&&delta_.at("build_input_closures").size()==1&&same_pin(configure_process.evidence.at("input_closure"),delta_.at("configure_input_closure"))&&same_pin(build_processes[0].evidence.at("input_closure"),delta_.at("build_input_closures")[0]),"exact CPU configure/build native closure projections");
            source_plan_join(configure_process);source_plan_join(build_processes[0]);require_input(build_processes[0],ninja);require_input(build_processes[0],cache);
            w::require(delta_.at("component_controls").is_array()&&delta_.at("component_controls").size()==4,"all four CPU semantic control native receipts");std::set<std::string> control_targets;
            for(const auto& row:delta_.at("component_controls")){
                w::require(row.is_object()&&row.size()==7,"exact CPU control receipt projection");const auto target=row.at("target").get<std::string>();w::require(control_targets.insert(target).second,"duplicate CPU control role");
                const auto component_report=object(row.at("report"));component_delta::control_report(row,component_report);r_.bound(row.at("binary"));const auto process=by_verification(row.at("receipt"));
                w::require(same_path_text(row.at("binary").at("path").get<std::string>(),fs::path(delta_.at("build_root").get<std::string>())/(target+".exe"))&&same_pin(row.at("binary"),find_output(fs::path(row.at("binary").at("path").get<std::string>())))&&
                    process.spec.at("argv")==J::array({row.at("report").at("path")})&&same_path_text(process.spec.at("exe").get<std::string>(),row.at("binary").at("path").get<std::string>())&&same_pin(process.evidence.at("input_closure"),row.at("input_closure")),"exact CPU control image/report/command/closure join");
                bool runtime=false;for(const auto& pin:delta_.at("files"))runtime|=same_pin(pin,row.at("binary"));w::require(runtime,"CPU control executable is sealed runtime output");
                source_plan_join(process);for(const auto& pin:delta_.at("linked_static_inputs"))require_input(process,pin);for(const auto& pin:delta_.at("compiled_outputs"))require_input(process,pin);
            }
            for(const auto& row:component_delta::control_declarations())w::require(control_targets.contains(row.at("target").get<std::string>()),"missing required CPU semantic control role");
            for(const auto& process:std::array<Native,3>{control,by_verification(delta_.at("affinity_controls_receipt")),by_verification(delta_.at("wait_controls_receipt"))}){source_plan_join(process);for(const auto& pin:delta_.at("linked_static_inputs"))require_input(process,pin);}
        }
        for(const auto& p:delta_.at("old_configuration_proofs"))if(p.contains("archive_proof")){
            const auto proof=object(p.at("archive_proof"));w::require(proof.at("schema")=="pas.recoverable-main32-compiler-archive.v1"&&proof.at("source_commit")==base_commit&&proof.at("all_entry_SHAs_readback_verified").get<bool>()&&proof.at("originals_rechecked").get<bool>()&&same_pin(proof.at("build_binding"),p.at("build"))&&same_pin(proof.at("archive"),p.at("archive")),"preserved compiler ZIP proof");pin_shape(p.at("archive"));const fs::path archive=p.at("archive").at("path").get<std::string>();w::require(fs::is_regular_file(archive)&&fs::file_size(archive)==p.at("archive").at("bytes"),"compiler ZIP preservation existence/length");
        }
        provenance_={{"base_formal_source_commit",base_commit},{"compiled_harness_source_commit",compiled_},{"execution_source_commit",execution_},{"consumer_priority_requested",consumer_priority_requested_},{"producer_active_tail_ns",producer_active_tail_ns_},{"formal_rebuilt",component_cpu},{"component_cpu_delta",component_cpu},{"fresh_translation_units",component_cpu?32:active_tail?4:affinity?3:2},{"rebuilt_static_targets",component_cpu?plan.at("rebuilt_static_targets"):J::array()},{"formal_changed_paths",component_cpu?delta_.at("formal_changed_paths"):J::array()},{"historical75_and_production9_prerequisite_only",true},{"component_rebuilt",!delta_.value("maintenance_only_amendment",false)},{"old_controls_prerequisite_only",delta_.value("maintenance_only_amendment",false)},{"parent_repair_build",delta_.value("parent_repair_build",J(nullptr))},{"original_inventory",required("original_inventory")},{"narrow_build",required("narrow_build")}};
    }
    CostCampaign campaign(const J& pin,bool abba){
        CostCampaign c{pin,object(pin),{}, {}};const auto& v=c.value;
        w::require(v.at("schema")=="pas.current-rails-campaign-freeze.v2"&&v.at("source_commit")==execution_&&v.at("configuration")=="Release"&&v.at("platform")=="windows-x64"&&!v.at("id").get<std::string>().empty()&&v.at("requested")==2560&&v.at("warmup")==256&&v.at("measurement")==2304&&v.at("cadence_ms")==20&&v.at("protocol_revision")=="R2"&&!v.at("protocol_modified").get<bool>()&&!v.at("old_results_reused_as_new_runs").get<bool>()&&!v.at("old_STOP_resumed").get<bool>()&&v.at("scenes")==J::array({"tap-closed-reset-v1","hold-closed-reset-v2","dense"})&&v.at("device_operations")==0&&!v.at("live_ready").get<bool>(),"fresh declared unchanged R2 campaign");
        for(const auto& b:v.at("bindings")){r_.bound(b);w::require(c.bindings.emplace(b.at("role").get<std::string>(),b).second,"unique fresh campaign binding role");}
        for(const auto* role:{"candidate_binary","analyzer_binary","auditor_binary","profile","environment","source_manifest","cost_protocol","process_guard","base_functional_prefix","narrow_noise_repair_build"})w::require(c.bindings.contains(role),("missing narrow campaign role: "+std::string(role)).c_str());
        w::require(c.bindings.at("cost_protocol").at("sha256")=="0000ad66d05649ebc4f60570b58e9a9a4cb58f7e6629279e4d38a945c8d4c060"&&same_pin(c.bindings.at("base_functional_prefix"),inventory_.at("prefix_receipt"))&&same_pin(c.bindings.at("narrow_noise_repair_build"),required("narrow_build")),"original R2/base prefix/narrow provenance roles");
        bool binary=false;for(const auto& f:delta_.at("files"))binary|=fs::path(f.at("path").get<std::string>()).filename()=="current_chain.exe"&&same_pin(f,c.bindings.at("candidate_binary"));w::require(binary,"campaign uses actual sealed delta binary");
        const auto source=object(c.bindings.at("source_manifest"));w::require(source.at("schema")=="pas.current-source-closure.v1"&&source.at("source_commit")==execution_&&!source.at("dirty").get<bool>()&&same_pin(source.at("original_build_source_manifest"),delta_.at("source_manifest")),"campaign execution manifest/delta join");
        r_.entries(source.at("files"));(void)pas::cost_provenance::effective_profile(c.bindings.at("profile"));
        const auto& plans=v.at("planned_runs");w::require(plans.size()==(abba?36:12),"entire declared R2 plan");if(abba)w::require(v.at("comparison_kind")=="ABBA","fixed ABBA comparison kind");std::set<std::string> ids,paths;
        for(std::size_t i=0;i<plans.size();++i){const auto& p=plans[i];w::require(p.at("scene")==v.at("scenes")[i/(abba?12:4)]&&p.at("mode")== (abba?std::array<const char*,4>{"A","B","B","A"}[i%4]:"A")&&ids.insert(p.at("run_id").get<std::string>()).second&&paths.insert(p.at("report_path").get<std::string>()).second,"fixed distinct planned run order");if(abba)w::require(p.at("batch")==i%12/4+1,"three fixed ABBA batches");}return c;
    }
    bool runs(CostCampaign& c,const J& rows){
        bool valid=true;
        w::require(rows.size()==c.value.at("planned_runs").size(),"all planned runs present");std::size_t i=0;
        for(const auto& e:rows){const auto& plan=c.value.at("planned_runs")[i++];auto report=object(e,w::MiB);const auto& prov=report.at("provenance");
            verify_priority(report.at("consumer_priority"),consumer_priority_requested_);
            if(producer_active_tail_ns_)verify_active_tail(report,producer_active_tail_ns_);
            w::require(e.at("run_id")==plan.at("run_id")&&e.at("scene")==plan.at("scene")&&report.at("mode")==plan.at("mode")&&report.at("scene")==plan.at("scene")&&same_path_text(e.at("path").get<std::string>(),plan.at("report_path").get<std::string>()),"run/report plan join");
            const auto producer=native(e.at("process_evidence"));w::require(same_path_text(producer.spec.at("exe").get<std::string>(),c.bindings.at("candidate_binary").at("path").get<std::string>())&&producer.spec.at("argv")==J::array({"pipeline",plan.at("mode"),plan.at("scene"),"2560",c.pin.at("path"),plan.at("run_id"),e.at("path")}),"actual frozen producer command");
            w::require(prov.at("schema")=="pas.current-rails-run-provenance.v1"&&prov.at("campaign_sha256")==c.pin.at("sha256")&&prov.at("campaign_id")==c.value.at("id")&&prov.at("source_commit")==execution_&&prov.at("run_id")==plan.at("run_id")&&prov.at("scene")==plan.at("scene")&&prov.at("mode")==plan.at("mode")&&prov.at("requested")==2560&&same_path_text(prov.at("report_path").get<std::string>(),e.at("path").get<std::string>())&&prov.at("process").at("pid")==producer.facts.at("pid")&&prov.at("process").at("creation_filetime")==producer.facts.at("creation_filetime"),"fresh source/campaign/process identity");
            for(const auto& [role,b]:c.bindings){(void)role;w::require(producer.inputs.contains(key(b))&&same_pin(producer.inputs.at(key(b)),b),"every campaign member in producer closure");}
            const auto source=object(c.bindings.at("source_manifest"));for(const auto& f:source.at("files"))w::require(producer.inputs.contains(key(f))&&same_pin(producer.inputs.at(key(f)),f),"every execution source member in producer closure");
            w::require(prov.at("effective_profile")==pas::cost_provenance::effective_profile(c.bindings.at("profile")),"actual effective profile join");
            for(const auto* role:{"candidate_binary","source_manifest","profile","environment","cost_protocol"})w::require(prov.at(std::string(role)+"_sha256")==c.bindings.at(role).at("sha256"),"fresh run bound member SHA");
            const fs::path audit_path=e.at("audit_path").get<std::string>();const J apin={{"path",audit_path.string()},{"bytes",fs::file_size(audit_path)},{"sha256",e.at("audit_sha256")}};const auto audit=object(apin,w::MiB);
            w::require(audit.at("schema")=="pas.prelive-independent-integrity.v2"&&audit.at("integrity").get<bool>()&&audit.at("complete_prefix_evidence").get<bool>()&&audit.at("all_input_processed").get<bool>()&&audit.at("metrics_verified").get<bool>()&&audit.at("plan_join_complete").get<bool>()&&audit.at("coordinate_join_complete").get<bool>()&&audit.at("provenance_join_complete").get<bool>()&&!audit.at("release_uncertainty_seen").get<bool>()&&audit.at("run_provenance")==prov&&audit.at("verifier_binary").at("sha256")==c.bindings.at("auditor_binary").at("sha256")&&audit.at("errors").empty(),"independent full-attempt raw audit receipt");
            if(plan.at("scene")!="dense")w::require(audit.at("producer_metrics_verified").get<bool>(),"all attempted producer timing verified");
            const auto auditor=output_process(apin,c.bindings.at("auditor_binary"));w::require(auditor.spec.at("argv")==J::array({e.at("path"),apin.at("path")}),"actual independent audit command");
            const auto& raw=audit.at("input_bindings");for(const auto& [name,suffix]:std::array<std::pair<const char*,const char*>,3>{{{"report",""},{"attempts",".attempts.json"},{"journal",".journal.jsonl"}}}){const auto& pin=raw.at(name);w::require(same_path_text(pin.at("path").get<std::string>(),e.at("path").get<std::string>()+suffix)&&auditor.inputs.contains(key(pin))&&same_pin(auditor.inputs.at(key(pin)),pin),"all original raw bindings sealed in auditor closure");}
            const auto unique=[&](const std::string& id){w::require(cost_identities_.insert(id).second,"reused cost run/process/raw identity");};unique("report:"+e.at("sha256").get<std::string>());unique("path:"+key(e));unique("process:"+prov.at("process").at("pid").dump()+":"+prov.at("process").at("creation_filetime").dump());unique("attempts:"+raw.at("attempts").at("sha256").get<std::string>());unique("journal:"+raw.at("journal").at("sha256").get<std::string>());
            report["_audited_measurement"]={{"phase_counts",audit.at("measurement_phase_counts")},{"lateness",audit.at("measurement_all_phase_lateness_ms")}};if(!::validity(report).at("pass").get<bool>()){valid=false;blocked("cost_validity","original R2 validity NOT_READY: "+plan.at("run_id").get<std::string>());}c.reports.push_back(std::move(report));
        }return valid;
    }
    void aa(){
        if(!supplied(input_,{"aa_status","aa_input"})){checks_["aa"]={{"pass",false},{"state","NOT_RUN"}};blocked("aa","fresh AA status/input not supplied / NOT_RUN");return;}
        const auto status=object(required("aa_status")),input=object(required("aa_input"));w::require(status.at("schema")=="pas.windows-aa-batch-status.v1"&&input.at("schema")=="pas.current-rails-cost-input.v2"&&same_pin(status.at("campaign"),input.at("campaign")),"AA status/input campaign join");
        if(status.at("normal_stopped").get<bool>()||!status.at("all_12_cost_validity_pass").get<bool>()||status.at("runs").size()!=12||!status.at("remaining").empty()){blocked("aa","AA12 incomplete or NOT_READY; original status retained");return;}
        w::require(status.at("runs")==input.at("runs")&&!input.at("normal_stopped").get<bool>(),"complete identical AA run catalogue");aa_=campaign(input.at("campaign"),false);aa_ok_=runs(aa_,input.at("runs"));checks_["aa"]={{"pass",aa_ok_},{"runs",12}};
    }
    void noise(){
        if(!aa_ok_){blocked("noise","AA12 not accepted; noise unavailable");return;}if(!supplied(input_,{"noise","noise_process"})){blocked("noise","fresh noise report/native receipts not supplied / NOT_RUN");return;}const auto frozen=object(required("noise"));bool adequate=true;
        for(std::size_t load=0;load<3;++load){J metrics=J::object();bool load_ok=true;for(const auto* metric:{"observer_ms","owner_ms","capture_to_owner_ms"})for(const auto* q:{"p95","p99"}){
            std::array<double,4> values{};for(std::size_t i=0;i<4;++i)values[i]=aa_.reports[load*4+i].at("measurement").at(metric).at(q);const auto n=cc::noise(values,metric);load_ok&=n.adequate;metrics[std::string(metric)+"."+q]={{"values",values},{"d",n.d},{"T",n.t},{"adequate",n.adequate}};
        }adequate&=load_ok;noise_[aa_.value.at("scenes")[load].get<std::string>()]={{"adequate",load_ok},{"metrics",metrics}};}
        const auto aai=object(required("aa_input"));J normal_rows=J::array();for(std::size_t i=0;i<12;++i){const auto& e=aai.at("runs")[i];normal_rows.push_back({{"run_id",e.at("run_id")},{"path",e.at("path")},{"sha256",e.at("sha256")},{"validity",::validity(aa_.reports[i])}});}
        w::require(frozen.at("schema")=="pas.current-rails-noise-freeze.v2"&&frozen.at("campaign_id")==aa_.value.at("id")&&frozen.at("campaign_sha256")==aa_.pin.at("sha256")&&frozen.at("normal_runs")==normal_rows&&frozen.at("noise")==noise_&&frozen.at("ready")==adequate&&frozen.at("comparison_allowed")==adequate&&frozen.at("candidate_results_used")==0&&!frozen.at("live_ready").get<bool>(),"independent original 18-row R2 noise recomputation");
        const auto process=native(required("noise_process"));w::require(process.spec.at("argv")==J::array({"aa",required("aa_input").at("path"),required("noise").at("path")})&&same_path_text(process.spec.at("exe").get<std::string>(),aa_.bindings.at("analyzer_binary").at("path").get<std::string>()),"actual original noise analyzer command");
        noise_ok_=adequate;checks_["noise"]={{"pass",adequate},{"rows",18},{"recomputed",noise_}};if(!adequate)blocked("noise","original R2 noise inadequate; ABBA NOT_RUN");
    }
    void abba(){
        if(!noise_ok_){checks_["abba"]={{"pass",false},{"state","NOT_RUN"}};blocked("abba","adequate AA/noise prerequisite not met");return;}
        if(!supplied(input_,{"abba_status","abba_input","comparison","comparison_process"})){checks_["abba"]={{"pass",false},{"state","NOT_RUN"}};blocked("abba","ABBA existing receipts not supplied / NOT_RUN");return;}
        const auto status=object(required("abba_status")),input=object(required("abba_input"));w::require(status.at("schema")=="pas.pre-emulator-abba-status.v1"&&input.at("schema")=="pas.windows-cost-comparison-input.v1"&&same_pin(input.at("aa_input"),required("aa_input"))&&same_pin(input.at("noise"),required("noise")),"ABBA same AA/noise input chain");
        if(status.at("stopped").get<bool>()||status.at("runs").size()!=36||!status.at("remaining").empty()){blocked("abba","ABBA36 incomplete or stopped");return;}w::require(status.at("runs")==input.at("runs")&&!input.at("normal_stopped").get<bool>()&&same_pin(status.at("campaign"),input.at("campaign")),"all identical ABBA planned runs");
        abba_=campaign(input.at("campaign"),true);w::require(aa_.value.at("id")!=abba_.value.at("id")&&same_pin(abba_.bindings.at("aa_input"),required("aa_input"))&&same_pin(abba_.bindings.at("aa_noise"),required("noise"))&&same_pin(abba_.bindings.at("aa_campaign"),aa_.pin),"fresh distinct ABBA bound to original AA/noise");for(const auto& [role,b]:aa_.bindings)if(role!="environment"&&role!="maintenance_recipe")w::require(same_pin(b,abba_.bindings.at(role)),"ABBA preserves AA frozen component bindings");
        const auto ae=object(aa_.bindings.at("environment")),be=object(abba_.bindings.at("environment"));for(const auto* k:{"platform","os","processor","memory_bytes"})w::require(ae.at(k)==be.at(k),"same ABBA cost machine");if(!runs(abba_,input.at("runs")))return;J batches;try{batches=cost_batches(abba_,noise_);}catch(const std::exception& e){blocked("abba",std::string("original ABBA numerical predicate NOT_READY: ")+e.what());return;}const auto comparison=object(required("comparison"));
        J normal_rows=J::array();for(std::size_t i=0;i<36;++i)normal_rows.push_back({{"run_id",input.at("runs")[i].at("run_id")},{"validity",::validity(abba_.reports[i])}});
        if(!comparison.at("ready").get<bool>()||!comparison.at("all_36_valid").get<bool>()||!comparison.at("non_regression_pass").get<bool>()){blocked("abba","existing ABBA comparison NOT_READY / original numerical predicate failed");return;}
        w::require(comparison.at("schema")=="pas.windows-cost-comparison.v1"&&comparison.at("source_commit")==execution_&&comparison.at("campaign_id")==abba_.value.at("id")&&comparison.at("normal_runs")==normal_rows&&comparison.at("campaign_sha256")==abba_.pin.at("sha256")&&same_pin(comparison.at("noise_binding"),required("noise"))&&comparison.at("batches")==batches&&!comparison.at("live_ready").get<bool>()&&comparison.at("device_operations")==0,"original full ABBA comparison and numerical predicates");
        const auto process=native(required("comparison_process"));w::require(same_path_text(process.spec.at("exe").get<std::string>(),abba_.bindings.at("comparison_analyzer_binary").at("path").get<std::string>())&&process.spec.at("argv")==J::array({"--operation","compare","--input",required("abba_input").at("path"),"--expected-sha256",required("abba_input").at("sha256"),"--source-commit",execution_,"--analyzer",abba_.bindings.at("analyzer_binary").at("path"),"--output",required("comparison").at("path")}),"actual frozen comparison command");checks_["abba"]={{"pass",true},{"runs",36},{"batches",batches}};
    }
public:
    Preflight(Reader& reader,const fs::path& root,J input):r_(reader),root_(final_path(root)),input_(std::move(input)){}
    J run(){
        section("input",[&]{w::require(input_.at("schema")=="pas.readonly-narrow-preflight-input.v1"&&input_.at("device_operations")==0,"read-only narrow input scope");r_.gate(required("gate"));for(const auto& p:required("processes"))w::require(processes_.emplace(key(p.at("verification")),p).second,"unique native ProcessEvidence catalogue");});
        section("base",[&]{load_base();});if(!inventory_.is_null()&&!base_sources_.empty()){section("prefix",[&]{prefix();});section("faults",[&]{production(true);});section("production",[&]{production(false);});section("narrow_build",[&]{build();});}
        if(!execution_.empty()&&!delta_.is_null()){section("aa",[&]{aa();});section("noise",[&]{noise();});section("abba",[&]{abba();});}else blocked("cost","narrow build provenance unavailable");
        section("final_bytes",[&]{r_.finish();});
        return {{"schema","pas.readonly-narrow-preflight-check.v1"},{"status",rejected_?"REJECTED":ready_?"COLD_BUNDLE_PASS":"NOT_READY"},{"pass",ready_&&!rejected_},{"checks",checks_},{"issues",issues_},{"provenance",provenance_},{"files",r_.count()},{"bytes",r_.bytes()},{"qualification_issued",false},{"live_ready",false},{"device_operations",0},{"raw_revalidation_performed",false},{"transitive_historical_inputs_revalidated",false},{"archive_ZIP_rehashed",false},{"scope","hash-bound existing metadata/report/native seals and literal R2 arithmetic; preserved old scopes are prerequisites, no removed graph/raw revalidation and no admission"}};
    }
};
}
