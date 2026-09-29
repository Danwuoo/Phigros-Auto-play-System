#include "pas/game.hpp"
#include "pas/analysis.hpp"
#include "pas/adb.hpp"
#include "pas/strategy_version.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <fstream>
#include <map>
#include <memory>
#include <numbers>
#include <set>
#include <stdexcept>
#include <tuple>

namespace pas {
namespace {
using nlohmann::json;
struct ClipRecord {
    std::string source,root,family,mode,trigger;
    int round=0,clip=0,max_lines=0,max_targets=0;
    json frames=json::array();
    int selection_rank=0;
    std::string selection_reason;
};
json read_bounded_json(const std::filesystem::path& path) {
    if(std::filesystem::file_size(path)>16*1024*1024)
        throw std::invalid_argument("corpus metadata capacity");
    std::ifstream in(path,std::ios::binary);
    if(!in) throw std::runtime_error("cannot open corpus metadata");
    return json::parse(in);
}
std::map<int,std::pair<std::string,std::string>> round_metadata(const json& data,bool historical) {
    std::map<int,std::pair<std::string,std::string>> result;
    const auto& rows=data.at(historical?"rounds":"results");
    if(!rows.is_array()||rows.size()>32) throw std::invalid_argument("corpus round metadata capacity");
    for(const auto& row:rows) {
        const int id=row.at(historical?"round_id":"round").get<int>();
        const auto song=row.at("song").get<std::string>();
        const auto mode=row.at(historical?"difficulty":"mode").get<std::string>();
        if(id<1||id>32||song.empty()||song.size()>128||(mode!="HD"&&mode!="IN")||
           !result.emplace(id,std::pair{song,mode}).second)
            throw std::invalid_argument("corpus round metadata identity");
    }
    return result;
}
void add_session(std::vector<ClipRecord>& clips,std::set<std::string>& hashes,
    const std::filesystem::path& root,const std::filesystem::path& metadata,
    const std::string& source,bool historical,std::size_t& indexed) {
    const auto session=read_bounded_json(root.parent_path()/"manifest.json");
    const auto& geometry=session.at("capability_preflight").at("capture_geometry");
    if(geometry.at("width")!=1280||geometry.at("height")!=720||
       geometry.at("source_rotation")!=1)
        throw std::invalid_argument("corpus session geometry/rotation mismatch");
    const auto rounds=round_metadata(read_bounded_json(metadata),historical);
    const auto replay=replay_game_pixel_clips(root);
    indexed+=replay.at("indexed_rgb_frames").get<std::size_t>();
    std::map<std::pair<int,int>,std::size_t> by_clip;
    for(const auto& frame:replay.at("frames")) {
        const int round=frame.at("round_id").get<int>(),clip=frame.at("clip_id").get<int>();
        const auto metadata_it=rounds.find(round);
        if(metadata_it==rounds.end()) throw std::invalid_argument("clip has no round metadata");
        const auto key=std::pair{round,clip};
        auto it=by_clip.find(key);
        if(it==by_clip.end()) {
            ClipRecord record;
            record.source=source;record.root=root.generic_string();
            record.family=metadata_it->second.first;record.mode=metadata_it->second.second;
            record.round=round;record.clip=clip;
            record.trigger=frame.at("clip_trigger").get<std::string>();
            if(record.trigger.size()>64) throw std::invalid_argument("clip trigger length");
            clips.push_back(std::move(record));
            it=by_clip.emplace(key,clips.size()-1).first;
        }
        auto& record=clips.at(it->second);
        if(record.frames.size()>=3||record.trigger!=frame.at("clip_trigger").get<std::string>())
            throw std::invalid_argument("clip frame grouping");
        record.max_lines=std::max(record.max_lines,frame.at("recorded_warm_lines").get<int>());
        record.max_targets=std::max(record.max_targets,frame.at("recorded_warm_targets").get<int>());
        record.frames.push_back({{"frame_in_clip",frame.at("frame_in_clip")},
            {"relative_path",frame.at("relative_path")},{"sha256",frame.at("sha256")},
            {"source_frame",frame.at("source_frame")},
            {"source_rotation",frame.at("source_rotation")},
            {"capture_complete_ns",frame.at("capture_complete_ns")},
            {"recorded_warm_lines",frame.at("recorded_warm_lines")},
            {"recorded_warm_targets",frame.at("recorded_warm_targets")}});
        hashes.insert(frame.at("sha256").get<std::string>());
    }
    for(const auto& [key,pos]:by_clip)
        if(clips.at(pos).frames.size()!=3) throw std::invalid_argument("corpus incomplete clip");
}
bool preferred(const ClipRecord& a,const ClipRecord& b) {
    const auto ka=std::tuple{a.trigger=="complex_line_event",a.max_lines,a.max_targets,
        -a.round,-a.clip};
    const auto kb=std::tuple{b.trigger=="complex_line_event",b.max_lines,b.max_targets,
        -b.round,-b.clip};
    return ka>kb;
}
} // namespace

json index_game_pixel_corpus(const std::filesystem::path& old_clips,
    const std::filesystem::path& old_results,const std::filesystem::path& new_clips,
    const std::filesystem::path& new_results) {
    std::vector<ClipRecord> clips;
    std::set<std::string> hashes,families;
    std::size_t indexed=0;
    add_session(clips,hashes,old_clips,old_results,"observer36/planner18",true,indexed);
    add_session(clips,hashes,new_clips,new_results,"observer37/planner19",false,indexed);
    if(clips.size()>400||indexed>1200) throw std::invalid_argument("corpus clip capacity");
    for(const auto& clip:clips) families.insert(clip.family);
    std::vector<std::size_t> chosen;
    const auto choose=[&](auto predicate,const std::string& reason) {
        std::size_t best=clips.size();
        for(std::size_t i=0;i<clips.size();++i) {
            if(clips[i].selection_rank||!predicate(clips[i])) continue;
            if(best==clips.size()||preferred(clips[i],clips[best]))best=i;
        }
        if(best==clips.size())return false;
        auto& clip=clips[best];clip.selection_rank=static_cast<int>(chosen.size()+1);
        clip.selection_reason=reason;chosen.push_back(best);return true;
    };
    // One historical clip per family, then one from every current round.
    // This guarantees family breadth and preserves each current round without
    // using the detector's output as correctness ground truth.
    for(const auto& family:families)choose([&](const ClipRecord& c) {
        return c.family==family&&c.source=="observer36/planner18";
    },"historical_family_coverage");
    std::set<int> new_rounds;
    for(const auto& clip:clips)if(clip.source=="observer37/planner19")new_rounds.insert(clip.round);
    for(int round:new_rounds)choose([&](const ClipRecord& c) {
        return c.source=="observer37/planner19"&&c.round==round;
    },"current_round_coverage");
    if(chosen.size()<30)choose([](const ClipRecord& c){return c.max_targets==0;},"zero_recorded_targets_control");
    if(chosen.size()<30)choose([](const ClipRecord& c){return c.max_lines>=2;},"recorded_multi_line_candidate");
    if(chosen.size()<30)choose([](const ClipRecord& c){return c.max_targets>=3;},"recorded_dense_candidate");
    if(chosen.size()<30)choose([](const ClipRecord& c){return c.trigger=="complex_line_event";},"complex_line_trigger_candidate");
    while(chosen.size()<30&&choose([](const ClipRecord&){return true;},"additional_breadth")){}
    json output_clips=json::array(),selected=json::array(),split_rows=json::array();
    std::size_t old_frames=0,new_frames=0;
    for(const auto& clip:clips) {
        const auto id=clip.source+":"+std::to_string(clip.round)+":"+std::to_string(clip.clip);
        json row={{"clip_id",id},{"source_strategy",clip.source},{"source_root",clip.root},
            {"round_id",clip.round},{"clip_number",clip.clip},{"family",clip.family},
            {"mode",clip.mode},{"clip_trigger",clip.trigger},
            {"recorded_max_lines",clip.max_lines},{"recorded_max_targets",clip.max_targets},
            {"frames",clip.frames},{"selected_rank",clip.selection_rank},
            {"selection_reason",clip.selection_reason},
            {"truth_status","unreviewed_source_pixels"}};
        output_clips.push_back(row);
        if(clip.selection_rank)selected.push_back({{"rank",clip.selection_rank},{"clip_id",id},
            {"family",clip.family},{"mode",clip.mode},{"reason",clip.selection_reason}});
        if(clip.source=="observer36/planner18")old_frames+=clip.frames.size();
        else new_frames+=clip.frames.size();
    }
    std::sort(selected.begin(),selected.end(),[](const json& a,const json& b) {
        return a.at("rank").get<int>()<b.at("rank").get<int>();
    });
    // A frozen future calibration split; every family was already seen by
    // previous development, so this is not an unseen-song test set.
    std::size_t family_index=0;
    for(const auto& family:families) {
        split_rows.push_back({{"family",family},
            {"calibration_split",family_index++%5==0?"future_validation":"fit"},
            {"historically_seen_by_development",true}});
    }
    return {{"schema_version",1},{"corpus_kind","existing_rgb_three_frame_clips"},
        {"old_index_sha256",sha256_file(old_clips/"index.jsonl")},
        {"new_index_sha256",sha256_file(new_clips/"index.jsonl")},
        {"old_session_manifest_sha256",sha256_file(old_clips.parent_path()/"manifest.json")},
        {"new_session_manifest_sha256",sha256_file(new_clips.parent_path()/"manifest.json")},
        {"old_metadata_sha256",sha256_file(old_results)},
        {"new_metadata_sha256",sha256_file(new_results)},
        {"old_indexed_frames",old_frames},{"new_indexed_frames",new_frames},
        {"indexed_rgb_frames",indexed},{"unique_rgb_hashes",hashes.size()},
        {"clip_count",clips.size()},{"family_count",families.size()},
        {"selected_count",selected.size()},{"selected",selected},
        {"family_splits",split_rows},{"clips",output_clips},
        {"all_source_sha256_verified",true},{"human_reviewed_truth",false},
        {"selection_semantics","recorded counts and triggers guide inspection, not truth"},
        {"missing_visual_categories","Hold part, rotation, oblique and multi-line status require source-pixel review"}};
}

namespace {
Frame load_indexed_rgb(const json& clip,const json& indexed_frame) {
    const auto relative=indexed_frame.at("relative_path").get<std::string>();
    const auto relative_path=std::filesystem::path(relative);
    if(relative.empty()||relative_path.is_absolute()||relative.find("..")!=std::string::npos||
       relative_path.extension()!=".rgb")throw std::invalid_argument("corpus RGB path");
    const auto path=std::filesystem::path(clip.at("source_root").get<std::string>())/relative_path;
    constexpr std::size_t bytes=1280ULL*720*3;
    if(std::filesystem::file_size(path)!=bytes||
       sha256_file(path)!=indexed_frame.at("sha256").get<std::string>())
        throw std::invalid_argument("corpus RGB bytes/hash");
    Frame frame;frame.width=1280;frame.height=720;frame.stride=3840;
    frame.source_rotation=1;
    frame.epoch=1;frame.generation=1;frame.geometry_version=1;
    frame.sequence=indexed_frame.at("source_frame").get<std::uint64_t>();
    frame.capture_complete_ns=indexed_frame.at("capture_complete_ns").get<Nanoseconds>();
    frame.rgb.resize(bytes);
    std::ifstream in(path,std::ios::binary);
    if(!in||!in.read(reinterpret_cast<char*>(frame.rgb.data()),bytes))
        throw std::runtime_error("corpus RGB read");
    return frame;
}
void place_thumbnail(Frame& sheet,const Frame& source,int rank) {
    if(rank<1||rank>30||sheet.width!=1600||sheet.height!=1080||
       source.width!=1280||source.height!=720)throw std::invalid_argument("corpus contact sheet geometry");
    const int x0=((rank-1)%5)*320,y0=((rank-1)/5)*180;
    for(int y=0;y<180;++y)for(int x=0;x<320;++x) {
        const auto from=static_cast<std::size_t>(y*4)*source.stride+x*4*3;
        const auto to=static_cast<std::size_t>(y0+y)*sheet.stride+(x0+x)*3;
        for(int channel=0;channel<3;++channel)sheet.rgb[to+channel]=source.rgb[from+channel];
    }
}
Frame empty_contact_sheet() {
    Frame sheet;sheet.width=1600;sheet.height=1080;sheet.stride=4800;
    sheet.rgb.assign(static_cast<std::size_t>(sheet.stride)*sheet.height,0);
    return sheet;
}
bool bright_support(const Frame& frame,int x,int y) {
    if(x<0||y<0||x>=frame.width||y>=frame.height)return false;
    const auto* p=frame.rgb.data()+static_cast<std::size_t>(y)*frame.stride+x*3;
    const int r=p[0],g=p[1],b=p[2];
    const bool neutral=r>115&&g>115&&b>115&&
        std::max({r,g,b})-std::min({r,g,b})<42;
    const bool yellow=r>180&&g>155&&b<g-45;
    const bool blue=b>165&&g>115&&b>r+35;
    return neutral||yellow||blue;
}
json proposed_line_support(const Frame& frame,const DecisionSnapshot& scene) {
    json result=json::array();
    for(std::size_t line_index=0;line_index<scene.lines.size();++line_index) {
        const auto& line=scene.lines[line_index];
        json segments=json::array();
        const Vec2 normal{-line.tangent.y,line.tangent.x};
        const int count=std::min(1024,std::max(0,static_cast<int>(std::ceil(line.length/4))));
        bool active=false;Vec2 begin{},end{};int support_samples=0;
        for(int sample=0;sample<=count;++sample) {
            const double along=(sample*4.0)-line.length/2;
            const Vec2 point{line.center.x+line.tangent.x*along,
                             line.center.y+line.tangent.y*along};
            bool supported=false;
            for(int offset=-3;offset<=3&&!supported;++offset)
                supported=bright_support(frame,static_cast<int>(std::lround(point.x+normal.x*offset)),
                    static_cast<int>(std::lround(point.y+normal.y*offset)));
            if(supported) {
                if(!active){begin=point;support_samples=0;active=true;}
                end=point;++support_samples;
            } else if(active) {
                if(support_samples>=2&&segments.size()<64)segments.push_back({
                    {"x0",begin.x},{"y0",begin.y},{"x1",end.x},{"y1",end.y},
                    {"support_samples",support_samples}});
                active=false;
            }
        }
        if(active&&support_samples>=2&&segments.size()<64)segments.push_back({
            {"x0",begin.x},{"y0",begin.y},{"x1",end.x},{"y1",end.y},
            {"support_samples",support_samples}});
        double angle=std::atan2(line.tangent.y,line.tangent.x);
        if(angle<0)angle+=std::numbers::pi;
        if(angle>=std::numbers::pi)angle-=std::numbers::pi;
        result.push_back({{"proposed_line_id",line.track_id},
            {"diagnostic_line_index",line_index},{"orientation_mod_pi_rad",angle},
            {"visible_support_segments",segments},
            {"segment_cap_reached",segments.size()==64},
            {"support_method","local_bright_pixel_samples_4px_along_7px_normal"},
            {"trust","proposed"}});
    }
    return result;
}
json proposed_note_parts(const DecisionSnapshot& scene,int frame_in_clip) {
    json result=json::array();
    std::set<std::uint64_t> current_lines;
    for(const auto& line:scene.lines)if(line.track_id)current_lines.insert(line.track_id);
    for(const auto& target:scene.targets) {
        const bool hold=target.note.kind==NoteKind::hold;
        const bool body=hold&&(target.note.rails_geometry||target.note.held_body_evidence);
        const bool tail=hold&&target.note.tail.has_value();
        const bool relation_known=target.line_id&&current_lines.contains(target.line_id);
        result.push_back({{"proposed_note_id",target.note_id},
            {"diagnostic_revision",target.revision},{"type",name(target.note.kind)},
            {"head_keypoint",hold&&target.note.head_on_line?
                json{{"x",target.note.center.x},{"y",target.note.center.y}}:json(nullptr)},
            {"head_visibility",hold&&target.note.head_on_line?"proposed_visible":"unknown"},
            {"body_visibility",body?"proposed_visible":"unknown"},
            {"body_mask",nullptr},{"body_box_is_visible_mask",false},
            {"tail_keypoint",tail?json{{"x",target.note.tail->x},{"y",target.note.tail->y}}:json(nullptr)},
            {"tail_visibility",tail?"proposed_visible":"unknown"},
            {"relation_line_id",relation_known?json(target.line_id):json(nullptr)},
            {"relation_status",relation_known?"proposed_unique":"unknown"},
            {"relation_evidence_frame_in_clip",frame_in_clip},
            {"trust","proposed"}});
    }
    return result;
}
} // namespace

json write_game_corpus_proposals(const std::filesystem::path& corpus_index,
    const std::filesystem::path& output_directory) {
    const auto algorithm="observer"+std::to_string(game_observer_version)+
        "_cold_start_three_frame_clip";
    const auto corpus=read_bounded_json(corpus_index);
    if(corpus.at("schema_version")!=1||corpus.at("selected_count").get<int>()<1||
       corpus.at("selected_count").get<int>()>30||
       corpus.at("selected").size()!=corpus.at("selected_count").get<std::size_t>()||
       corpus.at("clips").size()>400)
        throw std::invalid_argument("corpus selection capacity");
    if(std::filesystem::exists(output_directory/"proposals.json"))
        throw std::invalid_argument("refusing to overwrite corpus proposals");
    std::filesystem::create_directories(output_directory);
    auto source_sheet=empty_contact_sheet(),proposed_sheet=empty_contact_sheet();
    json proposals=json::array(),mapping=json::array();
    for(const auto& selection:corpus.at("selected")) {
        const int rank=selection.at("rank").get<int>();
        const auto id=selection.at("clip_id").get<std::string>();
        if(rank<1||rank>30||rank!=static_cast<int>(mapping.size()+1))
            throw std::invalid_argument("corpus selected rank");
        const json* clip=nullptr;
        for(const auto& row:corpus.at("clips"))
            if(row.at("clip_id")==id) {if(clip)throw std::invalid_argument("duplicate corpus clip ID");clip=&row;}
        if(!clip||clip->at("selected_rank")!=rank||clip->at("frames").size()!=3)
            throw std::invalid_argument("corpus selected clip reference");
        FakeClock clock;GameObserver observer(clock);
        for(int i=0;i<3;++i) {
            const auto& indexed_frame=clip->at("frames").at(i);
            if(indexed_frame.at("frame_in_clip")!=i)
                throw std::invalid_argument("corpus selected frame order");
            auto frame=load_indexed_rgb(*clip,indexed_frame);
            clock.set(frame.capture_complete_ns);
            const auto scene=observer.process(frame);
            proposals.push_back({{"clip_id",id},{"rank",rank},{"frame_in_clip",i},
                {"relative_path",indexed_frame.at("relative_path")},
                {"source_sha256",indexed_frame.at("sha256")},
                {"source_frame",frame.sequence},{"capture_complete_ns",frame.capture_complete_ns},
                {"source_rotation",frame.source_rotation},
                {"crop_transform","full_1280x720_identity"},
                {"trust","proposed"},{"algorithm",algorithm},
                {"line_instances",proposed_line_support(frame,scene)},
                {"note_instances",proposed_note_parts(scene,i)},
                {"decision",decision_json(scene)}});
            if(i==1) {
                const auto suffix=std::to_string(rank)+".png";
                write_diagnostic_png(output_directory/("source-"+suffix),frame);
                place_thumbnail(source_sheet,frame,rank);
                draw_game_overlay(frame,scene);
                write_diagnostic_png(output_directory/("proposed-"+suffix),frame);
                place_thumbnail(proposed_sheet,frame,rank);
            }
        }
        mapping.push_back({{"rank",rank},{"clip_id",id},
            {"family",clip->at("family")},{"mode",clip->at("mode")},
            {"source_png","source-"+std::to_string(rank)+".png"},
            {"proposed_png","proposed-"+std::to_string(rank)+".png"}});
    }
    write_diagnostic_png(output_directory/"source-contact-sheet.png",source_sheet);
    write_diagnostic_png(output_directory/"proposed-contact-sheet.png",proposed_sheet);
    const json manifest={{"schema_version",1},{"corpus_index_sha256",sha256_file(corpus_index)},
        {"proposal_algorithm",algorithm},
        {"trust","proposed"},{"human_reviewed_truth",false},{"gold_eligible",false},
        {"selected_clips",mapping},{"proposal_frames",proposals},
        {"source_contact_sheet","source-contact-sheet.png"},
        {"proposed_contact_sheet","proposed-contact-sheet.png"},
        {"interpretation","Source pixels and machine proposals only; unmarked pixels are unknown, not negative background"}};
    std::ofstream out(output_directory/"proposals.json",std::ios::binary);
    if(!out||!(out<<manifest.dump(2)<<'\n'))throw std::runtime_error("corpus proposal manifest write");
    return {{"selected_clips",mapping.size()},{"proposal_frames",proposals.size()},
        {"manifest_path",(output_directory/"proposals.json").generic_string()},
        {"manifest_sha256",sha256_file(output_directory/"proposals.json")},
        {"human_reviewed_truth",false},{"gold_eligible",false}};
}

json validate_game_corpus_proposals(const std::filesystem::path& corpus_index,
    const std::filesystem::path& proposals_manifest) {
    const auto corpus=read_bounded_json(corpus_index),manifest=read_bounded_json(proposals_manifest);
    if(corpus.at("schema_version")!=1||manifest.at("schema_version")!=1||
       manifest.at("corpus_index_sha256")!=sha256_file(corpus_index)||
       manifest.at("trust")!="proposed"||
       manifest.at("human_reviewed_truth")!=false||manifest.at("gold_eligible")!=false||
       manifest.at("selected_clips").size()!=corpus.at("selected_count").get<std::size_t>()||
       manifest.at("proposal_frames").size()!=corpus.at("selected_count").get<std::size_t>()*3)
        throw std::invalid_argument("corpus proposal trust/reference contract");
    std::map<std::string,const json*> selected;
    for(const auto& clip:corpus.at("clips"))if(clip.at("selected_rank").get<int>()>0) {
        const auto id=clip.at("clip_id").get<std::string>();
        if(!selected.emplace(id,&clip).second)throw std::invalid_argument("corpus selected ID duplicate");
    }
    std::set<std::pair<std::string,int>> seen;
    for(const auto& proposal:manifest.at("proposal_frames")) {
        const auto id=proposal.at("clip_id").get<std::string>();
        const int i=proposal.at("frame_in_clip").get<int>();
        const auto it=selected.find(id);
        if(it==selected.end()||i<0||i>2||!seen.emplace(id,i).second||
           proposal.at("trust")!="proposed")
            throw std::invalid_argument("corpus proposal frame identity");
        const auto& clip=*it->second;
        const auto& indexed=clip.at("frames").at(i);
        if(proposal.at("rank")!=clip.at("selected_rank")||
           proposal.at("source_sha256")!=indexed.at("sha256")||
           proposal.at("relative_path")!=indexed.at("relative_path")||
           proposal.at("source_frame")!=indexed.at("source_frame")||
           proposal.at("source_rotation")!=indexed.at("source_rotation")||
           proposal.at("crop_transform")!="full_1280x720_identity"||
           proposal.at("capture_complete_ns")!=indexed.at("capture_complete_ns"))
            throw std::invalid_argument("corpus proposal source mismatch");
        const auto& decision=proposal.at("decision");
        if(decision.at("frame_sequence")!=indexed.at("source_frame")||
           decision.at("lines").size()>16||decision.at("targets").size()>128)
            throw std::invalid_argument("corpus proposal decision capacity");
        const auto& line_instances=proposal.at("line_instances");
        const auto& note_instances=proposal.at("note_instances");
        if(line_instances.size()!=decision.at("lines").size()||
           note_instances.size()!=decision.at("targets").size())
            throw std::invalid_argument("corpus proposal instance count");
        std::set<std::uint64_t> proposed_line_ids;
        for(const auto& line:line_instances) {
            if(line.at("trust")!="proposed"||
               line.at("visible_support_segments").size()>64)
                throw std::invalid_argument("corpus line proposal trust/capacity");
            const auto id=line.at("proposed_line_id").get<std::uint64_t>();
            if(id)proposed_line_ids.insert(id);
            const double angle=line.at("orientation_mod_pi_rad").get<double>();
            if(!std::isfinite(angle)||angle<0||angle>=std::numbers::pi)
                throw std::invalid_argument("corpus line proposal angle");
            for(const auto& segment:line.at("visible_support_segments")) {
                for(const char* field:{"x0","x1","y0","y1"}) {
                    const double value=segment.at(field).get<double>();
                    const bool horizontal=field[0]=='x';
                    if(!std::isfinite(value)||value< -4||value>(horizontal?1284:724))
                        throw std::invalid_argument("corpus line support bounds");
                }
                if(segment.at("support_samples").get<int>()<2)
                    throw std::invalid_argument("corpus line support samples");
            }
        }
        for(const auto& note:note_instances) {
            if(note.at("trust")!="proposed"||note.at("body_mask")!=nullptr||
               note.at("body_box_is_visible_mask")!=false)
                throw std::invalid_argument("corpus note part trust/mask");
            for(const char* field:{"head_visibility","body_visibility","tail_visibility"}) {
                const auto visibility=note.at(field).get<std::string>();
                if(visibility!="proposed_visible"&&visibility!="unknown")
                    throw std::invalid_argument("corpus note part visibility");
            }
            if(note.at("relation_status")=="proposed_unique") {
                if(!proposed_line_ids.contains(note.at("relation_line_id").get<std::uint64_t>()))
                    throw std::invalid_argument("corpus relation line reference");
            } else if(note.at("relation_status")!="unknown"||note.at("relation_line_id")!=nullptr)
                throw std::invalid_argument("corpus relation unknown reference");
        }
        const auto relative=indexed.at("relative_path").get<std::string>();
        const auto path=std::filesystem::path(clip.at("source_root").get<std::string>())/relative;
        if(sha256_file(path)!=indexed.at("sha256").get<std::string>())
            throw std::invalid_argument("corpus proposal source hash changed");
    }
    return {{"valid",true},{"source_frames_verified",seen.size()},
        {"selected_clips",selected.size()},{"trust","proposed"},
        {"human_reviewed_truth",false},{"gold_eligible",false}};
}

json validate_game_cold_coverage_manifest(const std::filesystem::path& manifest_path) {
    const auto manifest=read_bounded_json(manifest_path);
    if(manifest.at("schema_version")!=1||!manifest.at("cases").is_array()||
       manifest.at("cases").size()<32||manifest.at("cases").size()>256)
        throw std::invalid_argument("cold coverage schema/capacity");
    constexpr std::array<const char*,9> groups{"G1","G2","G3","N1","N2","H1","H2","T1","T2"};
    constexpr std::array<const char*,7> risks{"R1","R2","R3","R4","R5","R6","R7"};
    std::set<std::string> expected,seen;
    for(const auto* group:groups)for(const auto* variant:{"normal","negative"})
        expected.insert(std::string(group)+"-"+variant);
    for(const auto* risk:risks)for(const auto* variant:{"normal","negative"})
        expected.insert(std::string(risk)+"-"+variant);
    std::size_t passed=0,pending=0,failed=0,unknown=0;
    for(const auto& row:manifest.at("cases")) {
        const auto id=row.at("case_id").get<std::string>();
        const auto variant=row.at("variant").get<std::string>();
        const auto status=row.at("status").get<std::string>();
        if(!expected.contains(id)||!seen.insert(id).second||
           (variant!="normal"&&variant!="negative")||
           id.substr(id.find('-')+1)!=variant||
           row.at("form_item").get<std::string>().empty()||
           row.at("expected_result").get<std::string>().empty()||
           !row.at("required_layers").is_array()||!row.at("verified_layers").is_array())
            throw std::invalid_argument("cold coverage case identity/contract");
        const auto stem=id.substr(0,id.find('-'));
        if((stem[0]=='R'&&(row.at("risk_combo")!=stem||row.at("matrix_group")!=nullptr))||
           (stem[0]!='R'&&(row.at("matrix_group")!=stem||row.at("risk_combo")!=nullptr)))
            throw std::invalid_argument("cold coverage group/risk mapping");
        std::set<std::string> required,verified;
        for(const auto& layer:row.at("required_layers")) {
            const auto name=layer.get<std::string>();
            if(name!="RGB"&&name!="oracle"&&name!="fake-clock")
                throw std::invalid_argument("cold coverage layer name");
            required.insert(name);
        }
        for(const auto& layer:row.at("verified_layers")) {
            const auto name=layer.get<std::string>();
            if(!required.contains(name)||!verified.insert(name).second)
                throw std::invalid_argument("cold coverage verified layer");
        }
        if(required.empty())throw std::invalid_argument("cold coverage missing layers");
        if(status=="pending")++pending;
        else if(status=="failed")++failed;
        else if(status=="unknown")++unknown;
        else if(status=="passed") {
            ++passed;
            const auto truth=row.at("truth_basis").get<std::string>();
            if((truth!="independent_synthetic"&&truth!="human_reviewed")||
               verified!=required||
               (row.at("input_sha256").is_null()&&row.at("seed").is_null())||
               row.at("test_name").get<std::string>().empty()||
               row.at("result_path").get<std::string>().empty())
                throw std::invalid_argument("cold coverage passed evidence incomplete");
            const auto result=row.at("result_path").get<std::string>();
            const auto relative=std::filesystem::path(result);
            if(relative.is_absolute()||result.find("..")!=std::string::npos||
               !std::filesystem::is_regular_file(manifest_path.parent_path()/relative))
                throw std::invalid_argument("cold coverage result path");
        } else throw std::invalid_argument("cold coverage status");
    }
    if(seen!=expected||manifest.at("case_count")!=seen.size())
        throw std::invalid_argument("cold coverage matrix incomplete");
    return {{"valid",true},{"required_cases",expected.size()},
        {"passed",passed},{"pending",pending},{"failed",failed},{"unknown",unknown},
        {"complete",passed==expected.size()}};
}

json compare_game_pixel_replays(const std::filesystem::path& corpus_index,
    const std::filesystem::path& before_old,const std::filesystem::path& after_old,
    const std::filesystem::path& before_new,const std::filesystem::path& after_new) {
    const auto corpus=read_bounded_json(corpus_index);
    std::map<std::string,std::string> family_splits;
    for(const auto& item:corpus.value("family_splits",json::array())) {
        const auto family=item.at("family").get<std::string>();
        const auto split=item.at("calibration_split").get<std::string>();
        if((split!="fit"&&split!="future_validation")||
           !family_splits.emplace(family,split).second)
            throw std::invalid_argument("replay comparison family split");
    }
    using Key=std::tuple<std::string,int,int,int>;
    std::map<Key,std::pair<std::string,std::string>> expected;
    for(const auto& clip:corpus.at("clips")) {
        const auto source=clip.at("source_strategy").get<std::string>();
        const int round=clip.at("round_id").get<int>(),number=clip.at("clip_number").get<int>();
        const auto family=clip.at("family").get<std::string>();
        for(const auto& frame:clip.at("frames")) {
            const Key key{source,round,number,frame.at("frame_in_clip").get<int>()};
            if(!expected.emplace(key,std::pair{family,frame.at("sha256").get<std::string>()}).second)
                throw std::invalid_argument("replay comparison duplicate corpus frame");
        }
    }
    struct Counts {int frames=0,line_changes=0,target_changes=0,semantic_changes=0,
        line_delta=0,target_delta=0;};
    std::map<std::string,Counts> by_family;
    json differences=json::array();
    json semantic_differences=json::array();
    std::size_t compared=0;
    std::size_t semantic_compared=0;
    const auto compare=[&](const std::filesystem::path& before_path,
                           const std::filesystem::path& after_path,const std::string& source) {
        const auto before=read_bounded_json(before_path),after=read_bounded_json(after_path);
        if(before.at("input_integrity_verified")!=true||after.at("input_integrity_verified")!=true||
           !before.at("frames").is_array()||before.at("frames").size()!=after.at("frames").size())
            throw std::invalid_argument("replay comparison input integrity/count");
        for(std::size_t i=0;i<before.at("frames").size();++i) {
            const auto& b=before.at("frames").at(i),&a=after.at("frames").at(i);
            const int round=b.at("round_id").get<int>(),clip=b.at("clip_id").get<int>();
            const int frame=b.at("frame_in_clip").get<int>();
            const Key key{source,round,clip,frame};
            const auto it=expected.find(key);
            if(it==expected.end()||b.at("sha256")!=it->second.second||
               a.at("sha256")!=it->second.second||
               a.at("round_id")!=round||a.at("clip_id")!=clip||
               a.at("frame_in_clip")!=frame||a.at("source_frame")!=b.at("source_frame"))
                throw std::invalid_argument("replay comparison source frame mismatch");
            const int before_lines=b.at("cold_lines").get<int>();
            const int after_lines=a.at("cold_lines").get<int>();
            const int before_targets=b.at("cold_targets").get<int>();
            const int after_targets=a.at("cold_targets").get<int>();
            if(before_lines<0||after_lines<0||before_lines>16||after_lines>16||
               before_targets<0||after_targets<0||before_targets>128||after_targets>128)
                throw std::invalid_argument("replay comparison capacity");
            auto& counts=by_family[it->second.first];++counts.frames;
            counts.line_delta+=after_lines-before_lines;
            counts.target_delta+=after_targets-before_targets;
            if(before_lines!=after_lines)++counts.line_changes;
            if(before_targets!=after_targets)++counts.target_changes;
            if(before_lines!=after_lines||before_targets!=after_targets)
                differences.push_back({{"source_strategy",source},{"family",it->second.first},
                    {"round_id",round},{"clip_id",clip},{"frame_in_clip",frame},
                    {"sha256",it->second.second},{"before_lines",before_lines},
                    {"after_lines",after_lines},{"before_targets",before_targets},
                    {"after_targets",after_targets}});
            if(b.contains("semantic_decision")!=a.contains("semantic_decision"))
                throw std::invalid_argument("replay comparison semantic availability mismatch");
            if(b.contains("semantic_decision")) {
                ++semantic_compared;
                const auto& before_semantic=b.at("semantic_decision");
                const auto& after_semantic=a.at("semantic_decision");
                if(!before_semantic.is_object()||!after_semantic.is_object())
                    throw std::invalid_argument("replay comparison semantic decision shape");
                if(before_semantic!=after_semantic) {
                    ++counts.semantic_changes;
                    json fields=json::array();
                    for(auto item=before_semantic.begin();item!=before_semantic.end();++item)
                        if(!after_semantic.contains(item.key())||after_semantic.at(item.key())!=item.value())
                            fields.push_back(item.key());
                    for(auto item=after_semantic.begin();item!=after_semantic.end();++item)
                        if(!before_semantic.contains(item.key()))fields.push_back(item.key());
                    semantic_differences.push_back({{"source_strategy",source},
                        {"family",it->second.first},{"round_id",round},{"clip_id",clip},
                        {"frame_in_clip",frame},{"sha256",it->second.second},
                        {"changed_fields",std::move(fields)},
                        {"before_targets",before_semantic.value("targets",json::array())},
                        {"after_targets",after_semantic.value("targets",json::array())}});
                }
            }
            ++compared;
        }
    };
    compare(before_old,after_old,"observer36/planner18");
    compare(before_new,after_new,"observer37/planner19");
    if(compared!=expected.size()||compared!=corpus.at("indexed_rgb_frames").get<std::size_t>())
        throw std::invalid_argument("replay comparison corpus coverage");
    json family_rows=json::array();
    for(const auto& [family,c]:by_family)
        family_rows.push_back({{"family",family},
            {"calibration_split",family_splits.contains(family)?
                family_splits.at(family):"not_frozen"},
            {"indexed_frames",c.frames},
            {"line_count_changed_frames",c.line_changes},
            {"target_count_changed_frames",c.target_changes},
            {"semantic_changed_frames",c.semantic_changes},
            {"line_count_delta",c.line_delta},{"target_count_delta",c.target_delta}});
    return {{"schema_version",1},{"comparison","verified_frozen_vs_candidate_cold_clip_counts"},
        {"corpus_index_sha256",sha256_file(corpus_index)},
        {"before_old_sha256",sha256_file(before_old)},{"after_old_sha256",sha256_file(after_old)},
        {"before_new_sha256",sha256_file(before_new)},{"after_new_sha256",sha256_file(after_new)},
        {"indexed_frames_compared",compared},{"unique_rgb_hashes",corpus.at("unique_rgb_hashes")},
        {"changed_frame_count",differences.size()},{"changes",differences},
        {"semantic_frames_compared",semantic_compared},
        {"semantic_changed_frame_count",semantic_differences.size()},
        {"semantic_changes",semantic_differences},
        {"by_family",family_rows},{"ground_truth_comparison",false},
        {"interpretation","Cold observer candidate and complete decision differences; no real pixel truth or game effect"}};
}
} // namespace pas
