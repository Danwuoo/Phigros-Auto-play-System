#include "pas/game_dataset.hpp"
#include "pas/adb.hpp"
#include "pas/analysis.hpp"
#include <algorithm>
#include <cmath>
#include <fstream>
#include <set>
#include <stdexcept>
#include <thread>
#include <atomic>
#define NOMINMAX
#include <windows.h>
#include <wincodec.h>
#include <wrl/client.h>
#include <psapi.h>

namespace pas {
namespace {
using json=nlohmann::json;
constexpr std::size_t MiB=1024*1024;
bool compatible(const SceneContext& a,const SceneContext& b){return a.epoch==b.epoch&&a.generation==b.generation&&a.geometry==b.geometry&&a.width==b.width&&a.height==b.height&&a.rotation==b.rotation;}
json context_json(const SceneContext& c){return {{"epoch",c.epoch},{"generation",c.generation},{"geometry",c.geometry},{"frame",c.frame},{"capture_ns",c.capture_ns},{"width",c.width},{"height",c.height},{"rotation",c.rotation}};}
void write_json(const std::filesystem::path& p,const json& j){if(std::filesystem::exists(p))throw std::runtime_error("refusing to overwrite dataset file");std::ofstream out(p);if(!out||!(out<<j.dump(2)<<'\n'))throw std::runtime_error("dataset JSON write failed");}
json read_json(const std::filesystem::path& p){std::ifstream in(p);if(!in||std::filesystem::file_size(p)>16*MiB)throw std::runtime_error("dataset JSON missing/oversized");return json::parse(in);}
std::uint64_t working_set(){PROCESS_MEMORY_COUNTERS counters{};counters.cb=sizeof(counters);if(!K32GetProcessMemoryInfo(GetCurrentProcess(),&counters,sizeof(counters)))throw std::runtime_error("process memory measurement");return counters.WorkingSetSize;}
struct MemoryProbe {
 const std::uint64_t start=working_set();std::atomic<std::uint64_t> peak{start},samples{1};std::jthread worker;
 MemoryProbe():worker([this](std::stop_token stop){while(!stop.stop_requested()){const auto value=working_set();auto old=peak.load();while(old<value&&!peak.compare_exchange_weak(old,value)){}++samples;std::this_thread::sleep_for(std::chrono::milliseconds(1));}}){}
 void finish(){worker.request_stop();if(worker.joinable())worker.join();const auto value=working_set();peak=std::max(peak.load(),value);++samples;}
 ~MemoryProbe(){if(worker.joinable())finish();}
};
std::uint64_t disk_bytes(const std::filesystem::path& p){std::uint64_t bytes=0;std::size_t count=0;if(std::filesystem::exists(p))for(const auto& e:std::filesystem::recursive_directory_iterator(p)){if(++count>20000)throw std::runtime_error("dataset directory inventory cap");if(e.is_regular_file())bytes+=e.file_size();}return bytes;}
struct ComScope {HRESULT hr=CoInitializeEx(nullptr,COINIT_MULTITHREADED);ComScope(){if(FAILED(hr)&&hr!=RPC_E_CHANGED_MODE)throw std::runtime_error("dataset COM initialization");}~ComScope(){if(SUCCEEDED(hr))CoUninitialize();}};
template<class T>using Ptr=Microsoft::WRL::ComPtr<T>;
Ptr<IWICImagingFactory> factory(){Ptr<IWICImagingFactory> f;if(FAILED(CoCreateInstance(CLSID_WICImagingFactory,nullptr,CLSCTX_INPROC_SERVER,IID_PPV_ARGS(&f))))throw std::runtime_error("dataset WIC factory");return f;}
}
GamePixelSampler::GamePixelSampler(int w,int h,DatasetSamplingOptions o):width_(w),height_(h),roi_width_(std::min(w,640)),roi_height_(std::min(h,384)),options_(o){
 if(w<2||h<2||w>4096||h>4096)throw std::invalid_argument("sampler geometry");
 if(!o.dataset&&!o.diagnostics)return;
 const std::size_t full=static_cast<std::size_t>(w)*h*3,roi=static_cast<std::size_t>(roi_width_)*roi_height_*3;
 const std::size_t images=(o.dataset?128:0)+(o.diagnostics?8:0);
 const auto bytes=full*2+roi*images;
 // Reserve 8MiB for bounded metadata/explicit encode buffers within 128MiB.
 if(bytes>120*MiB)throw std::invalid_argument("sampler exceeds 128MiB combined budget");
 arena_.resize(bytes);clips_.reserve((o.dataset?16:0)+(o.diagnostics?2:0));
}
void GamePixelSampler::append(Clip& c,const SceneContext& ctx,const std::uint8_t* source){
 const auto roi_bytes=static_cast<std::size_t>(roi_width_)*roi_height_*3;
 const auto offset=static_cast<std::size_t>(width_)*height_*6+image_count_*roi_bytes;
 if(offset+roi_bytes>arena_.size())throw std::runtime_error("sampler arena capacity invariant");
 for(int row=0;row<c.height;++row)std::copy_n(source+(static_cast<std::size_t>(c.y+row)*width_+c.x)*3,c.width*3,arena_.data()+offset+static_cast<std::size_t>(row)*c.width*3);
 c.images.push_back({ctx,offset,c.x,c.y,c.width,c.height,c.reason});++image_count_;
}
void GamePixelSampler::consume(const Frame& f,const DecisionSnapshot& s,const std::optional<HoldDisappearance>& missing,bool combo){
 if(arena_.empty()||flushed_)return;
 if(!active_&&(!options_.diagnostics||diagnostic_count_==2)&&(!options_.dataset||(hard_count_==8&&normal_count_==4&&background_count_==4)))return;
 if(f.width!=width_||f.height!=height_||f.stride!=width_*3||f.rgb.size()!=static_cast<std::size_t>(width_)*height_*3)throw std::invalid_argument("sampler frame");
 const SceneContext expected_context{f.epoch,f.generation,f.geometry_version,f.sequence,f.capture_complete_ns,f.width,f.height,f.source_rotation};
 if(s.context!=expected_context)throw std::invalid_argument("sampler scene/frame mismatch: scene="+context_json(s.context).dump()+" frame="+context_json(expected_context).dump());
 const auto& ctx=s.context;
 if(history_count_){const auto& prior=history_[(history_cursor_+1)%2];if(!compatible(prior,ctx)||ctx.capture_ns<=prior.capture_ns||ctx.frame<=prior.frame){active_.reset();history_count_=0;}}
 if(!f.source_valid||!s.playing_gate){active_.reset();history_count_=0;++rejected_frames_;return;}
 if(!started_){started_=true;first_playing_ns_=ctx.capture_ns;}
 if(active_){auto& c=clips_[*active_];if(!compatible(c.trigger,ctx)||ctx.capture_ns-c.trigger.capture_ns>250'000'000)active_.reset();
  else {append(c,ctx,f.rgb.data());if(c.images.size()>=c.wanted)active_.reset();}}
 bool hard=missing.has_value()||combo;
 for(const auto& t:s.targets)if(t.reason=="association_ambiguous")hard=true;
 const auto elapsed=ctx.capture_ns-first_playing_ns_;std::string reason;bool diagnostic=false;
 if(hard&&options_.diagnostics&&diagnostic_count_<2){reason=missing?"hold_disappearance":combo?"combo_disappearance":"identity_competition";diagnostic=true;}
 else if(options_.dataset&&hard&&hard_count_<8)reason="hard_current_pixels";
 else if(options_.dataset&&normal_count_<4&&elapsed>=static_cast<Nanoseconds>(20+normal_count_*40)*1'000'000'000&&!s.targets.empty())reason="normal_stratum";
 else if(options_.dataset&&background_count_<4&&elapsed>=static_cast<Nanoseconds>(10+background_count_*40)*1'000'000'000)reason="detector_independent_background_stratum";
 if(!reason.empty()){
  if(active_){++dropped_triggers_;}
  else {
   Vec2 center{width_/2.,height_/2.};if(missing)center=missing->head;
   else if(reason=="detector_independent_background_stratum")center={static_cast<double>((background_count_*337+123)%width_),static_cast<double>((background_count_*173+97)%height_)};
   else if(!s.targets.empty())center=s.targets.front().note.center;
   Clip c;c.diagnostic=diagnostic;c.reason=reason;c.trigger=ctx;c.width=roi_width_;c.height=roi_height_;
   c.x=std::clamp(static_cast<int>(std::llround(center.x))-c.width/2,0,width_-c.width);
   c.y=std::clamp(static_cast<int>(std::llround(center.y))-c.height/2,0,height_-c.height);c.wanted=diagnostic?4:8;c.images.reserve(c.wanted);
   const auto pre=diagnostic?std::min(std::size_t{1},history_count_):history_count_;
   for(std::size_t i=history_count_-pre;i<history_count_;++i){const auto slot=(history_cursor_+2-history_count_+i)%2;
    if(compatible(history_[slot],ctx)&&ctx.capture_ns-history_[slot].capture_ns<=250'000'000)append(c,history_[slot],arena_.data()+slot*static_cast<std::size_t>(width_)*height_*3);}
   append(c,ctx,f.rgb.data());c.wanted=c.images.size()+(diagnostic?2:5);clips_.push_back(std::move(c));active_=clips_.size()-1;
   if(diagnostic)++diagnostic_count_;else if(reason=="hard_current_pixels")++hard_count_;else if(reason=="normal_stratum")++normal_count_;else ++background_count_;
  }
 }
 std::copy(f.rgb.begin(),f.rgb.end(),arena_.begin()+static_cast<std::ptrdiff_t>(history_cursor_*static_cast<std::size_t>(width_)*height_*3));
 history_[history_cursor_]=ctx;history_cursor_=(history_cursor_+1)%2;history_count_=std::min(std::size_t{2},history_count_+1);
}
json GamePixelSampler::stats() const{return {{"schema",1},{"diagnostics_enabled",options_.diagnostics},{"dataset_enabled",options_.dataset},{"allocated_raw_bytes",arena_.size()},
 {"combined_memory_budget_bytes",128*MiB},{"metadata_encode_explicit_allowance_bytes",8*MiB},{"images_retained",image_count_},{"diagnostic_clips",diagnostic_count_},{"hard_clips",hard_count_},
 {"normal_clips",normal_count_},{"background_clips",background_count_},{"dropped_triggers",dropped_triggers_},{"rejected_frames",rejected_frames_},{"real_input_created",false}};}
json GamePixelSampler::flush(const std::filesystem::path& root,const json& provenance,const Clock& clock,Nanoseconds timeout){
 if(flushed_)throw std::logic_error("sampler already flushed");flushed_=true;
 if(timeout<=0)throw std::invalid_argument("flush timeout");
 if(std::filesystem::exists(root))throw std::runtime_error("sampler output exists");
 const auto dataset_root=root.parent_path();const auto existing=disk_bytes(dataset_root);
 if(existing>=2ULL*1024*MiB)throw std::runtime_error("dataset disk cap reached");
 std::size_t runs=0,existing_images=0;
 if(std::filesystem::exists(dataset_root)){
  for(const auto& entry:std::filesystem::directory_iterator(dataset_root))if(entry.is_directory()&&std::filesystem::exists(entry.path()/"manifest.json"))++runs;
  for(const auto& entry:std::filesystem::recursive_directory_iterator(dataset_root))if(entry.is_regular_file()&&entry.path().filename()=="image.png")++existing_images;
 }
 if(runs>=10||existing_images>=1280)throw std::runtime_error("dataset global run/image cap reached");
 std::filesystem::create_directories(root);const auto begin=clock.now_ns();const auto ws_begin=working_set();auto ws_sampled_peak=ws_begin;json samples=json::array();std::uint64_t written=0;bool partial=false;std::string failure;
 MemoryProbe memory_probe;std::size_t truncated_clips=0;for(const auto& clip:clips_)truncated_clips+=clip.images.size()<clip.wanted;
 for(std::size_t ci=0;ci<clips_.size()&&!partial;++ci){const auto& clip=clips_[ci];for(std::size_t fi=0;fi<clip.images.size();++fi){const auto& image=clip.images[fi];
  const auto safe_reserve=static_cast<std::uint64_t>(image.width)*image.height*6+131072;
  if(clock.now_ns()-begin>=timeout||written+safe_reserve>256*MiB||existing+written+safe_reserve>2ULL*1024*MiB||existing_images+samples.size()>=1280){partial=true;failure="flush_time_disk_or_image_cap";break;}
  const auto folder="clip-"+std::to_string(ci)+"-frame-"+std::to_string(fi);std::filesystem::create_directory(root/folder);
  Frame rgb;rgb.width=image.width;rgb.height=image.height;rgb.stride=rgb.width*3;rgb.rgb.assign(arena_.begin()+static_cast<std::ptrdiff_t>(image.offset),arena_.begin()+static_cast<std::ptrdiff_t>(image.offset+static_cast<std::size_t>(rgb.stride)*rgb.height));
  ws_sampled_peak=std::max(ws_sampled_peak,working_set());
  try {write_diagnostic_png(root/folder/"image.png",rgb);ws_sampled_peak=std::max(ws_sampled_peak,working_set());const auto hash=sha256_file(root/folder/"image.png");
   json sidecar={{"schema",1},{"run_id",root.filename().string()},{"clip_id",std::to_string(ci)},{"context",context_json(image.context)},
    {"roi",{{"x",image.x},{"y",image.y},{"width",image.width},{"height",image.height},{"scale",1}}},{"image_sha256",hash},{"sampling_reason",image.reason},
    {"review_status","unannotated"},{"provenance",provenance},{"runtime_identity_is_ground_truth",false},{"objects",json::array()},{"polygons",json::array()}};
   write_json(root/folder/"annotation.json",sidecar);samples.push_back({{"folder",folder},{"run_id",root.filename().string()},{"clip_id",std::to_string(ci)},{"split","development"},{"image_sha256",hash},{"diagnostic",clip.diagnostic}});
   written+=std::filesystem::file_size(root/folder/"image.png")+std::filesystem::file_size(root/folder/"annotation.json");
  }catch(const std::exception& e){partial=true;failure=e.what();break;}
 }}
 memory_probe.finish();ws_sampled_peak=std::max(ws_sampled_peak,memory_probe.peak.load());
 json manifest={{"schema",1},{"dataset_id",root.filename().string()},{"samples",samples},{"training_ready",false},{"partial",partial},{"failure",failure},{"truncated_clips",truncated_clips},{"stats",stats()},
  {"written_bytes",written},{"single_copy",true},{"flush_duration_ns",clock.now_ns()-begin},{"explicit_encode_scratch_upper_bytes",static_cast<std::uint64_t>(roi_width_)*roi_height_*6},
  {"encoding_process_working_set_start_bytes",ws_begin},{"encoding_process_working_set_sampled_peak_bytes",ws_sampled_peak},
  {"encoding_working_set_sampled_growth_bytes",ws_sampled_peak-ws_begin},{"encoding_memory_probe_samples",memory_probe.samples.load()},{"encoding_memory_probe_interval_ms",1},
  {"scratch_measurement_scope","explicit buffers bounded; sampled 1ms process working set during flush includes WIC, transient sub-ms peak unknown"}};
 write_json(root/"manifest.json",manifest);arena_.clear();arena_.shrink_to_fit();return manifest;
}
void write_label_png(const std::filesystem::path& path,const LabelMask& m){
 if(std::filesystem::exists(path)||m.width<1||m.height<1||m.width>4096||m.height>4096||(m.bits!=8&&m.bits!=16)||m.values.size()!=static_cast<std::size_t>(m.width)*m.height)throw std::invalid_argument("label image");
 ComScope co;auto f=factory();Ptr<IWICStream> stream;Ptr<IWICBitmapEncoder> encoder;Ptr<IWICBitmapFrameEncode> image;Ptr<IPropertyBag2> props;
 if(FAILED(f->CreateStream(&stream))||FAILED(stream->InitializeFromFilename(path.c_str(),GENERIC_WRITE))||FAILED(f->CreateEncoder(GUID_ContainerFormatPng,nullptr,&encoder))||FAILED(encoder->Initialize(stream.Get(),WICBitmapEncoderNoCache))||FAILED(encoder->CreateNewFrame(&image,&props))||FAILED(image->Initialize(props.Get()))||FAILED(image->SetSize(m.width,m.height)))throw std::runtime_error("mask PNG initialization");
 GUID format=m.bits==8?GUID_WICPixelFormat8bppGray:GUID_WICPixelFormat16bppGray;const auto expected=format;
 std::vector<std::uint8_t> bytes(m.values.size()*(m.bits/8));for(std::size_t i=0;i<m.values.size();++i){if(m.bits==8&&m.values[i]>255)throw std::invalid_argument("8-bit label overflow");bytes[i*(m.bits/8)]=static_cast<std::uint8_t>(m.values[i]&255);if(m.bits==16)bytes[i*2+1]=static_cast<std::uint8_t>(m.values[i]>>8);}
 if(FAILED(image->SetPixelFormat(&format))||!InlineIsEqualGUID(format,expected)||FAILED(image->WritePixels(m.height,m.width*(m.bits/8),static_cast<UINT>(bytes.size()),bytes.data()))||FAILED(image->Commit())||FAILED(encoder->Commit()))throw std::runtime_error("mask PNG write");
}
LabelMask read_label_png(const std::filesystem::path& path,int bits){
 if(bits!=8&&bits!=16)throw std::invalid_argument("label depth");if(std::filesystem::file_size(path)>16*MiB)throw std::invalid_argument("label PNG capacity");
 ComScope co;auto f=factory();Ptr<IWICBitmapDecoder> decoder;Ptr<IWICBitmapFrameDecode> image;
 if(FAILED(f->CreateDecoderFromFilename(path.c_str(),nullptr,GENERIC_READ,WICDecodeMetadataCacheOnLoad,&decoder))||FAILED(decoder->GetFrame(0,&image)))throw std::runtime_error("mask PNG read");
 UINT w=0,h=0;GUID format;if(FAILED(image->GetSize(&w,&h))||FAILED(image->GetPixelFormat(&format))||w==0||h==0||w>4096||h>4096||!InlineIsEqualGUID(format,bits==8?GUID_WICPixelFormat8bppGray:GUID_WICPixelFormat16bppGray))throw std::invalid_argument("mask PNG geometry/format");
 std::vector<std::uint8_t> bytes(static_cast<std::size_t>(w)*h*(bits/8));if(FAILED(image->CopyPixels(nullptr,w*(bits/8),static_cast<UINT>(bytes.size()),bytes.data())))throw std::runtime_error("mask PNG pixels");
 LabelMask result{static_cast<int>(w),static_cast<int>(h),bits,{}};result.values.resize(static_cast<std::size_t>(w)*h);
 for(std::size_t i=0;i<result.values.size();++i)result.values[i]=bits==8?bytes[i]:static_cast<std::uint16_t>(bytes[i*2]|bytes[i*2+1]<<8);return result;
}
std::pair<LabelMask,LabelMask> annotation_masks(int width,int height,const json& a){
 if(width<1||height<1||width>640||height>384)throw std::invalid_argument("mask ROI dimensions");
 LabelMask semantic{width,height,8,std::vector<std::uint16_t>(static_cast<std::size_t>(width)*height,255)};
 LabelMask instance{width,height,16,std::vector<std::uint16_t>(semantic.values.size(),65535)};
 if(a.at("polygons").size()>256)throw std::invalid_argument("annotation polygon cap");
 for(const auto& poly:a.at("polygons")){const int cls=poly.at("semantic"),id=poly.at("instance");if((cls<0||cls>8)&&cls!=255)throw std::invalid_argument("semantic class");if(id<0||id>65535)throw std::invalid_argument("instance class");
  if((cls==255&&id!=65535)||((cls==0||cls==8)&&id!=0)||(cls>=1&&cls<=7&&(id==0||id==65535)))throw std::invalid_argument("semantic/instance contract");
  if(poly.at("points").size()<3||poly.at("points").size()>256)throw std::invalid_argument("polygon vertices");
  std::vector<Vec2> points;for(const auto& p:poly.at("points")){if(!p.is_array()||p.size()!=2)throw std::invalid_argument("polygon point shape");Vec2 v{p.at(0),p.at(1)};if(!std::isfinite(v.x)||!std::isfinite(v.y)||v.x<0||v.y<0||v.x>width||v.y>height)throw std::invalid_argument("polygon outside ROI");points.push_back(v);}
  if(points.size()<3||points.size()>256)throw std::invalid_argument("polygon vertices");
  for(int y=0;y<height;++y)for(int x=0;x<width;++x){bool inside=false;for(std::size_t i=0,j=points.size()-1;i<points.size();j=i++){
   const auto u=points[i],v=points[j];if((u.y>y+.5)!=(v.y>y+.5)&&(x+.5)<(v.x-u.x)*(y+.5-u.y)/(v.y-u.y)+u.x)inside=!inside;}
   if(inside){const auto offset=static_cast<std::size_t>(y)*width+x;semantic.values[offset]=static_cast<std::uint16_t>(cls);instance.values[offset]=static_cast<std::uint16_t>(id);}}
 }
 return {std::move(semantic),std::move(instance)};
}
void rasterize_annotation(const std::filesystem::path& folder,const json& a){
 const auto f=load_diagnostic_png(folder/"image.png");if(a.at("image_sha256")!=sha256_file(folder/"image.png"))throw std::invalid_argument("annotation source hash");
 const auto [semantic,instance]=annotation_masks(f.width,f.height,a);
 write_label_png(folder/"semantic.png",semantic);write_label_png(folder/"instance.png",instance);
 auto overlay=f;constexpr std::array<std::array<std::uint8_t,3>,9> colors{{{0,0,0},{255,255,255},{0,255,0},{255,128,0},{0,190,255},{180,0,255},{255,255,0},{255,0,0},{255,190,50}}};
 for(std::size_t i=0;i<semantic.values.size();++i)if(semantic.values[i]!=255){const auto& color=colors[semantic.values[i]];for(int ch=0;ch<3;++ch)overlay.rgb[i*3+ch]=static_cast<std::uint8_t>((overlay.rgb[i*3+ch]+color[ch])/2);}
 write_diagnostic_png(folder/"overlay.png",overlay);
}
} // namespace pas
