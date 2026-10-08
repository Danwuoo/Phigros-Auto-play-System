// Includes the actual production source with endpoint-free boundary doubles.
#define main pas_diagnostic_production_cold_entry
#include "research/windows_production_pipeline_v3/main.cpp"
#undef main

int main(int argc,char** argv){try{
    using namespace cold;
    require(argc==3,"usage tap|hold|dense|invalid fresh-cold-output");
    const std::string operation=argv[1];
    const fs::path root=fs::absolute(argv[2]);
    require(!fs::exists(root),"fresh cold test fixture");fs::create_directory(root);
    J report={{"schema","pas.score-only-literal-controls.v1"},{"device_operations",0},
        {"live_ready",false},{"cost_qualification",false},{"physical_qualification",false}};
    if(operation=="invalid"){
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
    }else{
        require(operation=="tap"||operation=="hold"||operation=="dense","fixed endpoint-free scene");
        state.scene=operation;state.requested=1000;state.cadence=20;
        const auto profile=root/"profile.json";
        J config={{"schema",2},{"name","score-only cold boundary fixture"},{"serial","emulator-offline-cold"},
            {"capture",{{"kind","emulator-grpc"},{"execution","thread"},{"transport","payload"},
                {"image_format","rgb888"},{"row_order","top-down"},{"width",1280},{"height",720},
                {"source_rotation",1},{"grpc_read_chunk_kib",256},{"max_relative_lag_ms",250}}},
            {"touch",{{"kind","emulator-grpc"},{"width",720},{"height",1280},{"rotation_deg",90},
                {"max_contacts",5},{"timeout_ms",100}}},
            {"scheduler",{{"max_plans",128},{"max_steps",16},{"horizon_ms",350},{"evidence_max_age_ms",100}}},
            {"game",{{"lead_ms",35},{"uncertainty_ms",30},{"enabled_types",J::array({"tap","hold","drag","flick"})}}},
            {"preview",{{"hz",0}}},{"log_dir",(root/"forbidden-gameplay-archive").string()}};
        save(profile,config);
        pas::run_manual_session_impl(profile.string(),"endpoint-free-test-double",true,
            600'000'000'000LL,false,false,true,pas::ManualVariant::current_candidate,
            pas::SessionArchive::Persistence::none);
        require(!fs::exists(root/"forbidden-gameplay-archive"),"no gameplay directory, journal, PNG or summary created");
        require(state.cancelled&&state.touch_owner.has_value()&&state.preflights==1&&state.discoveries==1&&
            state.releases>0,"finite literal lifecycle and sole owner release executed");
        if(operation!="dense")require(state.downs>0&&state.ups>0,"real cold Down/Up opportunities executed");
        report["scene"]=operation;report["down"]=state.downs;report["move"]=state.moves;
        report["up"]=state.ups;report["release_calls"]=state.releases;
        report["no_gameplay_files_created"]=true;report["literal_production_source_executed"]=true;
        report["actual_manual_translation_unit_sha256"]=PAS_COLD_MANUAL_SHA;
        report["base_component_source_commit"]=pas_session_build_commit;
        report["base_component_source_dirty"]=pas_session_build_dirty;
    }
    report["whole_candidate_qualification_issued"]=false;
    report["scope"]="freshly compiled production source; cold boundary doubles only";
    report["pass"]=true;report["failed"]=0;save(root/"control-report.json",report);return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 2;}}
