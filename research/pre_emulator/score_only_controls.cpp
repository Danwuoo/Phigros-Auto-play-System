// Includes the actual production source with endpoint-free boundary doubles.
#define main pas_diagnostic_production_cold_entry
#include "research/windows_production_pipeline_v3/main.cpp"
#undef main

namespace cold {
// Test the same producer orchestration called by GrpcCapture::stream. These
// bounded API doubles create no threads, timers, endpoints or gameplay files.
// The existing controlled Stop cases below additionally execute the real
// current-thread Windows scopes through the literal capture worker.
struct PlacementAffinityDouble {
    std::uint32_t id=11,process=0x20,groups=1;
    std::uint64_t process_mask=0xff,system_mask=0xff;
    pas::current_rails::WorkerGroupAffinity value{0,0xff};
    int reads=0,sets=0,fail_read=0,fail_set=0,ignore_set=0;
    std::uint32_t process_class()const noexcept{return process;}
    std::uint32_t active_groups()const noexcept{return groups;}
    bool process_masks(std::uint64_t& p,std::uint64_t& s)const noexcept{p=process_mask;s=system_mask;return true;}
    pas::current_rails::WorkerCoreTopology topology()const noexcept{
        pas::current_rails::WorkerCoreTopology t;t.failure=pas::current_rails::WorkerAffinityFailure::none;
        t.count=4;t.returned_bytes=192;
        t.cores[0]={0,1,0x03};t.cores[1]={0,0,0x0c};t.cores[2]={0,1,0x30};t.cores[3]={0,1,0xc0};return t;
    }
    std::uint32_t thread()const noexcept{return id;}
    std::optional<pas::current_rails::WorkerGroupAffinity> current()noexcept{if(++reads==fail_read)return {};return value;}
    bool set(pas::current_rails::WorkerGroupAffinity requested)noexcept{if(++sets==fail_set)return false;if(sets!=ignore_set)value=requested;return true;}
};
struct PlacementPriorityDouble {
    std::uint32_t id=11;int value=0,reads=0,sets=0,fail_read=0,fail_set=0,ignore_set=0;
    std::uint32_t thread()const noexcept{return id;}
    int current()noexcept{return ++reads==fail_read?std::numeric_limits<int>::max():value;}
    bool set(int requested)noexcept{if(++sets==fail_set)return false;if(sets!=ignore_set)value=requested;return true;}
};
template<class Body>
std::string placement_error(PlacementAffinityDouble& a,PlacementPriorityDouble& p,
        const pas::current_rails::PlacementPlan& plan,producer_placement::Snapshot& s,Body&& body){
    try{producer_placement::run(a,p,plan,s,std::forward<Body>(body));return {};}
    catch(const std::exception& e){return e.what();}
}
J placement_controls(){
    using namespace pas::current_rails;using S=producer_placement::Snapshot;using F=producer_placement::Failure;
    J cases=J::array();int failed=0;
    const auto check=[&](const char* id,bool pass){cases.push_back({{"id",id},{"pass",pass}});failed+=!pass;};
    {PlacementAffinityDouble a,writer,consumer;PlacementPriorityDouble p;S s;const auto plan=make_worker_placement(a);
     bool role=false;const auto error=placement_error(a,p,plan,s,[&]{role=a.value.mask==0x40&&a.value.mask!=plan.consumer_mask&&p.value==2;});
     check("actual_body_uses_existing_producer_role_and_HIGHEST2",error.empty()&&role&&s.verified());
     check("no_process_consumer_or_writer_mutation",a.process==0x20&&writer.value.mask==0xff&&writer.sets==0&&consumer.value.mask==0xff&&consumer.sets==0);
     check("successful_body_explicitly_restores_both_original_values",a.value.mask==0xff&&p.value==0&&s.affinity.close_attempted&&s.priority_close_attempted&&!s.affinity.fallback_attempted);}
    {PlacementAffinityDouble a;PlacementPriorityDouble p;S s;const auto plan=make_worker_placement(a);a.value.mask=8;p.value=-1;
     const auto error=placement_error(a,p,plan,s,[]{});check("nondefault_original_affinity_and_priority_exact_restore",error.empty()&&s.verified()&&a.value.mask==8&&p.value==-1&&s.priority_restored==-1);}
    {PlacementAffinityDouble a;PlacementPriorityDouble p;S s;const auto plan=make_worker_placement(a);bool cancelled=false;
     const auto error=placement_error(a,p,plan,s,[&]{cancelled=true;});check("cooperative_cancellation_returns_only_after_explicit_cleanup",error.empty()&&cancelled&&s.body_completed&&s.verified()&&a.value.mask==0xff&&p.value==0);}
    {PlacementAffinityDouble a;PlacementPriorityDouble p;S s;const auto plan=make_worker_placement(a);
     const auto error=placement_error(a,p,plan,s,[]{throw std::runtime_error("callback fault");});check("callback_exception_preserved_after_both_explicit_restores",error=="callback fault"&&s.failure==F::body_exception&&s.affinity.verified()&&s.priority_explicit_restored&&!s.verified()&&a.value.mask==0xff&&p.value==0);}
    {PlacementAffinityDouble a;PlacementPriorityDouble p;S s;a.groups=2;const auto plan=make_worker_placement(a);
     const auto error=placement_error(a,p,plan,s,[]{});check("invalid_plan_rejects_before_body_or_mutation",!error.empty()&&s.failure==F::placement&&!s.body_entered&&a.sets==0&&p.sets==0);}
    {PlacementAffinityDouble a;PlacementPriorityDouble p;S s;const auto plan=make_worker_placement(a);a.groups=2;
     const auto error=placement_error(a,p,plan,s,[]{});check("scope_rechecks_current_groups_before_body",!error.empty()&&s.failure==F::affinity_enter&&s.affinity.failure==WorkerAffinityFailure::single_active_group_required&&!s.body_entered&&p.sets==0);}
    {PlacementAffinityDouble a;PlacementPriorityDouble p;S s;const auto plan=make_worker_placement(a);a.process_mask=0xbf;
     const auto error=placement_error(a,p,plan,s,[]{});check("scope_rechecks_producer_lp_in_current_process_mask",!error.empty()&&s.affinity.failure==WorkerAffinityFailure::requested_mask_not_in_process&&!s.body_entered&&p.sets==0);}
    {PlacementAffinityDouble a;PlacementPriorityDouble p;S s;const auto plan=make_worker_placement(a);a.fail_read=1;
     const auto error=placement_error(a,p,plan,s,[]{});check("original_affinity_query_failure_skips_priority_and_body",!error.empty()&&s.affinity.failure==WorkerAffinityFailure::affinity_query&&a.sets==0&&p.sets==0&&!s.body_entered);}
    {PlacementAffinityDouble a;PlacementPriorityDouble p;S s;const auto plan=make_worker_placement(a);a.fail_set=1;
     const auto error=placement_error(a,p,plan,s,[]{});check("affinity_apply_failure_skips_priority_and_body",!error.empty()&&s.affinity.failure==WorkerAffinityFailure::affinity_set&&p.sets==0&&!s.body_entered&&!s.verified());}
    {PlacementAffinityDouble a;PlacementPriorityDouble p;S s;const auto plan=make_worker_placement(a);a.ignore_set=1;
     const auto error=placement_error(a,p,plan,s,[]{});check("affinity_apply_readback_failure_retains_verified_rollback",!error.empty()&&s.affinity.failure==WorkerAffinityFailure::affinity_readback_mismatch&&s.affinity.rollback_restored&&p.sets==0&&!s.verified());}
    {PlacementAffinityDouble a;PlacementPriorityDouble p;S s;const auto plan=make_worker_placement(a);p.fail_read=1;
     const auto error=placement_error(a,p,plan,s,[]{});check("priority_query_failure_explicitly_restores_affinity",!error.empty()&&s.failure==F::priority_query&&s.affinity.verified()&&a.value.mask==0xff&&p.sets==0&&!s.verified());}
    {PlacementAffinityDouble a;PlacementPriorityDouble p;S s;const auto plan=make_worker_placement(a);p.fail_set=1;
     const auto error=placement_error(a,p,plan,s,[]{});check("priority_constructor_failure_rollback_checked_and_body_skipped",!error.empty()&&s.failure==F::priority_enter&&s.priority_rollback_attempted&&s.priority_rollback_restored&&s.affinity.verified()&&!s.body_entered&&!s.verified());}
    {PlacementAffinityDouble a;PlacementPriorityDouble p;S s;const auto plan=make_worker_placement(a);p.ignore_set=1;
     const auto error=placement_error(a,p,plan,s,[]{});check("priority_constructor_readback_mismatch_never_upgrades_rollback",!error.empty()&&s.failure==F::priority_enter&&s.priority_rollback_restored&&s.affinity.verified()&&!s.verified());}
    {PlacementAffinityDouble a;PlacementPriorityDouble p;S s;const auto plan=make_worker_placement(a);p.fail_read=4;
     const auto error=placement_error(a,p,plan,s,[]{});check("actual_priority_readback_failure_restores_scopes_without_body",!error.empty()&&s.failure==F::priority_applied_readback&&s.priority_explicit_restored&&s.affinity.verified()&&!s.body_entered&&!s.verified());}
    {PlacementAffinityDouble a;PlacementPriorityDouble p;S s;const auto plan=make_worker_placement(a);p.fail_set=2;
     const auto error=placement_error(a,p,plan,s,[]{});check("priority_restore_set_failure_cleanup_continues_but_fallback_not_PASS",!error.empty()&&s.cleanup_failure==F::priority_restore&&s.affinity.verified()&&p.value==0&&!s.priority_explicit_restored&&!s.verified());}
    {PlacementAffinityDouble a;PlacementPriorityDouble p;S s;const auto plan=make_worker_placement(a);p.ignore_set=2;
     const auto error=placement_error(a,p,plan,s,[]{});check("priority_restore_readback_mismatch_rejects_successful_set",!error.empty()&&s.cleanup_failure==F::priority_restore&&s.affinity.verified()&&p.value==0&&!s.verified());}
    {PlacementAffinityDouble a;PlacementPriorityDouble p;S s;const auto plan=make_worker_placement(a);p.fail_read=6;
     const auto error=placement_error(a,p,plan,s,[]{});check("extra_actual_restored_priority_query_failure_keeps_negative",!error.empty()&&s.cleanup_failure==F::priority_restored_readback&&s.priority_explicit_restored&&!s.priority_restored_available&&s.affinity.verified()&&!s.verified());}
    {PlacementAffinityDouble a;PlacementPriorityDouble p;S s;const auto plan=make_worker_placement(a);a.fail_set=2;
     const auto error=placement_error(a,p,plan,s,[]{});check("affinity_restore_failure_keeps_priority_cleanup_and_sticky_negative",!error.empty()&&s.cleanup_failure==F::affinity_restore&&s.priority_explicit_restored&&s.affinity.failure==WorkerAffinityFailure::restore_set&&s.affinity.fallback_attempted&&a.value.mask==0xff&&!s.verified());}
    {PlacementAffinityDouble a;PlacementPriorityDouble p;S s;const auto plan=make_worker_placement(a);a.fail_read=3;
     const auto error=placement_error(a,p,plan,s,[]{});check("affinity_restore_query_failure_cannot_be_destructor_verified",!error.empty()&&s.affinity.failure==WorkerAffinityFailure::restore_readback_query&&s.affinity.fallback_attempted&&!s.affinity.restored&&!s.verified());}
    {PlacementAffinityDouble a;PlacementPriorityDouble p;S s;const auto plan=make_worker_placement(a);p.fail_set=2;a.fail_set=2;
     const auto error=placement_error(a,p,plan,s,[]{});check("both_restore_failures_retained_and_both_fallbacks_unqualified",!error.empty()&&s.cleanup_failure==F::priority_restore&&s.affinity.failure==WorkerAffinityFailure::restore_set&&p.value==0&&a.value.mask==0xff&&!s.verified());}
    {PlacementAffinityDouble a;PlacementPriorityDouble p;S s;const auto plan=make_worker_placement(a);p.fail_set=2;
     const auto error=placement_error(a,p,plan,s,[]{throw std::runtime_error("original callback failure");});check("body_and_cleanup_failures_preserve_original_error_and_cleanup_reason",error=="original callback failure"&&s.failure==F::body_exception&&s.cleanup_failure==F::priority_restore&&s.affinity.verified()&&!s.verified());}
    {PlacementAffinityDouble a;PlacementPriorityDouble p;S s;const auto plan=make_worker_placement(a);
     const auto error=placement_error(a,p,plan,s,[&]{a.id=12;p.id=12;});check("wrong_thread_cleanup_never_mutates_foreign_thread",!error.empty()&&s.cleanup_failure==F::priority_restore&&s.affinity.failure==WorkerAffinityFailure::wrong_thread&&a.sets==1&&p.sets==1&&!s.verified());}
    {PlacementAffinityDouble a;PlacementPriorityDouble p;S s;const auto plan=make_worker_placement(a);const auto error=placement_error(a,p,plan,s,[]{});
     const auto priority=producer_placement::priority_json(s),affinity=producer_placement::affinity_json(s.affinity),placement=producer_placement::plan_json(s.placement);
     check("report_binds_actual_HIGHEST2_role_and_exact_restore_scalars",error.empty()&&priority.at("requested")==2&&priority.at("applied")==2&&priority.at("previous")==priority.at("restored")&&priority.at("verified")==true&&affinity.at("requested_mask")==placement.at("producer_mask")&&affinity.at("previous_mask")==affinity.at("restored_mask")&&placement.at("cost_qualification")==false);}
    {PlacementAffinityDouble a;PlacementPriorityDouble p;S s;const auto plan=make_worker_placement(a);a.value.mask=plan.producer_mask;p.value=2;
     const auto error=placement_error(a,p,plan,s,[]{});check("already_pinned_HIGHEST_worker_restores_same_original_values",error.empty()&&s.verified()&&s.priority_previous==2&&s.priority_restored==2&&a.value.mask==plan.producer_mask&&!s.affinity.fallback_attempted);}
    {PlacementAffinityDouble a;PlacementPriorityDouble p;S s;const auto plan=make_worker_placement(a);
     const auto error=placement_error(a,p,plan,s,[&]{a.process=0x80;});check("changed_process_class_after_body_rejected_after_both_restores",!error.empty()&&s.process_class_after_close==0x80&&s.cleanup_failure==F::process_class_after_close&&s.affinity.verified()&&s.priority_explicit_restored&&a.value.mask==0xff&&p.value==0&&!s.verified());}
    require(cases.size()==27,"fixed cold producer placement control denominator");
    J report={{"schema","pas.cold-producer-placement-controls.v1"},{"checks",cases.size()},
        {"passed",cases.size()-std::size_t(failed)},{"failed",failed},{"cases",cases},
        {"producer_priority_requested",pas::current_rails::producer_priority_requested},
        {"producer_wait_policy","full_period_active_qpc"},{"producer_active_tail_ns",0},
        {"producer_active_wait_horizon_ns",pas::current_rails::producer_active_wait_horizon_ns},
        {"producer_active_wait_poll_cap",pas::current_rails::producer_active_wait_poll_cap},{"producer_no_kernel_wait",true},
        {"synthetic_api_controls",true},{"device_operations",0},{"cost_qualification",false},{"live_ready",false}};
    if(failed)throw std::runtime_error("cold producer placement orchestration controls failed: "+report.dump());
    return report;
}
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
    pas::current_rails::ColdProcessQosSnapshot process_qos;
    pas::current_rails::WindowsColdProcessQosApi process_qos_api;
    pas::current_rails::run_cold_process_qos(process_qos_api,process_qos,[&]{
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
        report["producer_placement_controls"]=placement_controls();
        // Exercise both controlled stop inputs in the actual manual-session
        // body, including capture cancellation, owner release and no archive.
        // Invalid requests above still reject before any boundary I/O.
        int stop_cases=0;J stop_producer_snapshots=J::array();
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
            require(state.producer_placement.verified(),"controlled stop must explicitly restore producer placement and priority");
            stop_producer_snapshots.push_back({{"mode",console_stop?"console":"escape"},
                {"worker_placement",producer_placement::plan_json(state.producer_placement.placement)},
                {"producer_affinity",producer_placement::affinity_json(state.producer_placement.affinity)},
                {"producer_priority",producer_placement::priority_json(state.producer_placement)}});
            ++stop_cases;
        }
        report["controlled_stop_lifecycle_cases"]=stop_cases;
        report["controlled_stop_producer_snapshots"]=stop_producer_snapshots;
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
        require(state.producer_placement.verified(),"finite score-only fixture must explicitly restore producer placement and priority");
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
    });
    require(process_qos.verified(),"score-only cold process QoS must explicitly restore after joined workers");
    report["process_qos"]=pas::current_rails::cold_process_qos_json(process_qos);
    report["keyboard_is_test_double"]=true;report["console_registration_is_test_double"]=true;
    report["producer_priority_requested"]=pas::current_rails::producer_priority_requested;
    report["producer_wait_policy"]="full_period_active_qpc";report["producer_active_tail_ns"]=0;
    report["producer_active_wait_horizon_ns"]=pas::current_rails::producer_active_wait_horizon_ns;
    report["producer_active_wait_poll_cap"]=pas::current_rails::producer_active_wait_poll_cap;
    report["producer_no_kernel_wait"]=true;
    report["worker_placement"]=producer_placement::plan_json(state.producer_placement.placement);
    report["producer_affinity"]=producer_placement::affinity_json(state.producer_placement.affinity);
    report["producer_priority"]=producer_placement::priority_json(state.producer_placement);
    report["whole_candidate_qualification_issued"]=false;
    report["pass"]=true;report["failed"]=0;save(root/"control-report.json",report);return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 2;}}
