// Endpoint-free tests of the actual controller ownership/budget implementation.
#define PAS_COLD_SCHEDULER_TRACE_NO_MAIN
#include "../tools/cold_scheduler_trace_controller.cpp"

namespace {
using namespace pas::cold_scheduler;
Config config() {
    Config c; c.name=L"PAS-ColdScheduler-base55-exec57-test"; c.file=L"C:\\fixture\\scheduler.etl";
    c.guid=parse_guid("ad6d3168-42a3-45bf-a968-184e03d845ee");
    c.start_qpc=100; c.deadline_qpc=750100; c.qpc_frequency=10000; return c;
}
Snapshot good(const Config& c) {
    return {17,c.guid,c.name,c.file,1,log_mode,enable_flags,64,64,1024,64,128,321,0,0,0};
}
struct FakeApi final : Api {
    Config c=config(); Snapshot current=good(c); std::vector<Snapshot> others;
    ULONG all_status=ERROR_SUCCESS,name_status=ERROR_WMI_INSTANCE_NOT_FOUND,start_status=ERROR_SUCCESS,stop_status=ERROR_SUCCESS;
    bool active=false, disappear=false, flush_loss=false; unsigned starts=0,stops=0,handle_queries=0; TRACEHANDLE stopped_handle=0;
    ULONG all(std::vector<Snapshot>& out) override { out=others; return all_status; }
    ULONG by_name(const std::wstring&, Snapshot& out) override { out=current; return name_status; }
    ULONG by_handle(TRACEHANDLE, Snapshot& out) override {
        ++handle_queries; if(!active || disappear) return ERROR_WMI_INSTANCE_NOT_FOUND; out=current; return ERROR_SUCCESS;
    }
    ULONG start(const Config&, TRACEHANDLE& h) override { ++starts; if(start_status==ERROR_SUCCESS) { h=17; active=true; } return start_status; }
    ULONG stop(TRACEHANDLE h, Snapshot& out) override {
        ++stops; stopped_handle=h; out=current;
        out.buffers_written=17; if(flush_loss) ++out.lost;
        if(stop_status==ERROR_SUCCESS || stop_status==ERROR_MORE_DATA) active=false; return stop_status;
    }
};
bool rejects(const auto& fn) { try { fn(); return false; } catch(const std::exception&) { return true; } }
}

int wmain(int argc,wchar_t** argv) {
    try {
        require(argc==2,"controls_require_fresh_report_path");
        Json checks=Json::array();
        auto check=[&](const char* id,const auto& fn) {
            bool pass=false; try { pass=fn(); } catch(const std::exception&) {}
            checks.push_back({{"id",id},{"pass",pass}});
        };
        check("valid_start_exact_handle_stop",[] { FakeApi a; Session s(a,a.c); return s.begin()&&s.finish()&&a.starts==1&&a.stops==1&&a.stopped_handle==17&&s.cleanup_confirmed&&s.final_statistics_verified&&s.latest.buffers_written==17; });
        check("existing_name_case_insensitive_no_start",[] { FakeApi a; auto foreign=a.current; foreign.name=L"pas-coldscheduler-base55-exec57-test"; a.others.push_back(foreign); Session s(a,a.c); return !s.begin()&&a.starts==0&&a.stops==0; });
        check("existing_guid_other_name_no_start",[] { FakeApi a; auto foreign=a.current; foreign.name=L"AnotherLogger"; a.others.push_back(foreign); Session s(a,a.c); return !s.begin()&&a.starts==0&&a.stops==0; });
        check("query_all_more_data_fail_closed",[] { FakeApi a; a.all_status=ERROR_MORE_DATA; Session s(a,a.c); return !s.begin()&&a.starts==0&&a.stops==0; });
        check("name_query_access_denied_no_start",[] { FakeApi a; a.name_status=ERROR_ACCESS_DENIED; Session s(a,a.c); return !s.begin()&&a.starts==0&&a.stops==0; });
        check("start_access_denied_no_stop",[] { FakeApi a; a.start_status=ERROR_ACCESS_DENIED; Session s(a,a.c); return !s.begin()&&!s.finish()&&a.starts==1&&a.stops==0; });
        check("start_collision_race_no_stop",[] { FakeApi a; a.start_status=ERROR_ALREADY_EXISTS; Session s(a,a.c); return !s.begin()&&!s.finish()&&a.starts==1&&a.stops==0; });
        check("normalized_buffer_over_cap_cleanup_owned",[] { FakeApi a; a.current.maximum=1025; Session s(a,a.c); return !s.begin()&&s.reason=="normalized_bounds_or_policy"&&s.finish()&&a.stops==1; });
        check("wrong_clock_cleanup_owned",[] { FakeApi a; a.current.clock=2; Session s(a,a.c); return !s.begin()&&s.finish()&&a.stops==1; });
        check("extra_process_flag_rejected",[] { FakeApi a; a.current.flags|=EVENT_TRACE_FLAG_PROCESS; Session s(a,a.c); return !s.begin()&&s.finish()&&a.stops==1; });
        check("circular_policy_rejected",[] { FakeApi a; a.current.mode=EVENT_TRACE_SYSTEM_LOGGER_MODE|EVENT_TRACE_FILE_MODE_CIRCULAR; Session s(a,a.c); return !s.begin()&&s.finish(); });
        check("unbounded_file_setting_rejected",[] { FakeApi a; a.current.maximum_file_mb=0; Session s(a,a.c); return !s.begin()&&s.finish(); });
        check("lost_events_not_accepted_cleanup_owned",[] { FakeApi a; a.current.lost=1; Session s(a,a.c); return !s.begin()&&s.reason=="trace_events_lost"&&s.finish(); });
        check("wrong_handle_never_stopped",[] { FakeApi a; a.current.handle=18; Session s(a,a.c); return !s.begin()&&!s.finish()&&a.stops==0; });
        check("foreign_guid_after_start_never_stopped",[] { FakeApi a; Session s(a,a.c); if(!s.begin()) return false; a.current.guid.Data1^=1; return !s.finish()&&a.stops==0; });
        check("changed_logger_thread_never_stopped",[] { FakeApi a; Session s(a,a.c); if(!s.begin()) return false; ++a.current.logger_thread; return !s.finish()&&a.stops==0; });
        check("changed_file_never_stopped",[] { FakeApi a; Session s(a,a.c); if(!s.begin()) return false; a.current.file=L"C:\\foreign.etl"; return !s.finish()&&a.stops==0; });
        check("own_handle_absent_foreign_name_untouched",[] { FakeApi a; Session s(a,a.c); if(!s.begin()) return false; a.disappear=true; a.name_status=ERROR_SUCCESS; return s.finish()&&a.stops==0&&s.foreign_untouched; });
        check("stop_more_data_absent_final_stats_unknown",[] { FakeApi a; a.stop_status=ERROR_MORE_DATA; Session s(a,a.c); return s.begin()&&s.finish()&&s.stop_status==ERROR_MORE_DATA&&a.stops==1&&!s.final_statistics_verified; });
        check("stop_failed_not_claimed_clean",[] { FakeApi a; a.stop_status=ERROR_ACCESS_DENIED; Session s(a,a.c); return s.begin()&&!s.finish()&&!s.cleanup_confirmed&&a.stops==1; });
        check("watchdog_adopts_same_instance_no_start",[] { FakeApi a; a.active=true; Session s(a,a.c); return s.adopt(a.current)&&s.finish()&&a.starts==0&&a.stopped_handle==17; });
        check("watchdog_bad_owner_guid_no_stop",[] { FakeApi a; a.active=true; auto owner=a.current; owner.guid.Data1^=1; Session s(a,a.c); return !s.adopt(owner)&&!s.finish()&&a.starts==0&&a.stops==0; });
        check("watchdog_owner_thread_mismatch_no_stop",[] { FakeApi a; a.active=true; auto owner=a.current; ++a.current.logger_thread; Session s(a,a.c); return !s.adopt(owner)&&!s.finish()&&a.stops==0; });
        check("shared_deadline_75s_accepted",[] { return valid_deadline(100,750100,10000,101); });
        check("shared_deadline_extension_rejected",[] { return !valid_deadline(100,750101,10000,101); });
        check("expired_deadline_rejected",[] { return !valid_deadline(100,750100,10000,750100); });
        check("future_start_rejected",[] { return !valid_deadline(100,750100,10000,99); });
        check("deadline_arithmetic_overflow_rejected",[] { return !valid_deadline(1,3,std::numeric_limits<std::uint64_t>::max(),2); });
        check("integer_float_rejected",[] { return rejects([] { (void)integer(Json{{"x",1.0}},"x"); }); });
        check("stop_flush_loss_preserved_in_final_stats",[] { FakeApi a; a.flush_loss=true; Session s(a,a.c); return s.begin()&&s.finish()&&s.final_statistics_verified&&!loss_free(s.latest)&&s.latest.lost==1&&s.latest.buffers_written==17; });
        check("watchdog_stop_race_absent_not_final_stats",[] { FakeApi a; Session s(a,a.c); if(!s.begin()) return false; a.active=false; return !s.inspect()&&s.finish()&&s.query_status==ERROR_WMI_INSTANCE_NOT_FOUND&&!s.final_statistics_verified&&a.stops==0; });
        check("reserved_kernel_guid_rejected",[] { return rejects([] { (void)parse_guid("9e814aad-3204-11d2-9a82-006008a86939"); }); });
        require(checks.size()==32,"controls_denominator");
        unsigned failed=0; for(const auto& c:checks) if(!c.at("pass").get<bool>()) ++failed;
        Json report={{"schema","pas.cold-scheduler-controller-controls.v1"},{"assertions",32},{"checks",checks},{"failed",failed},{"pass",failed==0},
            {"synthetic_api_controls",true},{"etw_start_operations",0},{"etw_stop_operations",0},{"device_endpoints",0},{"device_operations",0},
            {"diagnostic_only",true},{"cost_qualification",false},{"live_ready",false},{"qualification_issued",false},
            {"tool_source_commit",PAS_COLD_SCHEDULER_TOOL_SOURCE_COMMIT},{"tool_source_dirty",PAS_COLD_SCHEDULER_TOOL_SOURCE_DIRTY!=0},
            {"actual_recording_tested",false},{"sequential_file_mode",true}};
        create_json(argv[1],report); return failed?2:0;
    } catch(const std::exception& e) { std::fprintf(stderr,"cold_scheduler_controls: %.128s\n",e.what()); return 2; }
}
