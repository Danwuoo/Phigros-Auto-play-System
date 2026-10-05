#include "contract.hpp"
#include "pas/adb.hpp"
#include "pas/analysis.hpp"
#include "review_io.hpp"
#include <iostream>
#include <map>
#include <set>
using namespace pas;
using namespace pas::review;
namespace {
NoteCandidate note(const json& j){NoteCandidate n;n.kind=j.at("kind")=="hold"?NoteKind::hold:NoteKind::ambiguous;n.center={j.at("center")[0],j.at("center")[1]};n.tangent={j.at("tangent")[0],j.at("tangent")[1]};n.width=j.at("width");n.height=j.at("height");n.rails_geometry=j.at("rails");n.head_on_line=j.at("head_on_line");n.held_body_evidence=j.at("held_body_evidence");return n;}
LineCandidate line(const json& j){LineCandidate l;l.center={j.at("x"),j.at("y")};l.tangent={j.at("ux"),j.at("uy")};l.track_id=j.at("line_id");l.association_valid=j.at("association_valid");l.observed_ns=j.at("observed_ns");return l;}
}
int main(int argc,char** argv){try{
 if(argc!=3)throw std::runtime_error("packet output arguments");
 if(fs::exists(argv[2]))throw std::runtime_error("new output required");
 const auto packet=load(argv[1]);if(packet.at("human_gold")!=0||packet.at("frames").size()!=196)throw std::runtime_error("packet contract");
 const auto tracePath=packet.at("trace_path").get<std::string>();if(sha256_file(tracePath)!=packet.at("trace_sha256").get<std::string>())throw std::runtime_error("trace SHA");
 std::map<int,json> trace;each_row(tracePath,256,[&](const json& r){if(!trace.emplace(r.at("ordinal"),r).second)throw std::runtime_error("duplicate trace ordinal");},16*1024*1024);
 json audit=json::array();std::size_t bank_count=0,hold_count=0,paired=0,targets=0,probes=0,skipped=0;std::set<int> seen;
 for(const auto& p:packet.at("frames")){
   const int ordinal=p.at("ordinal");if(!seen.insert(ordinal).second)throw std::runtime_error("duplicate packet ordinal");
   const auto& r=trace.at(ordinal);if(r.at("source_frame")!=p.at("source_frame")||r.at("png_sha256")!=p.at("png_sha256")||r.at("timing").at("original_capture_complete_ns")!=p.at("capture_complete_ns")||r.at("timing").at("original_pixels_ready_ns")!=p.at("pixels_ready_ns"))throw std::runtime_error("clock PNG join");
   const auto path=p.at("path").get<std::string>();if(fs::file_size(path)>4*1024*1024||sha256_file(path)!=p.at("png_sha256").get<std::string>())throw std::runtime_error("PNG SHA or cap");
   auto f=load_diagnostic_png(path);if(f.width!=1280||f.height!=720||f.rgb.size()!=1280*720*3)throw std::runtime_error("geometry");
   f.sequence=p.at("source_frame");f.epoch=f.generation=f.geometry_version=1;f.source_rotation=1;f.capture_complete_ns=r.at("timing").at("replay_capture_complete_ns");
   if(r.at("scene").is_null()||r.at("candidate_bank").is_null()){
     if(r.at("consumed")!=false)throw std::runtime_error("missing consumed state");++skipped;
     audit.push_back({{"ordinal",ordinal},{"source_frame",p.at("source_frame")},{"capture_complete_ns",p.at("capture_complete_ns")},{"pixels_ready_ns",p.at("pixels_ready_ns")},{"png_sha256",p.at("png_sha256")},{"consumed",false},{"reason","original_owner_cadence_skip_no_current_scene"},{"hold_supports",nullptr}});continue;
   }
   const auto& bank=r.at("candidate_bank").at("candidates");const auto& ts=r.at("scene").at("targets");const auto& ls=r.at("scene").at("lines");
   if(bank.size()>128||ts.size()>128||ls.size()>16)throw std::runtime_error("candidate cap");bank_count+=bank.size();targets+=ts.size();json supports=json::array();
   for(const auto& b:bank){if(b.at("note").at("kind")!="hold")continue;++hold_count;auto n=note(b.at("note"));LineCandidate l;
     // This is a current shape-support audit. No runtime identity or history
     // is fed to the rejected ownership hypothesis. It cannot authorize actions.
     if(!ls.empty())l=line(ls.front());auto c=x10d_o::resolve(f,n,l,{});paired+=c.pixel_support;probes+=c.probes;
     supports.push_back({{"candidate_id_local",b.at("candidate_id")},{"origin",b.at("origin")},{"note",b.at("note")},{"current_bilateral_support",c.pixel_support},{"measured_depth_px",c.measured_depth},{"probes",c.probes},{"reason",c.reason},{"physical_owner",nullptr},{"action_eligibility",nullptr}});
   }
   audit.push_back({{"ordinal",ordinal},{"source_frame",p.at("source_frame")},{"capture_complete_ns",p.at("capture_complete_ns")},{"pixels_ready_ns",p.at("pixels_ready_ns")},{"png_sha256",p.at("png_sha256")},{"all_bank_candidates",bank.size()},{"all_targets",ts.size()},{"hold_supports",supports}});
 }
 json result={{"schema",1},{"frames_verified",seen.size()},{"trace_rows",trace.size()},{"original_cadence_skips",skipped},{"consumed_selected_frames",seen.size()-skipped},{"all_bank_candidates",bank_count},{"all_targets",targets},{"hold_candidates",hold_count},{"bilateral_supported",paired},{"probes",probes},{"human_gold_added",0},{"runtime_policy_modified",false},{"new_perception_owner_runs",0},{"full_recording_replays",0},{"packet_sha256",sha256_file(argv[1])},{"trace_sha256",packet.at("trace_sha256")},{"scope","196 exact PNG/clock joins; current shape support on all available bank Holds; original skips stay null. Selected full-prefix software state reused, no window cold-start, no physical ownership or new causal action comparison"},{"rows",audit}};
 if(result.dump(2).size()>8*1024*1024)throw std::runtime_error("output cap");save(argv[2],result);std::cout<<"verified="<<seen.size()<<" bank="<<bank_count<<" holds="<<hold_count<<" paired="<<paired<<"\n";return 0;
 }catch(const std::exception& e){std::cerr<<e.what()<<"\n";return 1;}}
