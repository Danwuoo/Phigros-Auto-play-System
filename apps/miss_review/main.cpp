#include "pas/game.hpp"
#include "pas/adb.hpp"
#include "pas/analysis.hpp"
#include <algorithm>
#include <cmath>
#include <fstream>
#include <iostream>
#include <limits>
#include <set>

// Offline evidence packaging only. No emulator, touch owner or action policy.
// A telemetry trigger never becomes an actual game Miss or human annotation.
namespace {
using namespace pas;
using json = nlohmann::json;
constexpr std::size_t max_frames = 64, max_incidents = 2048;
constexpr Nanoseconds nearby_ns = 500'000'000;
json load_json(const std::filesystem::path& path) {
    if (std::filesystem::file_size(path) > 2 * 1024 * 1024)
        throw std::invalid_argument("JSON byte capacity");
    std::ifstream file(path, std::ios::binary);
    return json::parse(file);
}
std::filesystem::path relative_path(const std::string& value) {
    const std::filesystem::path path(value);
    if (path.empty() || path.is_absolute() || path.has_root_name())
        throw std::invalid_argument("absolute/empty evidence path");
    for (const auto& part : path) if (part == "..")
        throw std::invalid_argument("escaping evidence path");
    return path;
}
void write_json(const std::filesystem::path& path, const json& value) {
    std::ofstream file(path, std::ios::binary);
    file << value.dump(2) << '\n';
    if (!file) throw std::runtime_error("report write failed");
}
std::string escaped(const std::string& value) {
    std::string result;
    for (char c : value) {
        switch (c) {
        case '&': result += "&amp;"; break;
        case '<': result += "&lt;"; break;
        case '>': result += "&gt;"; break;
        case '"': result += "&quot;"; break;
        default: result += c;
        }
    }
    return result;
}
struct SavedFrame {
    json index, scene;
    std::filesystem::path path;
    std::uint64_t sequence = 0;
    Nanoseconds time = 0;
};
json slim_target(const json& target) {
    json result = json::object();
    for (const char* key : {"note_id", "kind", "x", "y", "width", "height",
         "line_id", "reason", "crossing_ns", "head_on_line", "held_body_evidence"})
        if (target.contains(key)) result[key] = target.at(key);
    return result;
}
void recorded_overlay(Frame& frame, const json& scene) {
    const auto pixel = [&](int x, int y, std::array<std::uint8_t,3> color) {
        if (x >= 0 && y >= 0 && x < frame.width && y < frame.height)
            std::copy(color.begin(), color.end(), frame.rgb.begin() + y * frame.stride + x * 3);
    };
    const auto line = [&](double ax, double ay, double bx, double by,
                          std::array<std::uint8_t,3> color) {
        // Clip extreme journal coordinates; metadata remains available verbatim.
        if (!std::isfinite(ax) || !std::isfinite(ay) || !std::isfinite(bx) || !std::isfinite(by) ||
            std::max({std::abs(ax),std::abs(ay),std::abs(bx),std::abs(by)}) > 8192) return;
        const int n = std::max(1, static_cast<int>(std::ceil(std::max(std::abs(bx-ax),std::abs(by-ay)))));
        for (int i = 0; i <= n; ++i) {
            const double u = static_cast<double>(i) / n;
            pixel(static_cast<int>(std::lround(ax+(bx-ax)*u)),
                  static_cast<int>(std::lround(ay+(by-ay)*u)), color);
        }
    };
    if (scene.is_null()) return;
    for (const auto& candidate : scene.at("lines")) {
        const double x=candidate.at("x"), y=candidate.at("y"),
            dx=candidate.at("ux").get<double>()*candidate.at("length").get<double>()/2,
            dy=candidate.at("uy").get<double>()*candidate.at("length").get<double>()/2;
        line(x-dx,y-dy,x+dx,y+dy,candidate.value("association_valid",false)
            ? std::array<std::uint8_t,3>{0,255,255} : std::array<std::uint8_t,3>{255,128,0});
    }
    for (const auto& target : scene.at("targets")) {
        const double x=target.at("x"), y=target.at("y");
        line(x-8,y,x+8,y,{255,0,255}); line(x,y-8,x,y+8,{255,0,255});
        if (target.value("line_id",std::uint64_t{0}) && target.contains("hit_x") && target.contains("hit_y")) {
            const double hx=target.at("hit_x"), hy=target.at("hit_y");
            line(hx-5,hy-5,hx+5,hy+5,{0,255,0}); line(hx-5,hy+5,hx+5,hy-5,{0,255,0});
        }
    }
}
int review(const std::filesystem::path& round_root, const std::filesystem::path& clip_root,
           const std::filesystem::path& output) {
    if (std::filesystem::exists(output)) throw std::invalid_argument("output already exists");
    const auto summary=load_json(round_root/"summary.json");
    const int round=summary.at("round_id");
    const auto start=summary.at("round_start_ns").get<Nanoseconds>();
    const auto& segments=summary.at("event_segments");
    if (!segments.is_array() || segments.empty() || segments.size()>32)
        throw std::invalid_argument("segment capacity");
    json verified=json::array(); std::size_t segment_index=0;
    for (const auto& segment:segments) {
        const auto relative=relative_path(segment.at("path").get<std::string>());
        if (relative != "events-"+std::to_string(segment_index++)+".jsonl" ||
            std::filesystem::file_size(round_root/relative)>32*1024*1024 ||
            sha256_file(round_root/relative)!=segment.at("sha256").get<std::string>())
            throw std::invalid_argument("segment order/bytes/SHA mismatch");
        verified.push_back(segment);
    }
    if (std::filesystem::file_size(clip_root/"index.jsonl")>2*1024*1024)
        throw std::invalid_argument("index capacity");
    std::vector<SavedFrame> frames;
    std::ifstream index(clip_root/"index.jsonl",std::ios::binary);
    std::string row; std::size_t index_rows=0;
    while (std::getline(index,row)) {
        if (++index_rows>2048 || row.size()>64*1024) throw std::invalid_argument("index row capacity");
        if (row.empty()) continue;
        auto entry=json::parse(row);
        const auto relative=relative_path(entry.at("path").get<std::string>());
        if (entry.at("round_id")!=round) continue;
        if (frames.size()>=max_frames || entry.at("width")!=1280 || entry.at("height")!=720 ||
            entry.at("stride")!=3840 || entry.at("layout")!="top_down_rgb888")
            throw std::invalid_argument("frame capacity/layout");
        const auto path=clip_root/relative;
        if (std::filesystem::file_size(path)!=2764800 || sha256_file(path)!=entry.at("sha256").get<std::string>())
            throw std::invalid_argument("RGB bytes/SHA mismatch");
        SavedFrame saved; saved.sequence=entry.at("source_frame");saved.time=entry.at("capture_complete_ns");
        saved.index=std::move(entry);saved.path=path;frames.push_back(std::move(saved));
    }
    if (!index.eof() || frames.empty()) throw std::invalid_argument("missing/empty clip index");
    std::sort(frames.begin(),frames.end(),[](const auto& a,const auto& b){return a.time<b.time;});
    std::map<std::uint64_t,std::size_t> lookup;
    for (std::size_t i=0;i<frames.size();++i) {
        if (!lookup.emplace(frames[i].sequence,i).second || (i&&frames[i].time<=frames[i-1].time))
            throw std::invalid_argument("duplicate/unordered saved frame");
    }
    ComboVisibilityDiagnostic combo;
    json latest,incidents=json::array();
    std::map<std::string,std::uint64_t> triggers,cancellations;
    std::uint64_t rows=0,decisions=0,skipped_contact_before_down=0;
    const auto add=[&](const std::string& trigger,Nanoseconds time,const json& evidence) {
        if (incidents.size()>=max_incidents) throw std::invalid_argument("incident capacity");
        ++triggers[trigger];
        json current={{"source_frame",nullptr},{"capture_complete_ns",nullptr},
            {"line_count",0},{"target_count",0},{"related_target",nullptr}};
        if (!latest.is_null()) {
            current["source_frame"]=latest.at("frame_sequence");
            current["capture_complete_ns"]=latest.at("capture_complete_ns");
            current["line_count"]=latest.at("lines").size(); current["target_count"]=latest.at("targets").size();
            const auto note=evidence.value("note_id",std::uint64_t{0});
            for (const auto& target:latest.at("targets"))
                if (note && target.value("note_id",std::uint64_t{0})==note)
                    current["related_target"]=slim_target(target);
        }
        incidents.push_back({{"id",incidents.size()+1},{"trigger",trigger},{"host_qpc_ns",time},
            {"relative_ms",(time-start)/1e6},{"evidence",evidence},{"current_journal_context",current},
            {"game_judgment","unknown"},{"actual_miss_confirmed",false},{"human_gold",false}});
    };
    for (const auto& segment:segments) {
        std::ifstream journal(round_root/segment.at("path").get<std::string>(),std::ios::binary);
        while (std::getline(journal,row)) {
            if (++rows>1'000'000 || row.size()>2*1024*1024) throw std::invalid_argument("journal capacity");
            if (row.empty()) continue;
            auto event=json::parse(row);const auto type=event.value("event","");
            if (event.value("round_id",round)!=round) throw std::invalid_argument("journal round mismatch");
            if (type=="game_decision") {
                if (++decisions>100000 || event.at("lines").size()>16 || event.at("targets").size()>128)
                    throw std::invalid_argument("decision capacity");
                const auto frame=event.at("frame_sequence").get<std::uint64_t>();
                latest=std::move(event);
                if (const auto found=lookup.find(frame);found!=lookup.end()) {
                    auto& saved=frames[found->second];
                    if (!saved.scene.is_null() || latest.at("capture_complete_ns")!=saved.time)
                        throw std::invalid_argument("pixel/journal join mismatch");
                    saved.scene=latest;
                }
                DecisionSnapshot snapshot;
                snapshot.context={latest.at("epoch"),latest.at("generation"),latest.at("geometry_version"),
                    frame,latest.at("capture_complete_ns"),1280,720,1};
                snapshot.playing_gate=latest.at("playing_gate");snapshot.capacity_valid=latest.at("capacity_valid");
                snapshot.combo_digit_glyphs=latest.at("combo_digit_glyphs");
                if (combo.observe(snapshot)) add("combo_visibility_loss",snapshot.context.capture_ns,
                    {{"event","offline_combo_visibility_loss"},{"source_frame",frame},
                     {"semantics","glyph shape disappearance only; no numeric OCR or game Miss judgment"}});
            } else if (type=="game_contact_cancelled") {
                ++cancellations[event.value("kind","unknown")+"/"+event.value("reason","unknown")];
                if (event.value("contact_started",false)) add("active_contact_cancelled",event.at("cancel_ns"),event);
                else ++skipped_contact_before_down;
            } else if (type=="game_contact_up" && event.value("kind","")=="hold" &&
                       event.value("reason","")=="target_evidence_expired")
                add("hold_evidence_expired",event.at("injection_return_ns"),event);
        }
        if (!journal.eof()) throw std::runtime_error("journal read failed");
    }
    std::size_t nearby=0,exact=0;
    for (auto& incident:incidents) {
        const auto time=incident.at("host_qpc_ns").get<Nanoseconds>();
        const auto nearest=std::min_element(frames.begin(),frames.end(),[&](const auto& a,const auto& b) {
            return std::abs(a.time-time)<std::abs(b.time-time);
        });
        const auto distance=std::abs(nearest->time-time);
        incident["nearest_saved_frame"]=nearest->sequence;
        incident["nearest_saved_delta_ms"]=(nearest->time-time)/1e6;
        incident["saved_frames_within_500ms"]=json::array();
        for (const auto& saved:frames) if (std::abs(saved.time-time)<=nearby_ns)
            incident["saved_frames_within_500ms"].push_back(saved.sequence);
        const bool available=distance<=nearby_ns;
        incident["pixel_window_status"]=available?"sparse_frame_near_event_incomplete_window":"no_pixels_within_500ms";
        if (available) ++nearby;
        const auto source=incident.at("evidence").value("source_frame",std::uint64_t{0});
        incident["event_source_frame_saved"]=lookup.contains(source);
        if (lookup.contains(source)) ++exact;
    }
    std::filesystem::create_directories(output/"frames");
    json annotations=json::array(),clips=json::array();
    std::ofstream html(output/"review.html",std::ios::binary);
    html << "<!doctype html><meta charset=utf-8><title>Miss 候選影格覆核</title>"
        "<style>body{font:16px system-ui;background:#141820;color:#eee;margin:24px}a{color:#7ddfff}"
        "section{margin:24px 0;padding:14px;background:#202632}figure{margin:0}img{width:100%}"
        ".grid{display:grid;grid-template-columns:repeat(3,minmax(0,1fr));gap:10px}"
        "pre{white-space:pre-wrap;max-height:180px;overflow:auto;font-size:12px}</style>"
        "<h1>Miss 候選影格覆核</h1><p>原圖與當時 journal 的偵測候選。青色＝association valid 線候選，"
        "橘色＝invalid 線候選，紫色十字＝Note 候選，綠色叉＝候選投影落點（不代表實際觸控）。顏色不代表人工確認。</p>"
        "<p>" << frames.size() << " 張原始 RGB；" << incidents.size() << " 個診斷候選事件中 " << nearby
        << " 個在 ±500ms 內有稀疏原圖。此距離只作覆核索引，不能證明是同一個 Miss。"
        "所有逐 Note 判定、真／假線與關聯標籤初始為 unknown；人工 gold 為 0。</p>"
        "<p><a href='summary.json'>證據摘要</a> · <a href='incidents.json'>事件與缺圖列表</a> · "
        "<a href='annotation-template.json'>待覆核標註 JSON</a> · <a href='contact-sheet.png'>原圖總覽</a></p>";
    int prior_clip=-1; Nanoseconds clip_start=0,clip_end=0; json current_clip;
    Frame sheet;sheet.width=1280;sheet.height=static_cast<int>((frames.size()+2)/3)*240;
    sheet.stride=sheet.width*3;sheet.rgb.resize(static_cast<std::size_t>(sheet.stride)*sheet.height);
    for (std::size_t i=0;i<frames.size();++i) {
        const auto& saved=frames[i];const int clip=saved.index.at("clip_id");
        if (clip!=prior_clip) {
            if (prior_clip!=-1) {html << "</div></section>";current_clip["span_ms"]=(clip_end-clip_start)/1e6;clips.push_back(current_clip);}
            clip_start=saved.time;prior_clip=clip; current_clip={{"clip",clip},{"frames",json::array()}};
            html << "<section id='clip-" << clip << "'><h2>片段 " << clip << " · "
                << escaped(saved.index.at("clip_trigger")) << " · 開局後 " << (saved.time-start)/1e9 << " 秒</h2>";
            html << "<p>附近事件：";
            bool found=false;
            for (const auto& incident:incidents) {
                const auto time=incident.at("host_qpc_ns").get<Nanoseconds>();
                if (std::abs(time-saved.time)<=nearby_ns) {
                    found=true;html << " #" << incident.at("id") << ' ' << escaped(incident.at("trigger"))
                        << "（" << (time-saved.time)/1e6 << "ms）";
                }
            }
            if (!found) html << "無 ±500ms 事件，仍可標可見線／Note；無逐音符結果證據。";
            html << "</p><div class=grid>";
        }
        clip_end=saved.time;current_clip["frames"].push_back(saved.sequence);
        Frame frame;frame.width=1280;frame.height=720;frame.stride=3840;
        frame.rgb.resize(2764800);std::ifstream pixels(saved.path,std::ios::binary);
        pixels.read(reinterpret_cast<char*>(frame.rgb.data()),static_cast<std::streamsize>(frame.rgb.size()));
        if (pixels.gcount()!=static_cast<std::streamsize>(frame.rgb.size())) throw std::runtime_error("RGB read failed");
        const auto name=std::to_string(saved.sequence);const auto raw_name="frames/"+name+"-raw.png";
        const auto overlay_name="frames/"+name+"-recorded-overlay.png";
        write_diagnostic_png(output/raw_name,frame);
        // The sheet is a nearest-neighbor viewing aid. Full resolution PNG/RGB remains authoritative.
        const int ox=static_cast<int>(i%3)*426,oy=static_cast<int>(i/3)*240;
        for (int y=0;y<240;++y) for (int x=0;x<426;++x)
            std::copy_n(frame.rgb.begin()+y*3*frame.stride+x*3*3,3,sheet.rgb.begin()+(y+oy)*sheet.stride+(x+ox)*3);
        recorded_overlay(frame,saved.scene);write_diagnostic_png(output/overlay_name,frame);
        json note_hints=json::array(),line_hints=json::array();
        if (!saved.scene.is_null()) {
            for (const auto& candidate:saved.scene.at("lines"))
                line_hints.push_back({{"recorded_candidate",candidate},{"role","unknown"},
                    {"role_options",{"judgment_line","decorative_line","unknown"}},{"reviewer",nullptr}});
            for (const auto& target:saved.scene.at("targets"))
                note_hints.push_back({{"recorded_candidate",slim_target(target)},
                    {"part","unknown"},{"note_kind","unknown"},{"verified_geometry",nullptr},
                    {"associated_line","unknown"},{"judgment","unknown"},{"reviewer",nullptr}});
        }
        annotations.push_back({{"source_frame",saved.sequence},{"capture_complete_ns",saved.time},
            {"raw_rgb_path",std::filesystem::absolute(saved.path).generic_string()},
            {"raw_rgb_sha256",saved.index.at("sha256")},{"raw_png",raw_name},
            {"raw_png_sha256",sha256_file(output/raw_name)},{"recorded_overlay",overlay_name},
            {"exact_recorded_decision_join",!saved.scene.is_null()},
            {"line_roles",line_hints},{"note_parts_and_relations",note_hints},
            {"additional_pixel_instances",json::array()},{"actual_miss_confirmed",false},
            {"label_status","unknown"},{"human_gold",false},{"reviewer",nullptr}});
        html << "<figure><a href='" << raw_name << "'><img src='" << raw_name << "'></a><figcaption>Frame "
            << saved.sequence << " · " << (saved.time-clip_start)/1e6 << "ms · "
            << "<a href='" << overlay_name << "'>當時偵測疊圖</a></figcaption><details><summary>原始 journal 候選</summary><pre>"
            << escaped(saved.scene.dump(2)) << "</pre></details></figure>";
    }
    current_clip["span_ms"]=(clip_end-clip_start)/1e6;clips.push_back(current_clip);
    html << "</div></section><p>標註順序：先看原圖標 Note 類型／head-body-tail／當前接觸區，"
        "再覆核真／假線及 Note→line；最後另查觸控收據。看不到的判定保持 unknown。"
        "三幀沒有前後過程時，不補畫缺失影格、不把幾何過線視為已完成。</p>";
    html.close();if(!html)throw std::runtime_error("HTML write failed");
    write_diagnostic_png(output/"contact-sheet.png",sheet);
    write_json(output/"incidents.json",{{"schema",1},{"events",incidents},{"actual_miss_confirmed",0},
        {"semantics","suspected failure diagnostic events only; count is not game Miss count"}});
    write_json(output/"annotation-template.json",{{"schema",1},{"status","unreviewed_unknown"},
        {"candidate_origin","recorded_live_journal_not_human_truth"},{"human_gold",0},{"frames",annotations}});
    write_json(output/"clips.json",clips);
    double sampled_ms=0;for(const auto& clip:clips)sampled_ms+=clip.at("span_ms").get<double>();
    const json report={{"schema",1},{"round",round},{"frames",frames.size()},
        {"clips",clips.size()},{"sum_sparse_clip_spans_ms",sampled_ms},
        {"journal_decisions",decisions},{"incident_candidates",incidents.size()},
        {"triggers",triggers},{"contact_cancellations_all",cancellations},
        {"cancellations_before_down_excluded",skipped_contact_before_down},
        {"incidents_with_pixels_within_500ms",nearby},{"incidents_without_pixels_within_500ms",incidents.size()-nearby},
        {"incidents_with_event_source_frame_saved",exact},
        {"exact_recorded_frame_joins",std::count_if(frames.begin(),frames.end(),[](const auto& f){return !f.scene.is_null();})},
        {"actual_misses_located",0},{"human_gold",0},{"runtime_feedback",false},
        {"real_input_backend_constructed",false},{"source_absolute_age",nullptr},
        {"verified_event_segments",verified},{"round_summary_sha256",sha256_file(round_root/"summary.json")},
        {"index_sha256",sha256_file(clip_root/"index.jsonl")},
        {"incidents_sha256",sha256_file(output/"incidents.json")},
        {"annotations_sha256",sha256_file(output/"annotation-template.json")},
        {"capacities",{{"saved_frames",max_frames},{"incidents",max_incidents},{"rows",1000000},
            {"decisions",100000},{"segments",32},{"segment_bytes",33554432}}},
        {"coverage_semantics","500ms is a review search radius, not a Miss attribution window; triples are not continuous video"}};
    write_json(output/"summary.json",report);std::cout<<report.dump(2)<<'\n';
    return 0;
}
int proposal(const std::filesystem::path& root,const std::filesystem::path& input,
             const std::filesystem::path& output) {
    if(std::filesystem::exists(output))throw std::invalid_argument("proposal output exists");
    const auto report=load_json(root/"summary.json");
    if(sha256_file(root/"annotation-template.json")!=report.at("annotations_sha256").get<std::string>())
        throw std::invalid_argument("annotation template SHA mismatch");
    const auto original=load_json(root/"annotation-template.json"),labels=load_json(input);
    if(labels.at("label_status")!="proposed" || labels.at("human_gold")!=false ||
       labels.at("actual_miss_confirmed")!=false || labels.at("frames").size()>16)
        throw std::invalid_argument("proposal truth/capacity contract");
    // Verify every referenced image before writing anything. Labels remain proposed.
    json references=json::array();
    for(const auto& label:labels.at("frames")) {
        const auto frame=label.at("source_frame").get<std::uint64_t>();
        const auto found=std::find_if(original.at("frames").begin(),original.at("frames").end(),
            [&](const auto& candidate){return candidate.at("source_frame")==frame;});
        if(found==original.at("frames").end() || found->at("raw_rgb_sha256")!=label.at("raw_rgb_sha256"))
            throw std::invalid_argument("proposal frame provenance mismatch");
        const auto relative=relative_path(found->at("raw_png").get<std::string>());
        if(sha256_file(root/relative)!=found->at("raw_png_sha256").get<std::string>())
            throw std::invalid_argument("proposal raw PNG SHA mismatch");
        if(label.at("polygons").size()>32)throw std::invalid_argument("polygon capacity");
        for(const auto& polygon:label.at("polygons")) {
            if(polygon.at("points").size()<2 || polygon.at("points").size()>64)
                throw std::invalid_argument("polygon vertex capacity");
            for(const auto& point:polygon.at("points")) {
                if(!point.is_array()||point.size()!=2)throw std::invalid_argument("point shape");
                const double x=point.at(0),y=point.at(1);
                if(!std::isfinite(x)||!std::isfinite(y)||x<0||x>1280||y<0||y>720)
                    throw std::invalid_argument("point geometry");
            }
        }
        references.push_back(*found);
    }
    std::filesystem::create_directories(output);
    std::ofstream html(output/"review.html",std::ios::binary);
    html<<"<!doctype html><meta charset=utf-8><title>Hold 身分切分標註提議</title>"
        "<style>body{font:16px system-ui;background:#141820;color:#eee;margin:24px}a{color:#7ddfff}"
        "svg{width:100%;max-width:1280px}pre{white-space:pre-wrap}section{margin-bottom:28px}</style>"
        "<h1>Hold 身分切分：事件118</h1><p>助理依原圖提出 body（藍框）、rail（綠框）、"
        "tail（橘框）與水平線（紅框）。head被特效遮蔽，判定結果unknown；所有框是proposed、human gold 0。</p>";
    for(std::size_t i=0;i<labels.at("frames").size();++i) {
        const auto& label=labels.at("frames").at(i);const auto& ref=references.at(i);
        const auto raw=std::filesystem::relative(root/ref.at("raw_png").get<std::string>(),output).generic_string();
        html<<"<section><h2>Frame "<<label.at("source_frame")<<"</h2><a href='"<<escaped(raw)
            <<"'>完整未標註原圖</a><br><svg viewBox='0 0 1280 720' xmlns='http://www.w3.org/2000/svg'>"
            <<"<image href='"<<escaped(raw)<<"' width='1280' height='720'/>";
        for(const auto& polygon:label.at("polygons")) {
            const auto role=polygon.at("role").get<std::string>();
            const char* color=role=="body"?"#28bfff":role=="rail"?"#21ff9a":role=="tail"?"#ffad33":"#ff6060";
            html<<"<polygon fill='none' stroke='"<<color<<"' stroke-width='2' points='";
            for(const auto& point:polygon.at("points"))html<<point.at(0).get<double>()<<','<<point.at(1).get<double>()<<' ';
            html<<"'><title>"<<escaped(role)<<" proposed</title></polygon>";
        }
        html<<"</svg><p>"<<escaped(label.at("observation"))<<"</p></section>";
    }
    html<<"<p>原ID1052在frame27449因identity_ambiguous撤銷，當幀另生rails ID1063。"
        "三張COMBO仍18，未看到Miss判定。不能由框線確認遊戲是否接受touch。</p>";
    html.close();if(!html)throw std::runtime_error("proposal HTML write failed");
    write_json(output/"proposal.json",labels);
    write_json(output/"summary.json",{{"source_proposal_sha256",sha256_file(input)},
        {"frames",references},{"label_status","proposed"},{"human_gold",0},{"actual_misses_located",0},
        {"runtime_feedback",false},{"real_input_backend_constructed",false}});
    return 0;
}
}
int main(int argc,char** argv) {
    try {
        if(argc==5&&std::string(argv[1])=="proposal")return proposal(argv[2],argv[3],argv[4]);
        if (argc!=4) throw std::invalid_argument("usage: pas_miss_review round-root pixel-clips-root new-output-dir");
        return review(argv[1],argv[2],argv[3]);
    } catch (const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}
}
