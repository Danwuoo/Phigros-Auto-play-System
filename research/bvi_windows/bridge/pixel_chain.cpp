#define NOMINMAX
#include <windows.h>
#include <wincodec.h>
#include <psapi.h>
#include <wrl/client.h>
#include "bridge.hpp"
#include "pixel_diagnostics.hpp"
#include "pas/game_session.hpp"
#include <algorithm>
#include <cmath>
#include <fstream>
#include <iostream>
#include <memory>
#include <stdexcept>

using namespace pas;
using namespace pas::bvi_offline;
using J=nlohmann::json;
using Microsoft::WRL::ComPtr;
namespace {
void checked(HRESULT code){if(FAILED(code))throw std::runtime_error("WIC decode failed");}
J pixel_rgb(const Frame& frame,double x,double y) {
    return diagnostic::rgb_at(frame,x,y);
}
J loaded_modules() {
    std::array<HMODULE,128> modules{};DWORD needed=0;
    if(!K32EnumProcessModules(GetCurrentProcess(),modules.data(),sizeof(modules),&needed)||needed>sizeof(modules))
        throw std::runtime_error("loaded module capacity or enumeration failure");
    J result=J::array();
    for(std::size_t i=0;i<needed/sizeof(HMODULE);++i) {
        std::array<wchar_t,512> path{};
        const auto count=GetModuleFileNameW(modules[i],path.data(),static_cast<DWORD>(path.size()));
        if(!count||count>=path.size())throw std::runtime_error("loaded module path bound");
        result.push_back(std::filesystem::path(path.data()).string());
    }
    return result;
}
Frame load(IWICImagingFactory* factory,const J& input) {
    const auto& index=input.at("index");const std::filesystem::path path=input.at("path").get<std::string>();
    ComPtr<IWICBitmapDecoder> decoder;checked(factory->CreateDecoderFromFilename(path.c_str(),nullptr,
        GENERIC_READ,WICDecodeMetadataCacheOnDemand,&decoder));
    ComPtr<IWICBitmapFrameDecode> bitmap;checked(decoder->GetFrame(0,&bitmap));
    UINT width=0,height=0;checked(bitmap->GetSize(&width,&height));
    if(width!=1280||height!=720||index.at("width")!=1280||index.at("height")!=720||
       index.at("format")!="PNG_RGB24_lossless_unannotated")throw std::runtime_error("frozen pixel profile mismatch");
    ComPtr<IWICFormatConverter> converter;checked(factory->CreateFormatConverter(&converter));
    checked(converter->Initialize(bitmap.Get(),GUID_WICPixelFormat24bppRGB,WICBitmapDitherTypeNone,
        nullptr,0,WICBitmapPaletteTypeCustom));
    Frame f;f.epoch=f.generation=f.geometry_version=1; // declared new OFFLINE context
    f.width=1280;f.height=720;f.stride=3840;f.sequence=index.at("source_frame");
    f.capture_complete_ns=index.at("capture_complete_ns");f.pixels_ready_ns=index.at("pixels_ready_ns");
    f.source_rotation=index.at("source_rotation");f.source_valid=true;
    f.rgb.resize(1280*720*3);checked(converter->CopyPixels(nullptr,f.stride,static_cast<UINT>(f.rgb.size()),f.rgb.data()));
    return f;
}
J distribution(std::vector<double> values) {
    if(values.empty())return {{"n",0}};
    std::sort(values.begin(),values.end());
    auto percentile=[&](double p){const double n=(values.size()-1)*p;
        const auto lo=static_cast<std::size_t>(n),hi=std::min(lo+1,values.size()-1);
        return values[lo]+(values[hi]-values[lo])*(n-lo);};
    const auto median=percentile(.50);std::vector<double> deviations;
    for(double v:values)deviations.push_back(std::abs(v-median));std::sort(deviations.begin(),deviations.end());
    return {{"n",values.size()},{"p50",median},{"p95",percentile(.95)},{"p99",percentile(.99)},
        {"max",values.back()},{"jitter_p95_minus_p5",percentile(.95)-percentile(.05)},
        {"absolute_deviation_p95",deviations[static_cast<std::size_t>((deviations.size()-1)*.95)]}};
}
struct Window {
    FakeClock business;ReplayBackend backend{business};GameObserver observer{business};
    PlaySessionLifecycle lifecycle;
    SessionGameOwner owner{business,backend,5,{3,0,30'000'000}};
    ExecutionLedger ledger;
    std::unique_ptr<bvi::Candidate> candidate=std::make_unique<bvi::Candidate>();
    bool sync() {
        const auto events=backend.take_events();
        if(!owner.owner())return events.empty();
        const auto plans=owner.owner()->take_accepted_plans();
        owner.owner()->take_coverage_updates();owner.owner()->take_plan_cancellations();
        return !backend.overflow()&&ledger.observe(plans,events,*owner.scheduler());
    }
    bool finish() {
        if(!owner.owner())return backend.active_count()==0;
        const auto report=owner.finish();backend.take_events();
        return report.failed_ids.empty()&&report.unknown_ids.empty()&&backend.active_count()==0;
    }
};
}
int main(int argc,char** argv) {
    std::unique_ptr<Window> window;
    std::ofstream trace;
    std::size_t processed=0,trace_bytes=0;
    constexpr auto trace_cap=diagnostic::trace_cap_bytes;
    try {
    if(argc!=3||std::filesystem::exists(argv[2]))throw std::runtime_error("usage: pixel_chain SELECTION FRESH_REPORT");
    const auto trace_path=std::filesystem::absolute(std::string(argv[2])+".rows.jsonl");
    if(std::filesystem::exists(trace_path))throw std::runtime_error("fresh trace required");
    trace.open(trace_path);if(!trace)throw std::runtime_error("trace create failed");
    std::ifstream in(argv[1]);const J manifest=J::parse(in);
    const auto& frames=manifest.at("frames");
    if(manifest.at("schema")!="pas.windows-offline-pixels-selection.v1"||frames.empty()||frames.size()>256||
       manifest.at("selected_count")!=frames.size()||manifest.at("human_gold")!=0)
        throw std::runtime_error("bounded selection schema");
    checked(CoInitializeEx(nullptr,COINIT_MULTITHREADED));
    ComPtr<IWICImagingFactory> factory;checked(CoCreateInstance(CLSID_WICImagingFactory,nullptr,
        CLSCTX_INPROC_SERVER,IID_PPV_ARGS(&factory)));
    HostClock meter;
    int window_id=0;std::uint64_t previous_frame=0;Nanoseconds previous_capture=0;
    std::size_t invalid=0,unmapped=0,body_patch=0,unsupported=0,allowed=0,owned_downs=0,owned_moves=0,owned_ups=0;
    std::size_t peak_lines=0,peak_roi=0,peak_contacts=0,failed=0,raw_candidates=0,tracked_targets=0;
    std::uint64_t peak_probes=0;
    std::vector<double> decode,compute,total,bridge;
    for(const auto& input:frames) {
        const int next_window=input.at("window");
        if(next_window!=window_id) {
            if(window) {
                if(!window->finish())throw std::runtime_error("window release failed");
                owned_downs+=window->backend.downs;owned_moves+=window->backend.moves;owned_ups+=window->backend.ups;
            }
            if(next_window<=window_id)throw std::runtime_error("window ordering");
            window=std::make_unique<Window>();window_id=next_window;previous_frame=0;previous_capture=0;
        }
        const auto start=meter.now_ns();auto frame=load(factory.Get(),input);const auto ready=meter.now_ns();
        if(previous_frame&&(frame.sequence<=previous_frame||frame.capture_complete_ns<=previous_capture))
            throw std::runtime_error("nonmonotonic recorded frame");
        previous_frame=frame.sequence;previous_capture=frame.capture_complete_ns;
        // Recorded QPC values drive only the deterministic offline clock.
        // Native compute timestamps are measured separately by HostClock above.
        window->business.set(frame.pixels_ready_ns);window->owner.poll();
        if(!window->sync())throw std::runtime_error(std::string(window->ledger.reason()));
        auto scene=window->observer.process(frame);
        const auto recognition_complete=meter.now_ns();
        const auto ui=session_ui_pixels(frame,scene);
        const auto status=window->lifecycle.observe(scene.context,ui,scene.playing_gate,window->business.now_ns());
        if(status.new_round) {
            window->owner.start(status.round);
            // Same start transition as formal SessionPerception: the observer
            // rewarms after reset, and the opening snapshot cannot dispatch.
            window->observer.reset();scene.playing_gate=false;scene.targets.clear();scene.lines.clear();
        }
        const bool allow=status.state==PlaySessionState::playing&&status.active&&scene.playing_gate&&
            scene.capacity_valid&&!ui.result;
        const auto bridge_start=meter.now_ns();
        auto result=evaluate(*window->candidate,frame,window->observer.candidate_batch(),scene,
                             window->ledger,window->business.now_ns());
        const auto bridge_end=meter.now_ns();
        window->owner.accept(result.filtered,allow);window->owner.poll();
        const bool verified=window->sync();
        const auto complete=meter.now_ns();
        if(!verified){++failed;throw std::runtime_error(std::string(window->ledger.reason()));}
        invalid+=!result.input.valid||result.observation.invalid||result.observation.context_invalid;
        unmapped+=result.input.unmapped;body_patch+=result.input.body_patch_denied;unsupported+=result.input.unsupported;
        allowed+=result.allowed;peak_lines=std::max(peak_lines,result.input.line_count);peak_roi=std::max(peak_roi,result.input.count);
        const auto& current_batch=window->observer.candidate_batch();
        raw_candidates+=current_batch.candidates.size();tracked_targets+=scene.targets.size();
        peak_probes=std::max(peak_probes,result.observation.probes);
        peak_contacts=std::max(peak_contacts,window->backend.active_count());
        decode.push_back((ready-start)/1e6);compute.push_back((complete-ready)/1e6);
        total.push_back((complete-start)/1e6);bridge.push_back((bridge_end-bridge_start)/1e6);
        const auto ordinal=input.at("index").at("ordinal").get<int>();
        J roi=J::array();for(std::size_t i=0;i<result.input.count;++i) {
            const auto& d=result.observation.parts[i];const auto* a=window->ledger.find(result.input.note_ids[i]);
            const auto& q=result.input.queries[i];
            const double ux=std::cos(q.angle),uy=std::sin(q.angle),nx=-uy,ny=ux;
            auto sample=[&](double along,double normal){
                return pixel_rgb(frame,q.front.x+along*ux+normal*nx,q.front.y+along*uy+normal*ny);};
            roi.push_back({{"roi_index",i},{"candidate_id",result.input.candidate_ids[i]},
                {"note_id",result.input.note_ids[i]},{"front_end",d.front_end},{"body",bvi::name(d.body)},
                {"contact",bvi::name(d.contact)},{"line_id",d.line_id},{"line_unique",d.line_unique},
                {"relation_usable",result.observation.relations[i].usable},
                {"actual_intent",a?J(a->intent):J(nullptr)},{"actual_contact",a&&a->contact>=0?J(a->contact):J(nullptr)},
                {"full_cursor",a&&a->cursor?J(*a->cursor):J(nullptr)},
                {"query_tap",q.tap},{"query_front",{q.front.x,q.front.y}},{"query_depth",q.depth},
                {"query_width",q.width},{"query_angle",q.angle},
                {"tap_normal_minus3_rgb",sample(0,-3)},{"tap_normal_plus3_rgb",sample(0,3)},
                {"body_mid_left_rgb",sample(-.35*q.width,-q.depth/2)},
                {"body_mid_right_rgb",sample(.35*q.width,-q.depth/2)},
                {"rail_mid_left_rgb",sample(-q.width/2,-q.depth/2)},
                {"rail_mid_right_rgb",sample(q.width/2,-q.depth/2)},
                {"front_exterior_left3_rgb",sample(-.35*q.width,3)},
                {"front_exterior_right3_rgb",sample(.35*q.width,3)}});
            if(ordinal==1519||ordinal==3030)
                roi.back()["post_dispatch_witness_review"]=diagnostic::witness_review(frame,q);
        }
        // Diagnostic sidecars retain the complete current proposal denominator,
        // including body patches and unsupported kinds rejected by the adapter.
        // They flow out after dispatch and never feed a later decision.
        J raw=J::array(),lines=J::array();
        for(const auto& c:current_batch.candidates) {
            const auto& n=c.note;
            raw.push_back({{"candidate_id",c.candidate_id},{"origin",c.origin},
                {"quality",static_cast<int>(c.quality)},{"head_visible",c.head_visible},
                {"body_visible",c.body_visible},{"held_body_patch",n.held_body_patch},
                {"held_body_evidence",n.held_body_evidence},{"head_on_line",n.head_on_line},
                {"kind",static_cast<int>(n.kind)},{"center",{n.center.x,n.center.y}},
                {"width",n.width},{"screen_y_height",n.height},{"tangent",{n.tangent.x,n.tangent.y}},
                {"tail",n.tail?J::array({n.tail->x,n.tail->y}):J(nullptr)},
                {"left_rail",c.left_rail},{"right_rail",c.right_rail},{"action_support",c.action_support},
                {"hint_source_frame",c.hint_source_frame},{"hint_age_ns",c.hint_age_ns}});
        }
        for(const auto& l:current_batch.lines)
            lines.push_back({{"track_id",l.track_id},{"center",{l.center.x,l.center.y}},
                {"tangent",{l.tangent.x,l.tangent.y}},{"length",l.length},
                {"observed_ns",l.observed_ns},{"association_valid",l.association_valid},
                {"motion_valid",l.motion_valid}});
        const J row={{"window",window_id},{"ordinal",input.at("index").at("ordinal")},
            {"source_frame",frame.sequence},{"recorded_capture_ns",frame.capture_complete_ns},
            {"recorded_pixels_ready_ns",frame.pixels_ready_ns},{"host_decode_start_ns",start},
            {"host_pixels_ready_ns",ready},{"host_recognition_complete_ns",recognition_complete},
            {"host_bridge_complete_ns",bridge_end},{"host_complete_ns",complete},{"allow_down",allow},
            {"all_lines",result.input.line_count},{"roi_count",result.input.count},{"allowed_targets",result.allowed},
            {"bridge_input_valid",result.input.valid},{"bridge_reason",result.input.reason},
            {"body_patch_denied",result.input.body_patch_denied},{"unmapped",result.input.unmapped},
            {"ledger_valid",verified},{"actual_active_contacts",window->backend.active_count()},{"roi",roi},
            {"raw_candidates",raw},{"current_measured_lines",lines},{"raw_targets",scene.targets.size()},
            {"batch_history_source",current_batch.history_source},{"bvi_probes",result.observation.probes},
            {"bvi_metadata_bytes",bvi::Candidate::metadata_bytes()}};
        const auto encoded=row.dump();
        if(encoded.size()+1>trace_cap-trace_bytes)throw std::runtime_error("trace byte capacity");
        trace<<encoded<<'\n';trace.flush();if(!trace)throw std::runtime_error("trace write failed");
        trace_bytes+=encoded.size()+1;++processed;
    }
    if(!window->finish())throw std::runtime_error("final release failed");
    owned_downs+=window->backend.downs;owned_moves+=window->backend.moves;owned_ups+=window->backend.ups;
    J report={{"schema","pas.windows-bvi-real-pixels-offline-chain.v1"},{"input_frames",frames.size()},
        {"processed_frames",processed},{"invalid_frames",invalid},{"failed",failed},{"body_patch_denied",body_patch},
        {"unsupported_queries",unsupported},{"unmapped_queries",unmapped},{"allowed_targets",allowed},
        {"own_fake_downs",owned_downs},{"own_fake_moves",owned_moves},{"own_fake_ups",owned_ups},
        {"peak_all_lines",peak_lines},{"peak_roi",peak_roi},{"peak_contacts",peak_contacts},
        {"raw_candidates",raw_candidates},{"raw_tracked_targets",tracked_targets},
        {"peak_bvi_probes",peak_probes},{"bvi_metadata_bytes",bvi::Candidate::metadata_bytes()},
        {"final_release_verified",true},{"timing_clock_domain","current_host_qpc_ns"},
        {"business_clock_domain","offline_fake_clock_replaying_archived_host_qpc_values"},
        {"decode_io_ms",distribution(decode)},{"complete_observer_bridge_owner_scheduler_receipts_ms",distribution(compute)},
        {"decode_and_complete_chain_ms",distribution(total)},{"bridge_ms",distribution(bridge)},
        {"runtime_cost_gate","not_ready; sequential archived replay, no latest-frame producer/RPC/injection/journal-pressure or frozen AA/AB"},
        {"timing_excludes","post-dispatch diagnostic serialization/flush and final release; no live injection"},
        {"roi_source","formal GameObserver current candidate_batch; baseline-guided recent anchors disclosed"},
        {"frame_context","new offline epoch/generation/geometry; not a current device fingerprint"},
        {"physical_human_gold",0},{"device_commands",0},{"source_absolute_age",nullptr},
        {"loaded_module_paths",loaded_modules()},
        {"color_samples","diagnostic-only rounded current query-local pixels; neither human gold nor policy input"},
        {"witness_review_contract",{{"normal_count",diagnostic::normal_count},
            {"tangent_count",diagnostic::tangent_count},{"points_per_query",diagnostic::points_per_query},
            {"max_queries_per_frame",128},{"selected_frame_cap",2},{"flow","post-dispatch output only"}}},
        {"trace_rows_path",trace_path.string()},{"trace_rows_bytes",trace_bytes},{"trace_cap_bytes",trace_cap}};
    std::ofstream out(argv[2]);out<<report.dump(2)<<'\n';if(!out)return 2;
    std::cout<<"real pixels="<<processed<<" invalid="<<invalid<<" own_fake_downs="<<owned_downs<<'\n';return 0;
    }catch(const std::exception& e) {
        bool release_verified=!window;std::string release_reason="no_owner";
        if(window) {
            const auto* scheduler=window->owner.scheduler();
            if(scheduler&&!scheduler->fault().empty()) {
                // The scheduler has already performed its one release attempt.
                // Preserve failure/unknown state; never retry release here.
                const auto r=scheduler->last_release();
                release_verified=r.failed_ids.empty()&&r.unknown_ids.empty()&&window->backend.active_count()==0;
                release_reason=scheduler->fault();
            } else try {release_verified=window->finish();release_reason="exception_stop";}
              catch(const std::exception& x){release_verified=false;release_reason=x.what();}
        }
        trace.flush();
        if(argc==3) {
            const auto failure_path=std::filesystem::exists(argv[2])?std::string(argv[2])+".failure.json":std::string(argv[2]);
            if(!std::filesystem::exists(failure_path)) {
                std::ofstream out(failure_path);
                out<<J({{"schema","pas.windows-bvi-offline-chain-failure.v1"},{"reason",e.what()},
                    {"processed_frames",processed},{"trace_rows_bytes",trace_bytes},
                    {"release_verified",release_verified},{"release_reason",release_reason},
                    {"device_commands",0}}).dump(2)<<'\n';
            }
        }
        std::cerr<<e.what()<<'\n';return 2;
    }
}
