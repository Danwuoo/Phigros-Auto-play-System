#include "pas/analysis.hpp"
#include "pas/core.hpp"
#include <nlohmann/json.hpp>
#include <algorithm>
#include <fstream>
#include <iomanip>
#include <set>
#include <sstream>

namespace {
using nlohmann::json;
using pas::Nanoseconds;
json read_json(const std::filesystem::path& path) {
    if(std::filesystem::file_size(path)>2*1024*1024)throw std::invalid_argument("selection JSON capacity");
    std::ifstream file(path,std::ios::binary);return json::parse(file);
}
void write_json(const std::filesystem::path& path,const json& value) {
    std::ofstream file(path,std::ios::binary);file<<value.dump(2)<<'\n';
    if(!file)throw std::runtime_error("selection JSON write failed");
}
std::filesystem::path safe_relative(const std::string& value) {
    const std::filesystem::path path(value);
    if(path.empty()||path.is_absolute()||path.has_root_name())throw std::invalid_argument("selection absolute path");
    for(const auto& part:path)if(part=="..")throw std::invalid_argument("selection escaping path");
    return path;
}
bool within(Nanoseconds time,Nanoseconds start,Nanoseconds end){return time>=start&&time<=end;}
struct Clip {
    json request;
    std::filesystem::path root;
    std::size_t first=0,last=0,point=0;
    std::set<std::uint64_t> sequences;
    std::ofstream journal;
    std::uint64_t journal_rows=0,journal_bytes=0,core_frames=0;
};
}

// Offline packaging only. No observer, emulator, touch owner or model exists here.
std::vector<std::filesystem::path> prepare_recording_selection(
    const std::filesystem::path& session,const std::filesystem::path& selection_path,
    const std::filesystem::path& output) {
    if(std::filesystem::exists(output))throw std::invalid_argument("selection output exists; refusing overwrite");
    const auto selection=read_json(selection_path);
    const auto recording=session/"full-recording",round=session/"round-1";
    const auto recorded=read_json(recording/"summary.json"),summary=read_json(round/"summary.json");
    if(selection.value("example_only",false)||selection.at("session")!=session.filename().string()||
       selection.at("round_id")!=1||!recorded.at("continuous_received_pixels_complete").get<bool>()||
       pas::sha256_file(recording/"index.jsonl")!=selection.at("recording_index_sha256").get<std::string>()||
       recorded.at("index_sha256")!=selection.at("recording_index_sha256")||
       pas::sha256_file(selection_path.parent_path()/"full-round.mp4")!=selection.at("preview_mp4_sha256").get<std::string>())
        throw std::invalid_argument("selection source binding mismatch");
    if(std::filesystem::file_size(recording/"index.jsonl")>64*1024*1024)
        throw std::invalid_argument("recording index capacity");
    const auto requests=selection.at("clips");
    if(!requests.is_array()||requests.empty()||requests.size()>32)throw std::invalid_argument("selection clip capacity");
    std::vector<json> frames;std::ifstream source_index(recording/"index.jsonl");std::string line;
    Nanoseconds previous=0;std::uint64_t previous_sequence=0;
    while(std::getline(source_index,line)) {
        if(frames.size()>=36000||line.size()>64*1024)throw std::invalid_argument("recording rows capacity");
        auto entry=json::parse(line);const auto time=entry.at("capture_complete_ns").get<Nanoseconds>();
        const auto seq=entry.at("source_frame").get<std::uint64_t>();
        if(entry.at("ordinal")!=frames.size()||entry.at("width")!=1280||entry.at("height")!=720||
           (previous&&(time<=previous||seq<=previous_sequence)))throw std::invalid_argument("source recording order/geometry");
        safe_relative(entry.at("path").get<std::string>());frames.push_back(std::move(entry));
        previous=time;previous_sequence=seq;
    }
    if(frames.empty()||frames.size()!=recorded.at("frames_written").get<std::size_t>())
        throw std::invalid_argument("recording count mismatch");
    const auto origin=frames.front().at("capture_complete_ns").get<Nanoseconds>();
    const auto last_time=frames.back().at("capture_complete_ns").get<Nanoseconds>();
    std::vector<Clip> clips;std::set<std::string> ids;std::set<std::size_t> unique;
    std::uint64_t references=0;
    for(const auto& request:requests) {
        const auto id=request.at("id").get<std::string>();
        if(id.empty()||id.size()>32||!std::all_of(id.begin(),id.end(),[](unsigned char c){return
            (c>='a'&&c<='z')||(c>='0'&&c<='9')||c=='-';})||!ids.insert(id).second||request.dump().size()>16*1024)
            throw std::invalid_argument("selection id/request capacity");
        const auto start=request.at("start_ms").get<Nanoseconds>(),end=request.at("end_ms").get<Nanoseconds>();
        const auto before=request.value("context_before_ms",1000LL),after=request.value("context_after_ms",1000LL);
        if(start<0||end<start||end-start>40000||before<0||before>5000||after<0||after>5000||
           end*1'000'000>last_time-origin)throw std::invalid_argument("selection time bounds");
        const auto lo=origin+std::max<Nanoseconds>(0,start-before)*1'000'000;
        const auto hi=std::min(last_time,origin+(end+after)*1'000'000);
        auto first=std::lower_bound(frames.begin(),frames.end(),lo,[](const json& f,Nanoseconds t){return f.at("capture_complete_ns").get<Nanoseconds>()<t;});
        if(first!=frames.begin())--first; // retain the actual image active at window start
        auto stop=std::upper_bound(frames.begin(),frames.end(),hi,[](Nanoseconds t,const json& f){return t<f.at("capture_complete_ns").get<Nanoseconds>();});
        if(stop!=frames.end())++stop; // one real boundary-support image, never synthesized
        Clip clip;clip.request=request;clip.root=output/id;clip.first=first-frames.begin();clip.last=stop-frames.begin()-1;
        if(clip.last<clip.first||clip.last-clip.first+1>4000)throw std::invalid_argument("clip frames capacity");
        const auto desired=origin+start*1'000'000;
        clip.point=clip.first;
        for(auto i=clip.first;i<=clip.last;++i) {
            if(std::abs(frames[i].at("capture_complete_ns").get<Nanoseconds>()-desired)<
               std::abs(frames[clip.point].at("capture_complete_ns").get<Nanoseconds>()-desired))clip.point=i;
            unique.insert(i);clip.sequences.insert(frames[i].at("source_frame").get<std::uint64_t>());
        }
        references+=clip.last-clip.first+1;clips.push_back(std::move(clip));
    }
    if(unique.size()>12000||references>18000)throw std::invalid_argument("selection total frame capacity");
    const auto segments=summary.at("event_segments");
    if(segments.size()>32)throw std::invalid_argument("journal segment capacity");
    for(const auto& segment:segments) {
        const auto path=round/safe_relative(segment.at("path").get<std::string>());
        if(std::filesystem::file_size(path)>32*1024*1024||pas::sha256_file(path)!=segment.at("sha256").get<std::string>())
            throw std::invalid_argument("journal source SHA/capacity");
    }
    if(pas::sha256_file(round/"manifest.json")!=summary.at("manifest_sha256").get<std::string>())
        throw std::invalid_argument("round manifest SHA");
    std::uint64_t source_bytes=0;
    for(auto i:unique) {
        const auto& frame=frames[i];const auto path=recording/safe_relative(frame.at("path").get<std::string>());
        const auto bytes=std::filesystem::file_size(path);source_bytes+=bytes;
        if(bytes>4*1024*1024||source_bytes>3ULL*1024*1024*1024||pas::sha256_file(path)!=frame.at("png_sha256").get<std::string>())
            throw std::invalid_argument("selected PNG SHA/capacity");
    }
    std::filesystem::create_directories(output/"shared-frames");
    std::filesystem::copy_file(selection_path,output/"selection-snapshot.json");
    for(auto i:unique) {
        const auto& frame=frames[i];const auto relative=safe_relative(frame.at("path").get<std::string>());
        const auto target=output/"shared-frames"/relative.filename();
        std::filesystem::copy_file(recording/relative,target);
        if(pas::sha256_file(target)!=frame.at("png_sha256").get<std::string>())throw std::runtime_error("selected copy SHA mismatch");
        std::filesystem::permissions(target,std::filesystem::perms::owner_read|std::filesystem::perms::group_read|
            std::filesystem::perms::others_read,std::filesystem::perm_options::replace);
    }
    for(auto& clip:clips) {
        std::filesystem::create_directories(clip.root/"frames");
        std::ofstream index(clip.root/"index.jsonl",std::ios::binary);json gaps=json::array();
        Nanoseconds previous_clip_time=0;
        const auto base=frames[clip.first].at("capture_complete_ns").get<Nanoseconds>();
        for(auto i=clip.first;i<=clip.last;++i) {
            auto entry=frames[i];const auto time=entry.at("capture_complete_ns").get<Nanoseconds>();
            const auto name=safe_relative(entry.at("path").get<std::string>()).filename();
            std::filesystem::create_hard_link(output/"shared-frames"/name,clip.root/"frames"/name);
            const auto start=origin+clip.request.at("start_ms").get<Nanoseconds>()*1'000'000;
            const auto end=origin+clip.request.at("end_ms").get<Nanoseconds>()*1'000'000;
            const bool core=start==end?i==clip.point:within(time,start,end);
            if(core)++clip.core_frames;
            entry["recording_ordinal"]=entry.at("ordinal");entry["ordinal"]=i-clip.first;
            entry["full_video_time_ns"]=time-origin;entry["clip_time_ns"]=time-base;
            entry["selection_role"]=core?"requested":"context";entry["original_dt_ns"]=entry.at("dt_ns");
            entry["dt_ns"]=previous_clip_time?json(time-previous_clip_time):json(nullptr);
            index<<entry.dump()<<'\n';
            if(previous_clip_time&&time-previous_clip_time>100'000'000)gaps.push_back({
                {"recording_ordinal",i},{"full_video_time_ms",(time-origin)/1e6},{"dt_ms",(time-previous_clip_time)/1e6},
                {"semantics","received-frame delivery gap, not a verified Miss"}});
            previous_clip_time=time;
        }
        index.close();if(!index)throw std::runtime_error("clip index write failed");
        write_json(clip.root/"summary.json",{{"scope","selected received-pixel window; no game-source completeness claim"},
            {"continuous_received_pixels_complete",true},{"frames_written",clip.last-clip.first+1},
            {"index_sha256",pas::sha256_file(clip.root/"index.jsonl")},{"source_index_sha256",recorded.at("index_sha256")},
            {"source_absolute_age",nullptr},{"state_at_clip_begin","unknown; full recording retained for warm-up"}});
        write_json(clip.root/"timing-gaps.json",gaps);
        write_json(clip.root/"notes.json",{{"clip_id",clip.request.at("id")},{"start_ms",clip.request.at("start_ms")},
            {"end_ms",clip.request.at("end_ms")},{"user_note",clip.request.value("user_note",json(nullptr))},
            {"observation",nullptr},{"selection_reason",nullptr},{"annotation_request",json::array()},
            {"hypothesis",nullptr},{"annotation_status","not_started"},{"actual_game_judgment","unknown"},{"human_gold",false}});
        clip.journal.open(clip.root/"journal.jsonl",std::ios::binary);
    }
    std::uint64_t rows=0;
    for(const auto& segment:segments) {
        std::ifstream input(round/safe_relative(segment.at("path").get<std::string>()),std::ios::binary);
        while(std::getline(input,line)) {
            if(++rows>1'000'000||line.size()>2*1024*1024)throw std::invalid_argument("journal row capacity");
            const auto row=json::parse(line);
            for(auto& clip:clips) {
                bool match=false;
                for(const char* key:{"source_frame","frame_sequence"})
                    if(row.contains(key)&&row.at(key).is_number_unsigned()&&clip.sequences.contains(row.at(key).get<std::uint64_t>()))match=true;
                const auto lo=frames[clip.first].at("capture_complete_ns").get<Nanoseconds>();
                const auto hi=frames[clip.last].at("capture_complete_ns").get<Nanoseconds>();
                for(const char* key:{"capture_complete_ns","monotonic_ns","injection_start_ns","injection_return_ns",
                                    "recognition_start_ns","recognition_end_ns","start_ns","return_ns","scheduled_ns"})
                    if(row.contains(key)&&row.at(key).is_number_integer()&&within(row.at(key).get<Nanoseconds>(),lo,hi))match=true;
                if(match) {
                    if(++clip.journal_rows>100000||(clip.journal_bytes+=line.size()+1)>64*1024*1024)
                        throw std::invalid_argument("selected journal capacity");
                    clip.journal<<line<<'\n';
                }
            }
        }
    }
    json reports=json::array();std::vector<std::filesystem::path> result;
    for(auto& clip:clips) {
        clip.journal.close();if(!clip.journal)throw std::runtime_error("selected journal write failed");
        const auto begin=frames[clip.first].at("capture_complete_ns").get<Nanoseconds>();
        auto report=clip.request;
        report["frames"]=clip.last-clip.first+1;report["core_frames"]=clip.core_frames;
        report["recording_ordinal_first"]=clip.first;report["recording_ordinal_last"]=clip.last;
        report["actual_context_start_ms"]=(begin-origin)/1e6;
        report["actual_context_end_ms"]=(frames[clip.last].at("capture_complete_ns").get<Nanoseconds>()-origin)/1e6;
        report["requested_offset_in_clip_ms"]=(origin+clip.request.at("start_ms").get<Nanoseconds>()*1'000'000-begin)/1e6;
        report["journal_rows"]=clip.journal_rows;report["journal_sha256"]=pas::sha256_file(clip.root/"journal.jsonl");
        report["index_sha256"]=pas::sha256_file(clip.root/"index.jsonl");
        report["frame_storage"]="hard links to read-only private selected copies; full-round originals untouched";
        report["journal_filter"]="exact source_frame OR documented top-level host QPC time in context window; original rows preserved";
        report["initial_contact_state"]="unknown";
        write_json(clip.root/"manifest.json",report);reports.push_back(report);result.push_back(clip.root);
    }
    write_json(output/"package-summary.json",{{"selection_sha256",pas::sha256_file(output/"selection-snapshot.json")},
        {"source_index_sha256",recorded.at("index_sha256")},{"time_basis","full-round.mp4 relative capture QPC"},
        {"clips",reports},{"unique_pngs",unique.size()},{"frame_references",references},{"private_png_bytes",source_bytes},
        {"context_policy","requested time +/- 1000ms plus actual boundary support images; points use nearest actual frame"},
        {"reasons_status","awaiting_user"},{"human_gold",0},{"runtime_backend_constructed",false},{"state","packed_previews_pending"}});
    return result;
}
