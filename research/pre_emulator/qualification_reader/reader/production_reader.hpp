#pragma once
#include "graph_reader.hpp"
#include <array>

namespace pas::preflight_graph {
inline void frozen_raw_tree(const std::set<fs::path>& inputs,const fs::path& root){
    w::require(fs::is_directory(root),"existing cold raw directory");std::size_t files=0;
    for(const auto& e:fs::recursive_directory_iterator(root)){
        const auto attr=GetFileAttributesW(e.path().c_str());
        w::require(attr!=INVALID_FILE_ATTRIBUTES&&!(attr&FILE_ATTRIBUTE_REPARSE_POINT),"cold raw tree reparse");
        if(e.is_directory())continue;
        w::require(e.is_regular_file()&&++files<=512&&inputs.contains(final_path(e.path())),
            "every bounded original raw file frozen in auditor inputs");
    }
    w::require(files>0,"nonempty cold raw tree");
}
inline void verify_faults(Reader& r,const J& pin,const std::string& commit,const std::string& manual_sha,
                         const J& producer_binary,const J& auditor_binary,const J& auditors){
    const auto s=r.object(pin);
    w::require(s.at("schema")=="pas.literal-production-boundary-status.v1"&&s.at("source_commit")==commit&&
        !s.at("stopped").get<bool>()&&s.at("remaining").empty()&&s.at("rows").is_array()&&s.at("rows").size()==4&&
        !s.at("cost_qualification").get<bool>()&&!s.at("physical_qualification").get<bool>()&&
        !s.at("live_ready").get<bool>()&&s.at("device_operations")==0&&auditors.is_array()&&auditors.size()==4,
        "all four literal boundary controls completed");
    const auto plan=r.object(s.at("plan"));
    w::require(plan.at("schema")=="pas.literal-production-boundary-plan.v1"&&plan.at("source_commit")==commit&&
        plan.at("scenarios")==J::array({"capture","writer","rpc","slow"})&&plan.at("requested_per_scenario")==1000&&
        plan.at("warmup")==256&&plan.at("measurement")==744&&plan.at("cadence_ms")==20,"original fault plan");
    const std::array<const char*,4> names={"capture","writer","rpc","slow"};
    for(std::size_t i=0;i<4;++i){const auto& row=s.at("rows")[i];const std::string scenario=names[i];
        const auto report=r.object(row.at("report")),audit=r.object(row.at("audit"));
        const bool fault=scenario!="slow";const auto root=r.bound(row.at("report")).parent_path();
        w::require(row.at("scenario")==scenario&&report.at("scenario")==scenario&&audit.at("scenario")==scenario&&
            report.at("schema")=="pas.literal-production-boundary-controls.v1"&&report.at("requested")==1000&&
            report.at("warmup")==256&&report.at("measurement_attempts")==744&&report.at("cadence_ms")==20&&
            report.at("source_build_commit")==commit&&!report.at("source_build_dirty").get<bool>()&&
            report.at("actual_manual_translation_unit_sha256")==manual_sha&&report.at("endpoint_free").get<bool>()&&
            report.at("single_touch_owner").get<bool>()&&report.at("public_entry_rejected").get<bool>()&&
            report.at("expected_fault")==fault&&report.at("actual_threw")==fault&&
            !report.at("cost_qualification").get<bool>()&&!report.at("physical_qualification").get<bool>()&&
            !report.at("live_ready").get<bool>()&&report.at("device_operations")==0,"exact actual literal fault return");
        w::require(audit.at("schema")=="pas.literal-production-boundary-audit.v1"&&audit.at("source_commit")==commit&&
            audit.at("control_pass").get<bool>()&&audit.at("requested")==1000&&
            audit.at("produced")==report.at("produced")&&audit.at("not_produced")==report.at("not_produced")&&
            audit.at("produced").get<std::uint64_t>()+audit.at("not_produced").get<std::uint64_t>()==1000&&
            !audit.at("cost_qualification").get<bool>()&&!audit.at("physical_qualification").get<bool>()&&
            !audit.at("live_ready").get<bool>()&&audit.at("device_operations")==0,"all fault attempt slots independently audited");
        if(scenario=="capture")w::require(report.at("produced")==256&&
            report.at("actual_error")=="endpoint_free_capture_fault_at_attempt_257","exact capture fault");
        if(scenario=="rpc")w::require(report.at("down_attempts")==1&&audit.at("receipt_down")==1&&
            audit.at("unknown_injection_results")==1&&report.at("release_calls").get<std::uint64_t>()==
                report.at("releases_before_rpc").get<std::uint64_t>()+1,"unknown Down once and first release only");
        const auto producer_inputs=r.process(row.at("process"));
        const auto auditor_inputs=r.process(auditors[i]);
        const auto ps=r.object(row.at("process").at("specification")),as=r.object(auditors[i].at("specification"));
        w::require(final_path(ps.at("exe").get<std::string>())==r.bound(producer_binary)&&
            ps.at("argv").size()==2&&ps.at("argv")[0]==scenario&&
            final_path(ps.at("argv")[1].get<std::string>())==root&&producer_inputs.contains(r.bound(s.at("plan"))),
            "actual frozen fault producer command");
        const auto& args=as.at("argv");
        w::require(final_path(as.at("exe").get<std::string>())==r.bound(auditor_binary)&&
            args.size()==5&&final_path(args[0].get<std::string>())==root&&args[1]==row.at("report").at("sha256")&&
            args[2]==commit&&args[3]==manual_sha&&final_path(args[4].get<std::string>())==r.bound(row.at("audit")),
            "actual frozen fault auditor command");
        frozen_raw_tree(auditor_inputs,root);
    }
}
inline void verify_production(Reader& r,const J& pin,const std::string& commit,const std::string& manual_sha,
                             const J& producer_binaries,const J& auditor_binaries,const J& auditors){
    const auto s=r.object(pin);
    w::require(s.at("schema")=="pas.pre-emulator-production-status.v1"&&s.at("source_commit")==commit&&
        !s.at("stopped").get<bool>()&&s.at("failure").is_null()&&s.at("remaining").empty()&&
        s.at("rows").is_array()&&s.at("rows").size()==9&&s.at("device_operations")==0&&
        !s.at("live_ready").get<bool>()&&auditors.is_array()&&auditors.size()==9,"entire nine-stage production plan");
    const auto plan=r.object(s.at("plan"));
    w::require(plan.at("schema")=="pas.pre-emulator-production-plan.v1"&&plan.at("source_commit")==commit&&
        plan.at("plans").is_array()&&plan.at("plans").size()==9&&plan.at("device_operations")==0&&
        !plan.at("production_binary_executed").get<bool>()&&!plan.at("live_ready").get<bool>(),"endpoint-free production plan");
    const std::array<const char*,9> ids={"normal-tap","normal-hold","normal-dense","stress-tap","stress-dense",
        "debug-tap","debug-hold","asan-tap","asan-hold"};
    const std::array<const char*,9> modes={"release","release","release","release","release","debug","debug","asan","asan"};
    const std::array<const char*,9> scenes={"tap","hold","dense","tap","dense","tap","hold","tap","hold"};
    for(std::size_t i=0;i<9;++i){const auto& p=plan.at("plans")[i];const auto& row=s.at("rows")[i];
        const auto report=r.object(row.at("report")),audit=r.object(row.at("audit"));
        const auto root=r.bound(row.at("report")).parent_path();const auto n=i<3?2560:i<5?10000:1000,cadence=i>=3&&i<5?8:20;
        w::require(p.at("id")==ids[i]&&row.at("id")==ids[i]&&p.at("mode")==modes[i]&&p.at("scene")==scenes[i]&&
            p.at("n")==n&&p.at("cadence")==cadence,"all original production configurations/workloads");
        w::require(report.at("schema")=="pas.windows-production-pipeline-cold.v1"&&report.at("scene")==scenes[i]&&
            report.at("requested")==n&&report.at("warmup")==256&&report.at("measurement_attempts")==n-256&&
            report.at("cadence_ms")==cadence&&report.at("source_build_commit")==commit&&
            !report.at("source_build_dirty").get<bool>()&&report.at("actual_manual_translation_unit_sha256")==manual_sha&&
            report.at("endpoint_free").get<bool>()&&report.at("public_entry_rejected").get<bool>()&&
            report.at("production_packet_action_wake_archive_executed").get<bool>()&&
            !report.at("real_capture_rpc_measured").get<bool>()&&report.at("device_operations")==0&&
            !report.at("live_ready").get<bool>(),"same literal production source and endpoint-free scope");
        w::require(audit.at("schema")=="pas.windows-production-pipeline-audit.v5"&&audit.at("integrity_verified").get<bool>()&&
            audit.at("source_commit")==commit&&audit.at("report")==row.at("report")&&
            audit.at("normal_cost_valid")==row.at("normal_cost_valid")&&
            !audit.at("physical_qualification").get<bool>()&&!audit.at("windows_cost_qualification").get<bool>()&&
            !audit.at("live_ready").get<bool>()&&audit.at("device_operations")==0,"complete raw production integrity");
        if(i<3)w::require(audit.at("normal_cost_valid").get<bool>()&&audit.at("normal_cost_failures").empty(),
            "normal Tap/Hold/Dense original necessary numerical gates");
        const auto pi=r.process(row.at("process")),ai=r.process(auditors[i]);
        const auto ps=r.object(row.at("process").at("specification")),as=r.object(auditors[i].at("specification"));
        w::require(final_path(ps.at("exe").get<std::string>())==r.bound(producer_binaries.at(modes[i]))&&
            ps.at("argv").size()==4&&ps.at("argv")[0]==scenes[i]&&ps.at("argv")[1]==std::to_string(n)&&
            ps.at("argv")[2]==std::to_string(cadence)&&final_path(ps.at("argv")[3].get<std::string>())==root&&
            pi.contains(r.bound(s.at("plan"))),"actual production producer command");
        const auto& args=as.at("argv");
        w::require(final_path(as.at("exe").get<std::string>())==r.bound(auditor_binaries.at(modes[i]))&&
            args.size()==8&&args[0]=="--root"&&final_path(args[1].get<std::string>())==root&&
            args[2]=="--expected-report-sha256"&&args[3]==row.at("report").at("sha256")&&
            args[4]=="--source-commit"&&args[5]==commit&&args[6]=="--output"&&
            final_path(args[7].get<std::string>())==r.bound(row.at("audit")),"actual production auditor command");
        frozen_raw_tree(ai,root);
    }
}
}

