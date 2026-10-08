#pragma once
#include "graph_reader.hpp"
#define main pas_original_cost_analysis_entry
#include "research/prelive_current/cost_analysis.cpp"
#undef main
#include "research/windows_cost_comparison/metrics.hpp"

// Read-only prototype. Reuses the existing R2 run predicates and noise/batch
// arithmetic; does not create a live token, execute a process or touch a device.
namespace pas::preflight_graph {
namespace cc=pas::windows_cost_comparison;
struct CostCampaign {
    J pin,value;
    std::map<std::string,J> bindings;
    std::vector<J> reports;
};
inline CostCampaign cost_campaign(Reader& r,const J& pin,const std::string& commit,bool abba) {
    CostCampaign c{pin,r.object(pin),{}, {}};
    const auto& v=c.value;
    w::require(v.at("schema")=="pas.current-rails-campaign-freeze.v2" &&
        v.at("source_commit")==commit && v.at("configuration")=="Release" &&
        v.at("platform")=="windows-x64" && !v.at("id").get<std::string>().empty() &&
        v.at("requested")==2560 && v.at("warmup")==256 && v.at("measurement")==2304 &&
        v.at("cadence_ms")==20 && v.at("protocol_revision")=="R2" &&
        !v.at("protocol_modified").get<bool>() && !v.at("old_STOP_resumed").get<bool>() &&
        !v.at("old_results_reused_as_new_runs").get<bool>() && !v.at("live_ready").get<bool>() &&
        v.at("device_operations")==0,"same-source unchanged R2 campaign");
    w::require(v.at("scenes")==J::array({"tap-closed-reset-v1","hold-closed-reset-v2","dense"}),
        "three predeclared R2 workloads");
    const auto& bs=v.at("bindings");
    w::require(bs.is_array()&&!bs.empty()&&bs.size()<=32,"finite campaign bindings");
    for(const auto& b:bs){r.bound(b);w::require(c.bindings.emplace(b.at("role").get<std::string>(),b).second,"unique campaign role");}
    for(const auto* key:{"candidate_binary","analyzer_binary","auditor_binary","profile","cost_protocol",
        "environment","source_manifest","process_guard","current_candidate_prefix"})
        w::require(c.bindings.contains(key),"complete campaign identity");
    w::require(c.bindings.at("cost_protocol").at("sha256")==
        "0000ad66d05649ebc4f60570b58e9a9a4cb58f7e6629279e4d38a945c8d4c060","original R2 protocol SHA");
    const auto source=r.object(c.bindings.at("source_manifest"));
    w::require(source.at("schema")=="pas.current-source-closure.v1"&&source.at("source_commit")==commit,
        "cost source closure identity");
    r.entries(source.at("files"));
    (void)pas::cost_provenance::source_closure(c.value);
    (void)pas::cost_provenance::effective_profile(c.bindings.at("profile"));
    const auto& plans=v.at("planned_runs");
    w::require(plans.is_array()&&plans.size()==(abba?36:12),"entire predeclared cost plan");
    if(abba)w::require(v.at("comparison_kind")=="ABBA","ABBA campaign kind");
    std::set<std::string> ids,paths;
    for(std::size_t i=0;i<plans.size();++i){const auto& p=plans[i];
        w::require(p.at("scene")==v.at("scenes")[i/(abba?12:4)]&&
            p.at("mode")== (abba?std::array<const char*,4>{"A","B","B","A"}[i%4]:"A")&&
            ids.insert(p.at("run_id").get<std::string>()).second&&
            paths.insert(p.at("report_path").get<std::string>()).second,"fixed unique cost order");
        if(abba)w::require(p.at("batch")==i%12/4+1,"three fixed ABBA batches");
    }
    return c;
}
inline void cost_runs(Reader& r,CostCampaign& c,const J& runs,const J& auditor_processes,
                      std::set<std::string>& identities){
    w::require(runs.is_array()&&runs.size()==c.value.at("planned_runs").size()&&
        auditor_processes.is_array()&&auditor_processes.size()==runs.size(),"all raw cost runs and auditors");
    for(std::size_t i=0;i<runs.size();++i){const auto& e=runs[i];const auto& p=c.value.at("planned_runs")[i];
        const auto report_path=r.bound(e,w::MiB);auto report=r.object(e,w::MiB);
        w::require(e.at("run_id")==p.at("run_id")&&e.at("scene")==p.at("scene")&&
            report.at("scene")==p.at("scene")&&report.at("mode")==p.at("mode")&&
            report_path==final_path(p.at("report_path").get<std::string>()),"cost report order/identity");
        const auto& pe=e.at("process_evidence");const auto inputs=r.process(pe);
        const auto spec=r.object(pe.at("specification")),result=r.object(pe.at("result"));
        const auto& argv=spec.at("argv");const auto& prov=report.at("provenance");
        w::require(final_path(spec.at("exe").get<std::string>())==r.bound(c.bindings.at("candidate_binary"))&&
            argv.size()==7&&argv[0]=="pipeline"&&argv[1]==p.at("mode")&&argv[2]==p.at("scene")&&
            argv[3]=="2560"&&final_path(argv[4].get<std::string>())==r.bound(c.pin)&&
            argv[5]==p.at("run_id")&&final_path(argv[6].get<std::string>())==report_path,
            "direct frozen producer command");
        w::require(prov.at("schema")=="pas.current-rails-run-provenance.v1"&&
            prov.at("campaign_id")==c.value.at("id")&&prov.at("campaign_sha256")==c.pin.at("sha256")&&
            prov.at("source_commit")==c.value.at("source_commit")&&prov.at("run_id")==p.at("run_id")&&
            prov.at("scene")==p.at("scene")&&prov.at("mode")==p.at("mode")&&prov.at("requested")==2560&&
            final_path(prov.at("report_path").get<std::string>())==report_path&&
            prov.at("process").at("pid")==result.at("facts").at("pid")&&
            prov.at("process").at("creation_filetime")==result.at("facts").at("creation_filetime"),
            "report and actual producer process join");
        for(const auto& [role,b]:c.bindings){
            w::require(inputs.contains(r.bound(b)),"entire campaign frozen in producer inputs");
            if(role=="candidate_binary"||role=="source_manifest"||role=="profile"||role=="environment"||role=="cost_protocol")
                w::require(prov.at(role+"_sha256")==b.at("sha256"),"producer campaign member SHA");
        }
        const auto source=r.object(c.bindings.at("source_manifest"));
        for(const auto& file:source.at("files"))
            w::require(inputs.contains(r.bound(file)),"every source member frozen in producer inputs");
        w::require(prov.at("effective_profile")==pas::cost_provenance::effective_profile(c.bindings.at("profile")),
            "producer effective profile");
        const auto audit_path=fs::path(e.at("audit_path").get<std::string>());
        const J apin={{"path",audit_path.string()},{"bytes",fs::file_size(audit_path)},{"sha256",e.at("audit_sha256")}};
        const auto audit=r.object(apin,w::MiB);
        w::require(audit.at("schema")=="pas.prelive-independent-integrity.v2"&&audit.at("integrity").get<bool>()&&
            audit.at("complete_prefix_evidence").get<bool>()&&audit.at("all_input_processed").get<bool>()&&
            audit.at("metrics_verified").get<bool>()&&audit.at("plan_join_complete").get<bool>()&&
            audit.at("coordinate_join_complete").get<bool>()&&audit.at("provenance_join_complete").get<bool>()&&
            !audit.at("release_uncertainty_seen").get<bool>()&&audit.at("run_provenance")==prov&&
            audit.at("verifier_binary").at("sha256")==c.bindings.at("auditor_binary").at("sha256"),"complete independent raw audit");
        if(p.at("scene")!="dense")w::require(audit.at("producer_metrics_verified").get<bool>(),"all attempted producer timing verified");
        const auto auditor_inputs=r.process(auditor_processes[i]);
        const auto aspec=r.object(auditor_processes[i].at("specification"));
        w::require(final_path(aspec.at("exe").get<std::string>())==r.bound(c.bindings.at("auditor_binary"))&&
            aspec.at("argv").size()==2&&final_path(aspec.at("argv")[0].get<std::string>())==report_path&&
            final_path(aspec.at("argv")[1].get<std::string>())==r.bound(apin),"actual frozen independent auditor command");
        const auto& raw=audit.at("input_bindings");
        for(const auto& [key,suffix]:std::array<std::pair<const char*,const char*>,3>{{{"report",""},{"attempts",".attempts.json"},{"journal",".journal.jsonl"}}})
            w::require(r.bound(raw.at(key))==final_path(report_path.string()+suffix)&&
                auditor_inputs.contains(r.bound(raw.at(key))),"entire raw inputs frozen in auditor closure");
        const auto unique=[&](const std::string& id){w::require(identities.insert(id).second,"reused cost report/process/raw run");};
        unique("path:"+report_path.string());unique("report:"+e.at("sha256").get<std::string>());
        unique("process:"+prov.at("process").at("pid").dump()+":"+prov.at("process").at("creation_filetime").dump());
        unique("attempts:"+raw.at("attempts").at("sha256").get<std::string>());
        unique("journal:"+raw.at("journal").at("sha256").get<std::string>());
        report["_audited_measurement"]={{"phase_counts",audit.at("measurement_phase_counts")},
            {"lateness",audit.at("measurement_all_phase_lateness_ms")}};
        w::require(::validity(report).at("pass").get<bool>(),"original necessary R2 validity predicates");
        c.reports.push_back(std::move(report));
    }
}
inline J cost_noise(const CostCampaign& aa){
    w::require(aa.reports.size()==12,"complete AA12 for noise");J noise=J::object();
    for(std::size_t load=0;load<3;++load){J metrics=J::object();
        for(const auto* metric:{"observer_ms","owner_ms","capture_to_owner_ms"})for(const auto* q:{"p95","p99"}){
            std::array<double,4> values{};
            for(std::size_t i=0;i<4;++i)values[i]=aa.reports[load*4+i].at("measurement").at(metric).at(q);
            const auto n=cc::noise(values,metric);w::require(n.adequate,"original AA noise adequacy");
            metrics[std::string(metric)+"."+q]={{"values",values},{"d",n.d},{"T",n.t},{"adequate",true}};
        }
        noise[aa.value.at("scenes")[load].get<std::string>()]={{"adequate",true},{"metrics",metrics}};
    }
    return noise;
}
inline J cost_batches(const CostCampaign& abba,const J& noise){
    w::require(abba.reports.size()==36,"complete ABBA36");J batches=J::array();
    for(std::size_t load=0;load<3;++load)for(std::size_t batch=0;batch<3;++batch){
        const auto scene=abba.value.at("scenes")[load].get<std::string>();J checks=J::object();bool benefit=false;
        for(const auto* metric:{"observer_ms","owner_ms","capture_to_owner_ms"})for(const auto* q:{"p95","p99"}){
            std::array<double,4> values{};
            for(std::size_t i=0;i<4;++i)values[i]=abba.reports[load*12+batch*4+i].at("measurement").at(metric).at(q);
            const auto t=noise.at(scene).at("metrics").at(std::string(metric)+"."+q).at("T").get<double>();
            const auto result=cc::compare(values,t);w::require(result.non_regression,"original ABBA non-regression");
            checks[std::string(metric)+"."+q]={{"values_ABBA",values},{"A_quantile_mean",result.a},
                {"B_quantile_mean",result.b},{"B_minus_A",result.b_minus_a},{"frozen_T",result.t},
                {"non_regression",result.non_regression},{"improved_beyond_T",result.improved}};
        }
        for(const auto* metric:{"observer_ms","owner_ms"})for(const auto* q:{"p95","p99"})
            benefit|=checks.at(std::string(metric)+"."+q).at("improved_beyond_T").get<bool>()&&
                checks.at(std::string("capture_to_owner_ms.")+q).at("improved_beyond_T").get<bool>();
        batches.push_back({{"scene",scene},{"batch",batch+1},{"order","ABBA"},
            {"comparison",{{"metrics",checks},{"non_regression_pass",true},{"bottleneck_and_capture_improvement",benefit}}}});
    }
    return batches; // Benefit is reported separately; it is not a new gate.
}
inline void verify_cost(Reader& r,const J& aa_pin,const J& noise_pin,const J& abba_pin,
                       const J& comparison_pin,const std::string& commit,const J& aa_auditors,
                       const J& abba_auditors,const J& noise_process,const J& comparison_process){
    const auto aai=r.object(aa_pin),abi=r.object(abba_pin);
    w::require(aai.at("schema")=="pas.current-rails-cost-input.v2"&&!aai.at("normal_stopped").get<bool>()&&
        abi.at("schema")=="pas.windows-cost-comparison-input.v1"&&!abi.at("normal_stopped").get<bool>()&&
        abi.at("aa_input")==aa_pin&&abi.at("noise")==noise_pin,"complete same AA/comparison inputs");
    auto aa=cost_campaign(r,aai.at("campaign"),commit,false);
    auto abba=cost_campaign(r,abi.at("campaign"),commit,true);
    w::require(aa.value.at("id")!=abba.value.at("id"),"fresh distinct campaigns");
    for(const auto& [role,b]:aa.bindings)if(role!="environment"&&role!="maintenance_recipe")
        w::require(abba.bindings.at(role)==b,"comparison preserves frozen AA components");
    w::require(abba.bindings.at("aa_input").at("sha256")==aa_pin.at("sha256")&&
        abba.bindings.at("aa_noise").at("sha256")==noise_pin.at("sha256")&&
        abba.bindings.at("aa_campaign").at("sha256")==aai.at("campaign").at("sha256"),"prebound original AA/noise");
    const auto ae=r.object(aa.bindings.at("environment")),be=r.object(abba.bindings.at("environment"));
    for(const auto* key:{"platform","os","processor","memory_bytes"})
        w::require(ae.at(key)==be.at(key),"same cost machine");
    std::set<std::string> unique;
    cost_runs(r,aa,aai.at("runs"),aa_auditors,unique);
    cost_runs(r,abba,abi.at("runs"),abba_auditors,unique);
    const auto calculated=cost_noise(aa),noise=r.object(noise_pin);
    J normal_rows=J::array();
    for(std::size_t i=0;i<12;++i){const auto& e=aai.at("runs")[i];
        normal_rows.push_back({{"run_id",e.at("run_id")},{"path",e.at("path")},{"sha256",e.at("sha256")},
            {"validity",::validity(aa.reports[i])}});
    }
    w::require(noise.at("schema")=="pas.current-rails-noise-freeze.v2"&&noise.at("ready").get<bool>()&&
        noise.at("comparison_allowed").get<bool>()&&noise.at("candidate_results_used")==0&&
        noise.at("campaign_id")==aa.value.at("id")&&noise.at("campaign_sha256")==aa.pin.at("sha256")&&
        noise.at("normal_runs")==normal_rows&&noise.at("noise")==calculated&&!noise.at("live_ready").get<bool>(),
        "frozen noise equals all four A runs per load");
    const auto np=r.process(noise_process);const auto ns=r.object(noise_process.at("specification"));
    w::require(final_path(ns.at("exe").get<std::string>())==r.bound(aa.bindings.at("analyzer_binary"))&&
        ns.at("argv").size()==3&&ns.at("argv")[0]=="aa"&&
        final_path(ns.at("argv")[1].get<std::string>())==r.bound(aa_pin)&&
        final_path(ns.at("argv")[2].get<std::string>())==r.bound(noise_pin)&&np.contains(r.bound(aa_pin)),
        "actual original noise analyzer command");
    const auto batches=cost_batches(abba,calculated),comparison=r.object(comparison_pin);
    J compared_rows=J::array();
    for(std::size_t i=0;i<36;++i)compared_rows.push_back({{"run_id",abi.at("runs")[i].at("run_id")},
        {"validity",::validity(abba.reports[i])}});
    w::require(comparison.at("schema")=="pas.windows-cost-comparison.v1"&&comparison.at("ready").get<bool>()&&
        comparison.at("all_36_valid").get<bool>()&&comparison.at("non_regression_pass").get<bool>()&&
        comparison.at("source_commit")==commit&&comparison.at("campaign_id")==abba.value.at("id")&&
        comparison.at("campaign_sha256")==abba.pin.at("sha256")&&comparison.at("noise_binding")==noise_pin&&
        comparison.at("normal_runs")==compared_rows&&comparison.at("batches")==batches&&
        !comparison.at("live_ready").get<bool>()&&comparison.at("device_operations")==0,"complete original ABBA comparison");
    const auto cp=r.process(comparison_process);const auto cs=r.object(comparison_process.at("specification"));
    const auto& args=cs.at("argv");
    w::require(final_path(cs.at("exe").get<std::string>())==r.bound(abba.bindings.at("comparison_analyzer_binary"))&&
        args.size()==12&&args[0]=="--operation"&&args[1]=="compare"&&args[2]=="--input"&&
        final_path(args[3].get<std::string>())==r.bound(abba_pin)&&args[4]=="--expected-sha256"&&
        args[5]==abba_pin.at("sha256")&&args[6]=="--source-commit"&&args[7]==commit&&args[8]=="--analyzer"&&
        final_path(args[9].get<std::string>())==r.bound(abba.bindings.at("analyzer_binary"))&&args[10]=="--output"&&
        final_path(args[11].get<std::string>())==r.bound(comparison_pin)&&cp.contains(r.bound(abba_pin)),
        "actual frozen full-comparison analyzer command");
}
}

