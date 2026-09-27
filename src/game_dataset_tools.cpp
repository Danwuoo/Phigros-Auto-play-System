#include "pas/game_dataset.hpp"
#include "pas/adb.hpp"
#include "pas/analysis.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <fstream>
#include <map>
#include <set>
#include <stdexcept>

namespace pas {
namespace {
using json=nlohmann::json;
json read_json(const std::filesystem::path& p){if(std::filesystem::file_size(p)>16*1024*1024)throw std::invalid_argument("dataset JSON capacity");std::ifstream in(p);if(!in)throw std::runtime_error("dataset JSON missing");return json::parse(in);}
void write_json(const std::filesystem::path& p,const json& j){if(std::filesystem::exists(p))throw std::runtime_error("dataset output exists");std::ofstream out(p);if(!out||!(out<<j.dump(2)<<'\n'))throw std::runtime_error("dataset write");}
std::filesystem::path folder_path(const std::filesystem::path& root,const std::string& text){const std::filesystem::path p(text);if(p.is_absolute()||p.empty())throw std::invalid_argument("dataset relative path");for(const auto& component:p)if(component=="..")throw std::invalid_argument("dataset path traversal");
 const auto r=std::filesystem::weakly_canonical(root/p),base=std::filesystem::weakly_canonical(root);const auto rel=r.lexically_relative(base);if(rel.empty()||*rel.begin()=="..")throw std::invalid_argument("dataset link leaves root");return r;}
Frame roi(const Frame& f,int x,int y,int w,int h){Frame r;r.width=w;r.height=h;r.stride=w*3;r.rgb.resize(static_cast<std::size_t>(w)*h*3);
 for(int row=0;row<h;++row)std::copy_n(f.rgb.data()+static_cast<std::size_t>(row+y)*f.stride+x*3,w*3,r.rgb.data()+static_cast<std::size_t>(row)*r.stride);return r;}
std::uint64_t difference_hash(const Frame& f){std::uint64_t hash=0;for(int y=0;y<8;++y)for(int x=0;x<8;++x){const auto gray=[&](int gx){const int px=std::min(f.width-1,gx*f.width/9),py=std::min(f.height-1,y*f.height/8);const auto* p=f.rgb.data()+static_cast<std::size_t>(py)*f.stride+px*3;return p[0]+p[1]+p[2];};if(gray(x)>gray(x+1))hash|=std::uint64_t{1}<<(y*8+x);}return hash;}
int hamming(std::uint64_t a,std::uint64_t b){int n=0;for(auto v=a^b;v;v&=v-1)++n;return n;}
json box(int cls,int instance,int x0,int y0,int x1,int y1){return {{"semantic",cls},{"instance",instance},{"points",{{x0,y0},{x1,y0},{x1,y1},{x0,y1}}}};}
}
json validate_vision_dataset(const std::filesystem::path& root){const auto manifest=read_json(root/"manifest.json");if(manifest.at("schema")!=1||manifest.at("samples").size()>1280)throw std::invalid_argument("dataset schema/capacity");
 json errors=json::array(),warnings=json::array(),coverage=json::object(),near_pairs=json::array();std::set<std::string> folders;std::map<std::string,std::string> run_split,clip_split,exact_split;
 std::map<std::string,Nanoseconds> last_time;std::map<std::string,json> clip_roi,clip_context;
 struct Duplicate {std::string folder,run,split,hash;std::uint64_t perceptual;int width,height;};std::vector<Duplicate> duplicates;
 std::size_t annotated=0,reviewed=0,ignored_pixels=0,labeled_pixels=0,total_instances=0,near_count=0;std::set<std::string> runs;
 for(const auto& entry:manifest.at("samples")){const auto relative=entry.at("folder").get<std::string>();try{
  if(!folders.insert(relative).second)throw std::invalid_argument("duplicate sample folder");const auto folder=folder_path(root,relative);const auto a=read_json(folder/"annotation.json");
  if(a.at("schema")!=1||a.at("image_sha256")!=sha256_file(folder/"image.png")||entry.at("image_sha256")!=a.at("image_sha256"))throw std::invalid_argument("sample hash/schema");
  const auto image=load_diagnostic_png(folder/"image.png");const auto& crop=a.at("roi");const int x=crop.at("x"),y=crop.at("y"),w=crop.at("width"),h=crop.at("height");
  const auto& context=a.at("context");const int source_w=context.at("width"),source_h=context.at("height");
  if(x<0||y<0||w!=image.width||h!=image.height||w>640||h>384||source_w<2||source_h<2||x+w>source_w||y+h>source_h||crop.at("scale")!=1)throw std::invalid_argument("native ROI transform");
  const auto run=entry.at("run_id").get<std::string>(),clip=entry.at("clip_id").get<std::string>(),split=entry.at("split").get<std::string>(),key=run+"/"+clip;
  if(a.at("run_id")!=run||a.at("clip_id")!=clip)throw std::invalid_argument("sample group mismatch");if(split!="development"&&split!="train"&&split!="validation"&&split!="test")throw std::invalid_argument("split name");
  if(run_split.contains(run)&&run_split[run]!=split)throw std::invalid_argument("run split leakage");run_split[run]=split;
  if(clip_split.contains(key)&&clip_split[key]!=split)throw std::invalid_argument("clip split leakage");clip_split[key]=split;runs.insert(run);
  const Nanoseconds time=context.at("capture_ns");if(time<0||(last_time.contains(key)&&time<=last_time[key]))throw std::invalid_argument("clip QPC order");last_time[key]=time;
  if(clip_roi.contains(key)&&clip_roi[key]!=crop)throw std::invalid_argument("clip ROI changed");clip_roi[key]=crop;
  const json epoch_geometry={{"epoch",context.at("epoch")},{"generation",context.at("generation")},{"geometry",context.at("geometry")},{"rotation",context.at("rotation")},{"width",source_w},{"height",source_h}};
  if(clip_context.contains(key)&&clip_context[key]!=epoch_geometry)throw std::invalid_argument("clip context changed");clip_context[key]=epoch_geometry;
  const auto hash=entry.at("image_sha256").get<std::string>();if(exact_split.contains(hash)&&exact_split[hash]!=split)throw std::invalid_argument("exact duplicate split leakage");exact_split[hash]=split;
  const auto phash=difference_hash(image);for(const auto& d:duplicates)if(d.run!=run&&d.width==w&&d.height==h&&hamming(d.perceptual,phash)<=4){
   ++near_count;if(near_pairs.size()<256)near_pairs.push_back({{"a",d.folder},{"b",relative},{"hamming",hamming(d.perceptual,phash)},{"different_split",d.split!=split}});
   if(d.split!=split)throw std::invalid_argument("near duplicate split leakage: requires group review");}
  duplicates.push_back({relative,run,split,hash,phash,w,h});
  const bool has_semantic=std::filesystem::exists(folder/"semantic.png"),has_instance=std::filesystem::exists(folder/"instance.png");
  if(has_semantic!=has_instance)throw std::invalid_argument("incomplete label pair");
  if(!has_semantic){warnings.push_back({{"folder",relative},{"reason","unannotated"}});continue;}
  const auto semantic=read_label_png(folder/"semantic.png",8),instance=read_label_png(folder/"instance.png",16);
  if(semantic.width!=w||semantic.height!=h||instance.width!=w||instance.height!=h)throw std::invalid_argument("label dimensions");
  const auto expected=annotation_masks(w,h,a);
  if(expected.first.values!=semantic.values||expected.second.values!=instance.values)throw std::invalid_argument("polygon/mask pixel mismatch");
  if(a.at("objects").size()>256)throw std::invalid_argument("object cap");
  std::map<std::uint16_t,std::string> objects;
  for(const auto& object:a.at("objects")){const auto id=object.at("instance").get<int>();const auto type=object.at("type").get<std::string>();
   if(id<=0||id>=65535||!objects.emplace(static_cast<std::uint16_t>(id),type).second)throw std::invalid_argument("object instance/duplicate");
   if(type!="hold"&&type!="tap"&&type!="drag"&&type!="flick"&&type!="judge_line")throw std::invalid_argument("object type");
   if(object.value("amodal",true)||!object.contains("visibility")||!object.contains("truncated")||!object.contains("type_uncertainty"))throw std::invalid_argument("visible-only object metadata");
   if(type=="judge_line"){const auto& endpoints=object.at("centerline");if(!endpoints.is_array()||endpoints.size()!=2)throw std::invalid_argument("line endpoints");
    for(const auto& p:endpoints)if(p.size()!=2||!std::isfinite(p[0].get<double>())||!std::isfinite(p[1].get<double>())||p[0]<0||p[0]>w||p[1]<0||p[1]>h)throw std::invalid_argument("line endpoint bound");}
  }
  std::map<std::uint16_t,std::set<int>> instance_classes;
  for(std::size_t i=0;i<semantic.values.size();++i){const int c=semantic.values[i];const auto id=instance.values[i];
   if(c!=255&&(c<0||c>8))throw std::invalid_argument("unknown semantic id");if(c==255){if(id!=65535)throw std::invalid_argument("ignore must retain unknown instance");++ignored_pixels;continue;}
   ++labeled_pixels;if(c==0&&id!=0)throw std::invalid_argument("background instance must be zero");
   if(c>=2&&c<=7&&(id==0||id==65535))throw std::invalid_argument("Note part requires instance");
   if(c==1&&(id==0||id==65535))throw std::invalid_argument("line requires instance");if(c==8&&id!=0)throw std::invalid_argument("effect instance must be zero");
   if(id&&id!=65535){if(!objects.contains(id))throw std::invalid_argument("mask instance absent from objects");const auto& type=objects.at(id);
    if((c==1&&type!="judge_line")||(c==2&&type!="tap")||(c>=3&&c<=5&&type!="hold")||(c==6&&type!="drag")||(c==7&&type!="flick"))throw std::invalid_argument("mask/object type mismatch");}
   if(id&&id!=65535)instance_classes[id].insert(c);coverage[std::to_string(c)]=coverage.value(std::to_string(c),std::size_t{0})+1;
  }
  for(const auto& [id,classes]:instance_classes){(void)id;bool hold=false,other=false;for(int c:classes){if(c>=3&&c<=5)hold=true;else other=true;}if(hold&&other)throw std::invalid_argument("Hold part instance mixed with another type");if(!hold&&classes.size()>1)throw std::invalid_argument("instance type mismatch");}
  total_instances+=instance_classes.size();
  // Verify masks are exactly reproduced from visible polygons, without
  // overwriting originals. Geometry and source masks are separate evidence.
  if(a.at("polygons").size()>256)throw std::invalid_argument("polygon count");
  for(const auto& p:a.at("polygons")){if(p.at("points").size()<3||p.at("points").size()>256)throw std::invalid_argument("polygon vertices");for(const auto& v:p.at("points")){
   const double px=v.at(0),py=v.at(1);if(!std::isfinite(px)||!std::isfinite(py)||px<0||py<0||px>w||py>h)throw std::invalid_argument("polygon bound");}}
  ++annotated;const auto status=a.at("review_status").get<std::string>();
  if(status=="reviewed_human"&&!a.value("reviewer",std::string{}).empty())++reviewed;else warnings.push_back({{"folder",relative},{"reason","not_independently_human_reviewed"}});
 }catch(const std::exception& e){errors.push_back({{"folder",relative},{"reason",e.what()}});}}
 return {{"schema",1},{"valid",errors.empty()},{"samples",manifest.at("samples").size()},{"annotated",annotated},{"reviewed_human",reviewed},{"unique_runs",runs.size()},
 {"training_ready",false},{"coverage_labeled_pixels_by_class",coverage},{"labeled_pixels",labeled_pixels},{"ignore_pixels",ignored_pixels},{"visible_instances_per_sample_sum",total_instances},{"near_duplicate_candidates",near_pairs},
 {"near_duplicate_candidate_count",near_count},{"near_duplicate_details_truncated",near_count>near_pairs.size()},
 {"errors",errors},{"warnings",warnings},{"split_scope","development unless enough independent reviewed runs"},{"raw_backup","single_copy_unless_separate_backup_recorded"}};
}
json export_vision_dataset(const std::filesystem::path& root,const std::filesystem::path& output){const auto qa=validate_vision_dataset(root);if(!qa.at("valid").get<bool>())throw std::runtime_error("dataset failed validation");
 const auto manifest=read_json(root/"manifest.json");json index=json::array();for(const auto& s:manifest.at("samples")){const auto folder=folder_path(root,s.at("folder"));
  if(std::filesystem::exists(folder/"semantic.png"))index.push_back({{"folder",s.at("folder")},{"run_id",s.at("run_id")},{"clip_id",s.at("clip_id")},{"split",s.at("split")},
   {"image_sha256",sha256_file(folder/"image.png")},{"semantic_sha256",sha256_file(folder/"semantic.png")},{"instance_sha256",sha256_file(folder/"instance.png")},
   {"annotation_sha256",sha256_file(folder/"annotation.json")},{"review_status",read_json(folder/"annotation.json").at("review_status")}});}
 const json result={{"schema",1},{"dataset_id",manifest.at("dataset_id")},{"training_ready",false},{"samples",index},{"qa",qa},{"no_images_uploaded",true}};
 write_json(output,result);return result;
}
json create_v75_dataset_pilot(const std::filesystem::path& source,const std::filesystem::path& output){
 if(std::filesystem::exists(output))throw std::runtime_error("pilot output exists");
 struct Sample {std::string file,hash;std::uint64_t frame;Nanoseconds capture;int x,y;};
 const std::array<Sample,2> defs{{{"diagnostic-hold-disappearance.png","71a08f064f7fed821dcc8f456755129dd7f285f4914cb426a381d4a31a85cb29",1025,13409328914000,280,336},
  {"diagnostic-combo-disappearance.png","79bc66feba6d77489f158990886541bc41e3ae637c724ca5623cd17a0d6a33c7",1208,13412407414600,360,240}}};
 for(const auto& d:defs)if(sha256_file(source/d.file)!=d.hash)throw std::runtime_error("v75 pilot source hash mismatch");
 std::filesystem::create_directories(output);json samples=json::array();std::vector<Frame> contact;
 for(std::size_t i=0;i<defs.size();++i){const auto& d=defs[i];const auto full=load_diagnostic_png(source/d.file);const auto image=roi(full,d.x,d.y,640,384);
  const auto folder=output/("pilot-"+std::to_string(i));std::filesystem::create_directory(folder);write_diagnostic_png(folder/"image.png",image);
  json polygons=json::array();if(i==0){polygons.push_back(box(4,1,125,59,192,112));polygons.push_back(box(4,2,595,4,635,41));polygons.push_back(box(1,30001,0,238,27,241));polygons.push_back(box(0,0,5,24,30,114));}
  else {polygons.push_back(box(4,1,225,10,324,150));polygons.push_back(box(1,30001,0,334,185,337));polygons.push_back(box(0,0,15,40,115,170));polygons.push_back(box(8,0,174,110,193,130));}
  json objects=json::array({{{"instance",1},{"type","hold"},{"visible_parts",{"body"}},{"head","unknown"},{"tail","unknown"},{"amodal",false},{"visibility","partial"},{"truncated",false},{"type_uncertainty","head and tail not labeled"}}});
  if(i==0)objects.push_back({{"instance",2},{"type","hold"},{"visible_parts",{"body"}},{"head","unknown"},{"tail","unknown"},{"amodal",false},{"visibility","partial"},{"truncated",true},{"type_uncertainty","visible rails/body only"}});
  objects.push_back({{"instance",30001},{"type","judge_line"},{"amodal",false},{"visibility","partial"},{"truncated",true},{"type_uncertainty","small visible baseline-aligned segment"},
   {"centerline",i==0?json::array({{0,239.5},{27,239.5}}):json::array({{0,335.5},{185,335.5}})}});
  const json annotation={{"schema",1},{"run_id",source.filename().string()},{"clip_id","single-frame-"+std::to_string(d.frame)},
   {"context",{{"epoch",2},{"generation",1},{"geometry",1},{"frame",d.frame},{"capture_ns",d.capture},{"width",1280},{"height",720},{"rotation",1}}},
   {"roi",{{"x",d.x},{"y",d.y},{"width",640},{"height",384},{"scale",1}}},{"source_png",d.file},{"source_sha256",d.hash},{"image_sha256",sha256_file(folder/"image.png")},
   {"sampling_reason","existing_single_frame_format_pilot"},{"review_status","proposed_visual_assistant"},{"author","Codex visible-only conservative polygons"},{"reviewer","independent human review pending"},
   {"annotation_version",1},{"runtime_identity_is_ground_truth",false},{"tags",i==0?json::array({"warm","partial","truncated"}):json::array({"gray","warm","head_occluded"})},
   {"objects",objects},
   {"unknown_regions","default ignore; effect boundaries and unseen head/tail not completed"},{"polygons",polygons}};
  write_json(folder/"annotation.json",annotation);rasterize_annotation(folder,annotation);
  samples.push_back({{"folder",folder.filename().string()},{"run_id",source.filename().string()},{"clip_id",annotation.at("clip_id")},{"split","development"},{"image_sha256",annotation.at("image_sha256")}});
  contact.push_back(load_diagnostic_png(folder/"overlay.png"));
 }
 Frame sheet;sheet.width=1280;sheet.height=384;sheet.stride=3840;sheet.rgb.resize(static_cast<std::size_t>(sheet.stride)*sheet.height);
 for(std::size_t i=0;i<contact.size();++i)for(int y=0;y<384;++y)std::copy_n(contact[i].rgb.data()+static_cast<std::size_t>(y)*1920,1920,sheet.rgb.data()+static_cast<std::size_t>(y)*3840+i*1920);
 write_diagnostic_png(output/"contact-sheet.png",sheet);
 write_json(output/"manifest.json",{{"schema",1},{"dataset_id","v75-single-frame-pilot"},{"samples",samples},{"training_ready",false},{"temporal_sequence",false},{"single_copy",true}});
 return validate_vision_dataset(output);
}
json benchmark_sampling_copy(int updates){
 if(updates<1||updates>100000)throw std::invalid_argument("copy benchmark updates");HostClock clock;Frame f;f.width=1280;f.height=720;f.stride=3840;f.source_rotation=1;f.epoch=f.generation=f.geometry_version=1;f.rgb.resize(1280*720*3,47);
 json batches=json::array();for(int batch=0;batch<3;++batch)for(int k=0;k<2;++k){const bool enabled=(k+batch)%2==1;GamePixelSampler sampler(1280,720,{enabled,enabled});std::vector<double> samples;samples.reserve(updates);
  for(int i=-500;i<updates;++i){f.sequence=static_cast<std::uint64_t>(i+501);f.capture_complete_ns=clock.now_ns();DecisionSnapshot s;s.context={1,1,1,f.sequence,f.capture_complete_ns,1280,720,1};s.playing_gate=true;s.ui=GameUi::playing;
   const auto begin=clock.now_ns();sampler.consume(f,s,{},i%257==0);const auto end=clock.now_ns();if(i>=0)samples.push_back((end-begin)/1e6);}
  batches.push_back({{"batch",batch},{"sampling_enabled",enabled},{"consume_copy_ms",distribution(samples)},{"stats",sampler.stats()}});}
 return {{"schema",1},{"offline_only",true},{"input_created",false},{"synthetic_uniform_pixels",true},{"unique_real_game_frames",0},{"geometry",{1280,720}},{"clock_domain","host_qpc_ns"},{"qpc_frequency",clock.frequency()},{"warmup_updates",500},{"batches",batches},{"performance_pass",nullptr}};
}
} // namespace pas
