// Includes the actual production source with endpoint-free boundary doubles.
#define main pas_diagnostic_production_cold_entry
#include "research/windows_production_pipeline_v3/main.cpp"
#undef main

namespace cold {
J score_profile(const fs::path& root){
    return {{"schema",2},{"name","score-only cold boundary fixture"},{"serial","emulator-offline-cold"},
            {"capture",{{"kind","emulator-grpc"},{"execution","thread"},{"transport","payload"},
                {"image_format","rgb888"},{"row_order","top-down"},{"width",1280},{"height",720},
                {"source_rotation",1},{"grpc_read_chunk_kib",256},{"max_relative_lag_ms",250}}},
            {"touch",{{"kind","emulator-grpc"},{"width",720},{"height",1280},{"rotation_deg",90},
                {"max_contacts",5},{"timeout_ms",100}}},
            {"scheduler",{{"max_plans",128},{"max_steps",16},{"horizon_ms",350},{"evidence_max_age_ms",100}}},
            {"game",{{"lead_ms",35},{"uncertainty_ms",30},{"enabled_types",J::array({"tap","hold","drag","flick"})}}},
            {"preview",{{"hz",0}}},{"log_dir",(root/"forbidden-gameplay-archive").string()}};
}
}

int main(int argc,char** argv){try{
    using namespace cold;
#ifdef PAS_SCORE_ONLY_INSTRUMENTED
    constexpr bool instrumented=true;
#else
    constexpr bool instrumented=false;
#endif
    require(argc==3,"usage tap|hold|dense|invalid fresh-cold-output");
    const std::string operation=argv[1];
    const fs::path root=fs::absolute(argv[2]);
    require(!fs::exists(root),"fresh cold test fixture");fs::create_directory(root);
    J report={{"schema","pas.score-only-literal-controls.v1"},{"device_operations",0},
        {"live_ready",false},{"cost_qualification",false},{"physical_qualification",false},
        {"whole_candidate_qualification_issued",false},{"actual_manual_translation_unit_sha256",PAS_COLD_MANUAL_SHA},
        {"base_component_source_commit",pas_session_build_commit},{"base_component_source_dirty",pas_session_build_dirty},
        {"scope","finite no-persistence lifecycle; exact Down/Up/cancel balance; no Hold semantic or cost qualification"}};
    report["instrumented_build"]=instrumented;
    report["real_time_qualification_issued"]=false;
    if(operation=="invalid"){
        // Fake-clock regression: both on-time and severely late workloads leave
        // the same 64 result slots at 20 ms spacing, without shifting workload.
        constexpr pas::Nanoseconds planned_end=20'000'000'000LL,late_end=79'000'000'000LL,cadence=20'000'000LL;
        require(score_result_epoch(planned_end,planned_end-cadence)==planned_end&&
            score_result_epoch(planned_end,late_end)==late_end,"result epoch never precedes planned or actual workload end");
        for(const auto end:{planned_end,late_end}){
            const auto first=score_result_epoch(planned_end,end),last=first+63*cadence;
            require(first>=end&&last-first==1'260'000'000LL&&last<first+2'000'000'000LL,
                "bounded paced result window for on-time and late fake clocks");
        }
        report["result_tail_fake_clock_cases"]=4;
        int passed=0;
        for(int i=0;i<7;++i){
            bool rejected=false;
            try{
                pas::run_manual_session_impl("missing","missing",i!=0,
                    i==4?0:i==5?-1:i==6?600'000'000'001LL:600'000'000'000LL,
                    i==1,i==2,i!=3,pas::ManualVariant::current_candidate,
                    pas::SessionArchive::Persistence::none);
            }catch(const std::invalid_argument& e){
                rejected=std::string_view(e.what()).starts_with("score-only trial requires");
            }
            require(rejected,"invalid score-only request rejected before I/O");++passed;
        }
        bool legacy_rejected=false;
        try{pas::run_manual_session_impl("missing","missing",true,600'000'000'000LL,
            false,false,true,pas::ManualVariant::legacy,pas::SessionArchive::Persistence::none);
        }catch(const std::invalid_argument&){legacy_rejected=true;}
        require(legacy_rejected&&state.preflights==0&&state.discoveries==0&&state.constructors==0,
            "no legacy fallback or endpoint I/O");
        report["invalid_requests_rejected_before_io"]=passed+1;
        // Exercise both controlled stop inputs in the actual manual-session
        // body, including capture cancellation, owner release and no archive.
        // Invalid requests above still reject before any boundary I/O.
        int stop_cases=0;
        for(const bool console_stop:{false,true}){
            state.scene="tap";state.requested=1000;state.cadence=20;
            state.score_only_result_tail=true;state.cancelled=false;
            state.touch_owner.reset();state.downs=state.moves=state.ups=state.releases=0;
            state.released_contacts=state.contacts_final=0;
            stop_boundary=StopBoundary{};
            stop_boundary.escape_held=!console_stop;
            stop_boundary.console_pending=console_stop;stop_boundary.console_event=CTRL_C_EVENT;
            const auto stop_root=root/(console_stop?"controlled-console-stop":"controlled-escape-stop");
            fs::create_directory(stop_root);const auto profile=stop_root/"profile.json";
            save(profile,score_profile(stop_root));
            pas::run_manual_session_impl(profile.string(),"endpoint-free-test-double",true,
                600'000'000'000LL,false,false,true,pas::ManualVariant::current_candidate,
                pas::SessionArchive::Persistence::none);
            require(pas::manual_stop.load()&&state.cancelled&&stop_boundary.key_polls>0&&
                stop_boundary.handler_adds==1&&stop_boundary.handler_removes==1&&
                stop_boundary.handler==nullptr&&stop_boundary.console_deliveries==(console_stop?1u:0u),
                "controlled Escape/console stop reaches real stop loop and unregisters handler");
            require(state.contacts_final==0&&state.downs==state.ups+state.released_contacts&&
                !fs::exists(stop_root/"forbidden-gameplay-archive"),
                "controlled stop releases all admitted contacts and writes no gameplay files");
            ++stop_cases;
        }
        report["controlled_stop_lifecycle_cases"]=stop_cases;
        report["no_gameplay_files_created"]=true;
    }else{
        require(operation=="tap"||operation=="hold"||operation=="dense","fixed endpoint-free scene");
        state.scene=operation;state.requested=1000;state.cadence=20;state.score_only_result_tail=true;
        const auto profile=root/"profile.json";
        const auto config=score_profile(root);
        save(profile,config);
        pas::run_manual_session_impl(profile.string(),"endpoint-free-test-double",true,
            600'000'000'000LL,false,false,true,pas::ManualVariant::current_candidate,
            pas::SessionArchive::Persistence::none);
        require(!fs::exists(root/"forbidden-gameplay-archive"),"no gameplay directory, journal, PNG or summary created");
        require(state.cancelled&&state.touch_owner.has_value()&&state.preflights==1&&state.discoveries==1&&
            state.releases>0,"finite literal lifecycle and sole owner release executed");
        report["scene"]=operation;report["down"]=state.downs;report["move"]=state.moves;
        report["synthetic_result_tail_policy"]="64 slots at original cadence from max(planned workload end, actual completion); requested attempts unchanged; diagnostic cost path disabled for this fixture";
        report["up"]=state.ups;report["release_calls"]=state.releases;
        report["contacts_released_by_cancel"]=state.released_contacts;report["contacts_final"]=state.contacts_final;
        // Missing current support may cancel a Hold through release_all.
        // Require every admitted contact to end, rather than force a tail Up.
        const bool contact_balance=state.downs==state.ups+state.released_contacts&&state.contacts_final==0;
        report["all_downs_terminalized"]=contact_balance;
        // Instrumentation can exceed the unchanged deadlines and safely prevent
        // every Down. This case checks integrity/cleanup, never real-time cost.
        // Release and Debug still require actual Tap/Hold opportunities; all
        // original R2 production/AA/ABBA Down/Up and deadline gates stay exact.
        const bool positive_down_required=!instrumented&&operation!="dense";
        report["positive_down_required"]=positive_down_required;
        report["safe_no_down_observed"]=state.downs==0;
        if(!contact_balance||(positive_down_required&&state.downs==0)){
            report["pass"]=false;report["failed"]=1;report["no_gameplay_files_created"]=true;
            save(root/"control-report.json",report);
            throw std::runtime_error("cold Down opportunities and exact Up/cancel contact balance required");
        }
        report["no_gameplay_files_created"]=true;report["literal_production_source_executed"]=true;
    }
    report["keyboard_is_test_double"]=true;report["console_registration_is_test_double"]=true;
    report["whole_candidate_qualification_issued"]=false;
    report["pass"]=true;report["failed"]=0;save(root/"control-report.json",report);return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 2;}}
