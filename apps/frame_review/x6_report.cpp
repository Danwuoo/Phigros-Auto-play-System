#include "x6_pixel.hpp"
#include "review_io.hpp"
#include "pas/adb.hpp"
#include "pas/analysis.hpp"
#include <deque>
#include <iostream>
#include <map>
#include <set>
#define NOMINMAX
#include <windows.h>
namespace {
using namespace pas::x6;using pas::review::json;
constexpr auto parent_sha="8bf5b5a5973bd028a254a8cc6b7179720aaed6468c0d00858e0ea0f8c6d20770";
struct MemoryLimit{HANDLE h=CreateJobObjectW(nullptr,nullptr);MemoryLimit(){JOBOBJECT_EXTENDED_LIMIT_INFORMATION l{};l.BasicLimitInformation.LimitFlags=JOB_OBJECT_LIMIT_PROCESS_MEMORY;l.ProcessMemoryLimit=512ULL*1024*1024;if(!h||!SetInformationJobObject(h,JobObjectExtendedLimitInformation,&l,sizeof(l))||!AssignProcessToJobObject(h,GetCurrentProcess()))throw std::runtime_error("x6_memory_cap");}~MemoryLimit(){if(h)CloseHandle(h);}};
Point point(const json& j){if(!j.is_array()||j.size()!=2)throw std::runtime_error("x6_point");return {j[0].get<double>(),j[1].get<double>()};}
json measurement_json(const Measurement& m){const auto cls=classify_measurement(m);return {
    {"current_supported",m.supported},{"clipped",m.clipped},{"longitudinal_bins",m.bins},{"sampling_votes",m.sampled},{"target_votes",m.target_votes},{"foreign_votes",m.foreign_votes},
    {"segments",m.segments},{"supported_bins",m.supported_bins},{"gap_bins",m.gap_bins},{"maximum_internal_gap",m.max_gap},{"foreign_gap_bins",m.foreign_gap_bins},
    {"span_px",m.supported?json(m.span):json(nullptr)},{"coverage",m.supported?json(m.coverage):json(nullptr)},{"extent_midpoint_offset_px",m.supported?json(m.midpoint_offset):json(nullptr)},
    {"color_centroid_offset_px",m.supported?json(m.centroid_offset):json(nullptr)},{"normal_centroid_px",m.supported?json(m.normal_centroid):json(nullptr)},{"lateral_radius_px",m.lateral_radius},
    {"canonical_tangent",json::array({m.tangent.x,m.tangent.y})},{"classification",cls==Reliability::unknown?"unknown":cls==Reliability::foreign_overlap_fragmentation?"foreign_overlap_fragmentation":"current_core_supported_not_motion_gold"}};}
struct Previous {std::int64_t ns=0;std::uint64_t id=0;Region r;Measurement m;bool unique=false;std::size_t ordinal=0;};
struct HistoryFrame{std::int64_t ns=0;std::vector<Previous> objects;};
}
int main(int argc,char** argv){try {
    if(argc!=2)throw std::runtime_error("x6_report fixed_accepted_report > new_report_json");MemoryLimit memory;
    const std::filesystem::path path=argv[1];if(std::filesystem::file_size(path)>6*1024*1024)throw std::runtime_error("x6_parent_capacity");
    if(pas::sha256_file(path)!=parent_sha)throw std::runtime_error("x6_parent_SHA");std::ifstream stream(path);const auto parent=json::parse(stream);
    if(parent.at("frames").size()!=337)throw std::runtime_error("x6_frame_denominator");
    json out={{"schema",1},{"experiment","X6-current-pixel-v1"},{"parent_report_sha256",parent_sha},{"binary_sha256",pas::sha256_file(argv[0])},
        {"rule_input",false},{"new_full_contact_replays",0},{"human_gold_added",0},{"physical_identity_and_role","unknown"},
        {"protocol",{{"colors","same RGB predicates as frozen observer, independent local sampling"},{"longitudinal_padding_px",24},{"max_bins",513},{"normal_radius_min_max",json::array({3,12})},{"minimum_target_votes_per_bin",2},{"fragment_gap_min",3},{"foreign_gap_bins_min",2},{"longitudinal_limit",464},{"history_frames",6},{"history_ns",90000000},{"output_limit_bytes",4194304},{"process_commit_bytes",536870912}}},
        {"counts",json::object()},{"frames",json::array()}};
    std::string prior_role;std::deque<HistoryFrame> history;std::set<std::size_t> verified;std::size_t targets=0,outputBytes=0;
    for(const auto& f:parent.at("frames")){
        const auto role=f.at("role").get<std::string>();if(role!=prior_role){history.clear();prior_role=role;}
        const auto now=f.at("fake_capture_ns").get<std::int64_t>();const auto ordinal=f.at("ordinal").get<std::size_t>();
        while(!history.empty()&&now-history.front().ns>90'000'000)history.pop_front();
        const std::filesystem::path png=f.at("png_path").get<std::string>();if(std::filesystem::file_size(png)>4*1024*1024)throw std::runtime_error("x6_png_capacity");
        if(verified.insert(ordinal).second&&pas::sha256_file(png)!=f.at("png_sha256").get<std::string>())throw std::runtime_error("x6_png_SHA");
        const auto rgb=pas::load_diagnostic_png(png);if(rgb.width!=1280||rgb.height!=720||rgb.stride!=3840)throw std::runtime_error("x6_png_geometry");
        auto& counts=out["counts"][role];if(counts.is_null())counts={{"targets",0},{"measured",0},{"excluded",0},{"foreign_overlap",0},{"unknown",0},{"paired",0}};
        HistoryFrame current;current.ns=now;json objects=json::array();if(f.at("objects").size()>128)throw std::runtime_error("x6_object_capacity");
        for(const auto& o:f.at("objects")){
            if(++targets>1220)throw std::runtime_error("x6_target_capacity");counts["targets"]=counts.at("targets").get<int>()+1;
            const auto kind=o.at("kind").get<std::string>();const auto id=o.at("note_id").get<std::uint64_t>();
            json entry={{"note_id_local_only",id},{"kind",kind},{"center",o.at("center")},{"observer_length",o.at("width")},{"observer_thickness",o.at("height")},{"measurement",nullptr},{"pair",nullptr}};
            const int color=kind=="tap"?1:kind=="drag"?2:kind=="flick"?3:0;
            if(!color||o.at("tangent").is_null()||o.at("width").get<double>()>464||o.at("height").get<double>()>96||o.at("decision_reason")=="hold_head_body_tail_outside_rule"){
                entry["reason"]="outside_nonHold_bounded_core_probe";counts["excluded"]=counts.at("excluded").get<int>()+1;objects.push_back(std::move(entry));continue;
            }
            const Region r{point(o.at("center")),point(o.at("tangent")),o.at("width").get<double>(),o.at("height").get<double>(),color};const auto m=measure(rgb,r);
            entry["measurement"]=measurement_json(m);entry["reason"]="current_pixels_measured_no_coordinate_substitution";counts["measured"]=counts.at("measured").get<int>()+1;
            if(classify_measurement(m)==Reliability::foreign_overlap_fragmentation)counts["foreign_overlap"]=counts.at("foreign_overlap").get<int>()+1;
            if(classify_measurement(m)==Reliability::unknown)counts["unknown"]=counts.at("unknown").get<int>()+1;
            const auto& ct=o.at("identity_assignment");const bool unique=!ct.is_null()&&ct.at("identity_ambiguous")==false;
            if(unique&&ct.at("assigned_prior_index").get<int>()>=0){const auto prior_ns=ct.at("prior_observed_ns").get<std::int64_t>();
                for(const auto& h:history)if(h.ns==prior_ns)for(const auto& p:h.objects)if(p.id==id&&p.unique){
                    if(std::hypot(p.r.center.x-ct.at("prior_x").get<double>(),p.r.center.y-ct.at("prior_y").get<double>())>1e-6)throw std::runtime_error("x6_correspondence_geometry");
                    const auto dt=now-prior_ns;if(dt<=0||dt>90'000'000)throw std::runtime_error("x6_pair_time");
                    const Point delta{r.center.x-p.r.center.x,r.center.y-p.r.center.y};
                    entry["pair"]={{"prior_ordinal",p.ordinal},{"dt_ns",dt},{"observer_axis_shift",delta.x*m.tangent.x+delta.y*m.tangent.y},{"observer_normal_shift",-delta.x*m.tangent.y+delta.y*m.tangent.x},
                        {"observer_length_change",r.length-p.r.length},{"axis_angle_rad",std::acos(std::clamp(std::abs(r.tangent.x*p.r.tangent.x+r.tangent.y*p.r.tangent.y),0.,1.))},
                        {"pixel_span_change",m.supported&&p.m.supported?json(m.span-p.m.span):json(nullptr)},
                        {"pixel_midpoint_offset_change_in_current_axis",m.supported&&p.m.supported?json(midpoint_offset_change(m,p.m)):json(nullptr)},
                        {"identity_grade","runtime_correspondence_proposed_not_physical_gold"}};
                    counts["paired"]=counts.at("paired").get<int>()+1;
                }
            }
            current.objects.push_back({now,id,r,m,unique,ordinal});objects.push_back(std::move(entry));
        }
        json frame={{"role",role},{"ordinal",ordinal},{"source_frame",f.at("source_frame")},{"png_path",png.string()},{"png_sha256",f.at("png_sha256")},{"fake_capture_ns",now},{"objects",std::move(objects)}};
        outputBytes+=frame.dump().size();if(outputBytes>4*1024*1024-65536)throw std::runtime_error("x6_output_capacity");out["frames"].push_back(std::move(frame));history.push_back(std::move(current));while(history.size()>5)history.pop_front();
    }
    if(targets!=1220||verified.size()!=114)throw std::runtime_error("x6_denominator");out["target_occurrences"]=targets;out["unique_png_verified"]=verified.size();
    const auto encoded=out.dump();if(encoded.size()+1>4*1024*1024)throw std::runtime_error("x6_output_capacity");std::cout<<encoded<<'\n';return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
