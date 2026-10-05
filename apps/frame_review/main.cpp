#include "pas/game.hpp"
#include "pas/adb.hpp"
#include "pas/analysis.hpp"
#include <fstream>
#include <iostream>
#include <map>
#include <set>
#include <regex>
#include "review_io.hpp"
#ifdef PAS_X1_OFFLINE
#include "contact_replay.hpp"
#endif

using namespace pas;
using json=nlohmann::json;
namespace fs=std::filesystem;
namespace {
using pas::review::load;using pas::review::relative;using pas::review::rows;using pas::review::save;
void sheet(const fs::path& root,const std::vector<json>& index,const std::vector<std::size_t>& ordinals,const fs::path& output){
    Frame out;out.width=1280;out.height=static_cast<int>((ordinals.size()+2)/3)*240;out.stride=out.width*3;out.rgb.resize(static_cast<std::size_t>(out.stride)*out.height);
    json mapping=json::array();
    for(std::size_t i=0;i<ordinals.size();++i){const auto& e=index.at(ordinals[i]);auto f=load_diagnostic_png(root/relative(e.at("path")));const int ox=static_cast<int>(i%3)*426,oy=static_cast<int>(i/3)*240;
        for(int y=0;y<240;++y)for(int x=0;x<426;++x)for(int c=0;c<3;++c)out.rgb[static_cast<std::size_t>(oy+y)*out.stride+(ox+x)*3+c]=f.rgb[static_cast<std::size_t>(y*3)*f.stride+x*3*3+c];
        mapping.push_back({{"tile",i},{"ordinal",ordinals[i]},{"source_frame",e.at("source_frame")},{"video_s",(e.at("capture_complete_ns").get<Nanoseconds>()-index.front().at("capture_complete_ns").get<Nanoseconds>())/1e9},{"png",fs::absolute(root/relative(e.at("path"))).generic_string()}});
    }write_diagnostic_png(output,out);save(output.string()+".json",mapping);
}
}
int main(int argc,char** argv){try{
#ifdef PAS_X4_OFFLINE
    if(argc>1&&std::string(argv[1])=="contact-x4")return contact_replay_main(argc,argv);
#endif
#ifdef PAS_X1_OFFLINE
    if(argc>1&&(std::string(argv[1])=="contact"||std::string(argv[1])=="contact-compare"||std::string(argv[1])=="observer-compat"||std::string(argv[1])=="contact-x2"||std::string(argv[1])=="contact-x2-compare"))return contact_replay_main(argc,argv);
#endif
    if(argc==4&&std::string(argv[1])=="probe") {const auto f=load_diagnostic_png(argv[2]);const int x=std::stoi(argv[3]);if(x<20||x>=f.width-20)throw std::runtime_error("probe x");json columns=json::array();
        const auto gray=[](const std::uint8_t* p){return (p[0]*77+p[1]*150+p[2]*29)/256;};
        for(int px=x-4;px<=x+4;++px){int begin=-1,last=-1,gaps=0;json runs=json::array();const auto finish=[&]{if(begin>=0&&last-begin>400)runs.push_back({{"first",begin},{"last",last}});};
            for(int y=0;y<f.height;++y){const auto* p=f.rgb.data()+static_cast<std::size_t>(y)*f.stride+px*3;const bool white=p[0]>195&&p[1]>195&&p[2]>195&&std::max({p[0],p[1],p[2]})-std::min({p[0],p[1],p[2]})<35;if(white){if(begin<0)begin=y;last=y;gaps=0;}else if(begin>=0&&++gaps>4){finish();begin=last=-1;gaps=0;}}finish();columns.push_back({{"x",px},{"runs",runs}});
        }json samples=json::array();for(int k=1;k<=9;++k){const int y=8+711*k/10;const auto* p=f.rgb.data()+static_cast<std::size_t>(y)*f.stride+x*3;samples.push_back({{"y",y},{"center",gray(p)},{"left",gray(p-12)},{"right",gray(p+12)},{"rgb",{p[0],p[1],p[2]}}});}std::cout<<json{{"columns",columns},{"samples",samples}}.dump(2)<<'\n';return 0;
    }
    if(argc==4&&std::string(argv[1])=="digest") {
        const fs::path input=argv[2],output=argv[3];if(fs::exists(output))throw std::runtime_error("output exists");json notes=json::object(),cancels=json::array(),receipts=json::array();
        for(const auto& row:rows(input,18000)){const auto& d=row.at("recorded_decision");if(!d.is_null())for(const auto& t:d.at("targets")){auto key=std::to_string(t.at("note_id").get<std::uint64_t>());auto& n=notes[key];
            if(n.is_null())n={{"kind",t.at("kind")},{"first_ordinal",row.at("ordinal")},{"transitions",json::array()},{"frames",0},{"root_frames",0},{"accepted",json::array()},{"cancellations",json::array()}};
            n["frames"]=n["frames"].get<int>()+1;n["last_ordinal"]=row.at("ordinal");if(!t.at("crossing_ns").is_null())n["root_frames"]=n["root_frames"].get<int>()+1;
            const json state={{"reason",t.at("reason")},{"line_id",t.at("line_id")},{"basis",t.at("observation_basis")}};
            if(!n.contains("last_state")||n.at("last_state")!=state){if(n["transitions"].size()<128)n["transitions"].push_back({{"ordinal",row.at("ordinal")},{"video_s",row.at("video_s")},{"x",t.at("x")},{"y",t.at("y")},{"state",state}});n["last_state"]=state;}
        }
            for(const auto& e:row.at("recorded_events")){const auto event=e.value("event","");if(event=="game_contact_cancelled"){auto v=e;v["ordinal"]=row.at("ordinal");v["video_s"]=row.at("video_s");v["lines"]=d.is_null()?json(nullptr):d.at("lines");v["targets"]=d.is_null()?json(nullptr):d.at("targets");if(cancels.size()>=2048)throw std::runtime_error("cancel capacity");cancels.push_back(v);const auto key=std::to_string(e.at("note_id").get<std::uint64_t>());if(notes.contains(key))notes[key]["cancellations"].push_back(e);}
                if(event=="game_plan_accepted"){const auto key=std::to_string(e.at("note_id").get<std::uint64_t>());if(notes.contains(key))notes[key]["accepted"].push_back(e);}
                if(event=="game_touch_receipt"){if(receipts.size()>=10000)throw std::runtime_error("receipt capacity");auto v=e;v["ordinal"]=row.at("ordinal");receipts.push_back(v);}
            }
        }
        save(output,{{"source_sha256",sha256_file(input)},{"notes",notes},{"cancellations",cancels},{"receipts",receipts},{"notes_without_recorded_down_is_not_game_miss_gold",true}});std::cout<<"notes="<<notes.size()<<" cancellations="<<cancels.size()<<" receipts="<<receipts.size()<<'\n';return 0;
    }
    if(argc==5&&std::string(argv[1])=="compare") {
        const fs::path output=argv[4];if(fs::exists(output))throw std::runtime_error("output exists");const auto a=rows(argv[2],18000),b=rows(argv[3],18000);if(a.size()!=b.size())throw std::runtime_error("compare size");
        json changes=json::array();std::uint64_t changed=0,added=0,lost=0,vertical_added=0;std::vector<double> motion;
        for(std::size_t i=0;i<a.size();++i){if(a[i].at("source_frame")!=b[i].at("source_frame")||a[i].at("ordinal")!=b[i].at("ordinal"))throw std::runtime_error("compare context");const auto& sa=a[i].at("scene");const auto& sb=b[i].at("scene");if(sa.is_null()||sb.is_null())continue;
            const auto counts=[](const json& s){int roots=0,vertical=0;for(auto& t:s.at("targets"))roots+=!t.at("crossing_ns").is_null();for(auto& l:s.at("lines"))vertical+=std::abs(l.at("uy").get<double>())>.98;return std::pair{roots,vertical};};const auto ca=counts(sa),cb=counts(sb);added+=std::max(0,cb.first-ca.first);lost+=std::max(0,ca.first-cb.first);vertical_added+=std::max(0,cb.second-ca.second);
            if(ca!=cb){++changed;if(changes.size()<256)changes.push_back({{"ordinal",a[i].at("ordinal")},{"source_frame",a[i].at("source_frame")},{"a_roots",ca.first},{"b_roots",cb.first},{"a_vertical",ca.second},{"b_vertical",cb.second}});}
        }save(output,{{"frames",a.size()},{"changed_count_frames",changed},{"additional_root_occurrences",added},{"lost_root_occurrences",lost},{"additional_vertical_line_occurrences",vertical_added},{"examples",changes},{"correctness_from_difference",false}});return 0;
    }
    if(argc==6&&std::string(argv[1])=="sheet") {const fs::path root=argv[2],output=argv[5];if(fs::exists(output))throw std::runtime_error("output exists");auto index=rows(root/"index.jsonl",36000);std::vector<std::size_t> picks;for(auto i=std::stoul(argv[3]);i<=std::stoul(argv[4]);++i){picks.push_back(i);if(picks.size()>12)throw std::runtime_error("sheet capacity");}sheet(root,index,picks,output);return 0;}
    if(argc!=5)throw std::runtime_error("usage: pas_frame_review session-root selected-root new-output-dir replay|recorded");
    const fs::path session=argv[1],selected=argv[2],output=argv[3],recording=session/"full-recording";
    const bool replay=std::string(argv[4])=="replay";if(!replay&&std::string(argv[4])!="recorded")throw std::runtime_error("mode");
    if(fs::exists(output))throw std::runtime_error("output exists");
    auto index=rows(recording/"index.jsonl",36000);if(index.empty()||sha256_file(recording/"index.jsonl")!=load(recording/"summary.json").at("index_sha256").get<std::string>())throw std::runtime_error("index SHA");
    std::map<std::uint64_t,json> decisions;std::map<std::uint64_t,std::vector<json>> events;
    std::map<std::uint64_t,std::string> intent_kind;std::uint64_t last_frame=0;std::size_t event_rows=0;
    const auto round=session/"round-1";const auto round_summary=load(round/"summary.json");for(auto& seg:round_summary.at("event_segments")){
        const auto p=round/relative(seg.at("path"));if(sha256_file(p)!=seg.at("sha256").get<std::string>())throw std::runtime_error("journal SHA");
        for(auto& e:rows(p,100000)){if(++event_rows>1000000)throw std::runtime_error("journal total capacity");const auto name=e.value("event","");
            if(name=="game_decision"){last_frame=e.at("frame_sequence");if(decisions.size()>=36000)throw std::runtime_error("decision capacity");decisions[last_frame]=e;}
            else {const auto f=e.value("source_frame",last_frame);if(e.contains("intent_id")&&e.contains("kind"))intent_kind[e.at("intent_id")]=e.at("kind");
                if(e.contains("intent_id")&&!e.contains("kind")&&intent_kind.contains(e.at("intent_id")))e["derived_intent_kind"]=intent_kind.at(e.at("intent_id"));
                e["join_basis"]=e.contains("source_frame")?"exact_source_frame":"journal_order_latest_decision_not_exact_frame";events[f].push_back(e);}
        }
    }
    std::map<std::uint64_t,std::set<std::string>> membership;std::map<std::string,json> reports;std::map<std::string,std::vector<std::size_t>> core;
    for(int i=1;i<=12;++i){char label[16];std::snprintf(label,sizeof(label),"clip-%02d",i);for(auto& e:rows(selected/label/"index.jsonl",18000)){
        const auto ordinal=e.at("recording_ordinal").get<std::uint64_t>();if(index.at(ordinal).at("png_sha256")!=e.at("png_sha256"))throw std::runtime_error("selected provenance");membership[ordinal].insert(label);if(e.at("selection_role").get<std::string>()=="requested")core[label].push_back(ordinal);
    }reports[label]={{"frames",0},{"decision_join_missing",0},{"near_targets",0},{"near_targets_without_root",0},{"reasons",json::object()},{"cancellations",json::object()},{"no_line",0},{"line_invalid",0},{"capture_gap_over_50ms",0}};}
    fs::create_directories(output);fs::copy_file(selected/"REASONS.md",output/"user-reasons-snapshot.md");
    std::ifstream text_in(selected/"REASONS.md");std::string note((std::istreambuf_iterator<char>(text_in)),{});auto selection=load(selected.parent_path()/"selection.json");selection["selection_revision"]=3;selection["reason_source_sha256"]=sha256_file(selected/"REASONS.md");
    for(auto& c:selection["clips"]){auto id=c.at("id").get<std::string>();const auto start=note.find("## "+id),end=note.find("\n## ",start+1);if(start==std::string::npos)throw std::runtime_error("missing user reason section");const auto section=note.substr(start,end==std::string::npos?end:end-start);
        for(auto pair:std::vector<std::pair<std::string,std::string>>{{"看到的現象：","observation"},{"選取原因：","selection_reason"},{"希望標註：","annotation_request_text"},{"推測原因（可留白）：","hypothesis"}}){const auto pos=section.find(pair.first);c[pair.second]=pos==std::string::npos?json(nullptr):json(section.substr(pos+pair.first.size(),section.find('\n',pos)-pos-pair.first.size()));}
        c["human_gold"]=false;c["annotation_status"]="user_observation_received_pixel_roles_pending";if(id=="clip-09")c["analysis_role"]="user_corrected_no_issue_control";
    }save(output/"selection-with-reasons-r3.json",selection);
    std::ofstream joined(output/"frames.jsonl"),replayed(output/"replay.jsonl");FakeClock clock;GameObserver observer(clock);std::vector<double> costs;std::size_t verified=0,replay_n=0,selected_n=0;
    const auto zero=index.front().at("capture_complete_ns").get<Nanoseconds>();
    for(std::size_t i=0;i<index.size();++i){const auto& e=index[i];const auto f=e.at("source_frame").get<std::uint64_t>();const auto path=recording/relative(e.at("path"));if(fs::file_size(path)>4*1024*1024||sha256_file(path)!=e.at("png_sha256").get<std::string>())throw std::runtime_error("PNG SHA/capacity");++verified;
        json replay_scene=nullptr;if(replay&&(decisions.contains(f)||i<32)){auto pixels=load_diagnostic_png(path);pixels.sequence=f;pixels.capture_complete_ns=e.at("capture_complete_ns");pixels.source_rotation=1;pixels.epoch=pixels.generation=pixels.geometry_version=1;clock.set(pixels.capture_complete_ns);HostClock meter;auto begin=meter.now_ns();replay_scene=decision_json(observer.process(pixels));costs.push_back((meter.now_ns()-begin)/1e6);++replay_n;}
        if(!membership.contains(i))continue;++selected_n;const auto d=decisions.contains(f)?decisions.at(f):json(nullptr);json row={{"ordinal",i},{"source_frame",f},{"video_s",(e.at("capture_complete_ns").get<Nanoseconds>()-zero)/1e9},{"png_sha256",e.at("png_sha256")},{"png",fs::absolute(path).generic_string()},{"clips",membership.at(i)},{"dt_ns",e.at("dt_ns")},{"recorded_decision",d},{"recorded_events",events[f]}};joined<<row.dump()<<'\n';if(replay)replayed<<json{{"ordinal",i},{"source_frame",f},{"scene",replay_scene}}.dump()<<'\n';
        for(auto& id:membership.at(i)){auto& r=reports[id];r["frames"]=r["frames"].get<int>()+1;if(!e.at("dt_ns").is_null()&&e.at("dt_ns").get<Nanoseconds>()>50000000)r["capture_gap_over_50ms"]=r["capture_gap_over_50ms"].get<int>()+1;
            if(d.is_null())r["decision_join_missing"]=r["decision_join_missing"].get<int>()+1;else{if(d.at("lines").empty())r["no_line"]=r["no_line"].get<int>()+1;for(auto& l:d.at("lines"))if(!l.at("association_valid").get<bool>())r["line_invalid"]=r["line_invalid"].get<int>()+1;
                for(auto& t:d.at("targets")){const auto reason=t.at("kind").get<std::string>()+":"+t.at("reason").get<std::string>();r["reasons"][reason]=r["reasons"].value(reason,0)+1;if(t.at("line_id")!=0&&std::abs(t.at("relative_distance_px").get<double>())<32){r["near_targets"]=r["near_targets"].get<int>()+1;if(t.at("crossing_ns").is_null())r["near_targets_without_root"]=r["near_targets_without_root"].get<int>()+1;}}
            }for(auto& event:events[f])if(event.value("event","")=="game_contact_cancelled"){const auto key=event.value("kind",event.value("derived_intent_kind","unknown"))+":"+event.value("reason","");r["cancellations"][key]=r["cancellations"].value(key,0)+1;}
        }
    }
    joined.close();replayed.close();for(auto& [id,picks]:core){std::vector<std::size_t> thumbnails;for(std::size_t n=0;n<std::min<std::size_t>(12,picks.size());++n)thumbnails.push_back(picks[n*(picks.size()-1)/std::max<std::size_t>(1,std::min<std::size_t>(12,picks.size())-1)]);sheet(recording,index,thumbnails,output/(id+"-sheet.png"));}
    save(output/"summary.json",{{"all_source_pngs_verified",verified},{"unique_selected_frames",selected_n},{"journal_rows",event_rows},{"recorded_decisions",decisions.size()},{"reports",reports},{"replay_frames",replay_n},{"observer_ms",distribution(costs)},{"human_pixel_gold",0},{"per_note_game_judgment","unknown"},{"replay_scope","original recognized-frame cadence plus 32 pre-roll warmup; missing earlier standby state; predictions are proposals, no real or fake input constructed"},{"user_reasons_sha256",sha256_file(selected/"REASONS.md")},{"frames_sha256",sha256_file(output/"frames.jsonl")}});std::cout<<"verified="<<verified<<" selected="<<selected_n<<" replay="<<replay_n<<'\n';return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
