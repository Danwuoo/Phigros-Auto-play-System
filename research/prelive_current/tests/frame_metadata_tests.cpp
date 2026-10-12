// Fixed cold metadata reuse controls; original control groups stay separate.
#include "frame_metadata.hpp"
#include <nlohmann/json.hpp>
#include <algorithm>
#include <array>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <vector>
#ifndef PAS_FRAME_METADATA_DELTA_SOURCE_COMMIT
#error "Explicit clean compiled cold metadata delta source required"
#endif
#ifndef PAS_FRAME_METADATA_DELTA_SOURCE_DIRTY
#error "Explicit compiled dirty state required"
#endif
namespace {
using J=nlohmann::json;using Ns=pas::Nanoseconds;namespace fs=std::filesystem;
using cold::frame_metadata::FixedMetadata;
constexpr std::size_t bytes=1280*720*3,case_count=8;
std::size_t expectations=0;
std::size_t baseline_pointer_changes=0,baseline_capacity_growth=0,stable_attempts=0;
void need(bool ok,const char* text){++expectations;if(!ok)throw std::runtime_error(text);}
template<class Body> void invalid(Body&& body,const char* text){bool caught=false;try{body();}catch(const std::invalid_argument& e){caught=true;need(std::string(e.what()).find(text)!=std::string::npos,"original invalid input message");}need(caught,"original invalid_argument required");}
struct TypedFakeClock final:pas::Clock {
 Ns value=100;mutable std::size_t reads=0;
 explicit TypedFakeClock(Ns v=100):value(v){}
 Ns now_ns()const override{++reads;return value;}
};
struct AssignmentFacts {bool data_changed=false,capacity_grew=false;};
pas::Frame fresh(std::uint64_t sequence,Ns render_end,AssignmentFacts* facts=nullptr){
 // Exact original cold chain Frame assignments, not a second reuse helper.
 pas::Frame f;f.sequence=sequence;f.epoch=f.generation=f.geometry_version=1;
 f.width=1280;f.height=720;f.stride=3840;f.source_rotation=1;
 const auto before=reinterpret_cast<std::uintptr_t>(f.capture_backend.data());const auto capacity=f.capture_backend.capacity();
 f.capture_backend="offline_memory_stub";
 if(facts){facts->data_changed=before!=reinterpret_cast<std::uintptr_t>(f.capture_backend.data());facts->capacity_grew=f.capture_backend.capacity()>capacity;}
 f.capture_complete_ns=f.pixels_ready_ns=render_end;return f;
}
bool equal(const pas::Frame& a,const pas::Frame& b){
 return a.sequence==b.sequence&&a.epoch==b.epoch&&a.generation==b.generation&&a.geometry_version==b.geometry_version&&
  a.width==b.width&&a.height==b.height&&a.stride==b.stride&&a.source_rotation==b.source_rotation&&a.normalization_rotation_degrees==b.normalization_rotation_degrees&&
  a.capture_backend==b.capture_backend&&a.source_pixel_format==b.source_pixel_format&&a.source_stride==b.source_stride&&
  a.crop_x==b.crop_x&&a.crop_y==b.crop_y&&a.crop_width==b.crop_width&&a.crop_height==b.crop_height&&a.source_valid==b.source_valid&&
  a.capture_complete_ns==b.capture_complete_ns&&a.pixels_ready_ns==b.pixels_ready_ns&&a.published_ns==b.published_ns&&
  a.receive_start_ns==b.receive_start_ns&&a.parse_start_ns==b.parse_start_ns&&a.parse_end_ns==b.parse_end_ns&&a.notification_received_ns==b.notification_received_ns&&
  a.copy_start_ns==b.copy_start_ns&&a.copy_end_ns==b.copy_end_ns&&a.readback_start_ns==b.readback_start_ns&&a.readback_end_ns==b.readback_end_ns&&
  a.decode_start_ns==b.decode_start_ns&&a.decode_end_ns==b.decode_end_ns&&a.source_qpc_ticks==b.source_qpc_ticks&&
  a.source_system_relative_100ns==b.source_system_relative_100ns&&a.codec_pts==b.codec_pts&&a.source_timestamp_us==b.source_timestamp_us&&
  a.source_sequence==b.source_sequence&&a.rgb==b.rgb;
}
std::vector<std::uint8_t> rgb(unsigned offset){std::vector<std::uint8_t> value(bytes);for(std::size_t i=0;i<bytes;++i)value[i]=static_cast<std::uint8_t>((i*17+offset)%251);return value;}
bool counters(const pas::FrameCounters& a,const pas::FrameCounters& b){return a.published==b.published&&a.overwritten==b.overwritten&&a.pool_drops==b.pool_drops&&a.consumer_skips==b.consumer_skips;}
void full_metadata(){
 FixedMetadata reused;
 for(std::uint64_t i=1;i<=1000;++i){AssignmentFacts facts;auto oracle=fresh(i,Ns(i)*20'000'000,&facts);const auto& actual=reused.for_attempt(i,Ns(i)*20'000'000);
  need(equal(actual,oracle),"all public Frame fields equal original fresh baseline");baseline_pointer_changes+=facts.data_changed;baseline_capacity_growth+=facts.capacity_grew;}
}
void stable_storage(){
 FixedMetadata reused;const auto& first=reused.for_attempt(1,20);const auto address=reinterpret_cast<std::uintptr_t>(first.capture_backend.data());const auto capacity=first.capture_backend.capacity();const auto* object=&first;
 for(std::uint64_t i=1;i<=1000;++i){const auto& value=reused.for_attempt(i,Ns(i)*20'000'000);
  need(&value==object&&reinterpret_cast<std::uintptr_t>(value.capture_backend.data())==address&&value.capture_backend.capacity()==capacity&&value.capture_backend.size()==19,"same immutable backend object/data/capacity across1000 attempts");
  need(value.source_pixel_format.empty()&&value.rgb.empty()&&value.rgb.capacity()==0,"original empty format/pixel metadata defaults retained");++stable_attempts;}
}
void typed_clock_and_domains(){
 TypedFakeClock clock(444);FixedMetadata reused;need(clock.reads==0,"helper construction does not read Clock");
 for(auto time:{Ns(0),Ns(-1),std::numeric_limits<Ns>::min(),std::numeric_limits<Ns>::max(),Ns(17)}){
  const auto reads=clock.reads;const auto& value=reused.for_attempt(0,time);need(clock.reads==reads&&equal(value,fresh(0,time)),"zero Clock reads or new timestamp-domain gate");}
 for(std::uint64_t i=1;i<=1000;++i){const auto end=clock.now_ns();const auto reads=clock.reads;const auto& value=reused.for_attempt(i,end);
  need(clock.reads==reads&&value.capture_complete_ns==end&&value.pixels_ready_ns==end,"only original render_end value propagated");}
 need(clock.reads==1000,"no extra or skipped caller Clock reads");
 need(noexcept(reused.for_attempt(1,1))&&!std::is_copy_constructible_v<FixedMetadata>&&!std::is_move_constructible_v<FixedMetadata>,"fixed single-producer instance cannot introduce copy/move churn");
}
void published_pixels_and_metadata(){
 TypedFakeClock clock(999);pas::LatestFrame baseline(1280,720,2,&clock),candidate(1280,720,2,&clock);FixedMetadata reused;auto pixels=rgb(3);
 auto metadata=fresh(1,200);need(baseline.publish(pixels.data(),pixels.size(),metadata)==candidate.publish(pixels.data(),pixels.size(),reused.for_attempt(1,200)),"same publish acceptance");
 auto a=baseline.read_after(0,0),b=candidate.read_after(0,0);need(a&&b&&equal(*a,*b),"all published Frame fields and pixels identical");
 need(b->published_ns==999&&b->rgb==pixels&&b->capture_backend=="offline_memory_stub","published clock override and provided bytes exact");
 need(counters(baseline.counters(),candidate.counters()),"publication/lease counters exact");
}
void all_leases_and_release(){
 pas::LatestFrame baseline(1280,720,2),candidate(1280,720,2);FixedMetadata reused;auto p=rgb(5),q=rgb(9);
 need(baseline.publish(p.data(),p.size(),fresh(1,10))&&candidate.publish(p.data(),p.size(),reused.for_attempt(1,10)),"lease fixture first");auto a1=baseline.read_after(0,0),b1=candidate.read_after(0,0);
 need(baseline.publish(q.data(),q.size(),fresh(2,20))&&candidate.publish(q.data(),q.size(),reused.for_attempt(2,20)),"lease fixture second");auto a2=baseline.read_after(1,0),b2=candidate.read_after(1,0);
 need(!baseline.publish(p.data(),p.size(),fresh(3,30))&&!candidate.publish(p.data(),p.size(),reused.for_attempt(3,30)),"all leased original pool drop");
 need(equal(*a1,*b1)&&equal(*a2,*b2)&&b1->rgb==p&&b2->rgb==q&&counters(baseline.counters(),candidate.counters()),"held bytes/metadata/counters unaffected by subsequent helper updates");
 a1.reset();b1.reset();need(baseline.publish(p.data(),p.size(),fresh(4,40))&&candidate.publish(p.data(),p.size(),reused.for_attempt(4,40)),"released physical slot reused");
 need(equal(*a2,*b2)&&b2->sequence==2&&b2->rgb==q,"live second lease cannot alias mutable helper");auto a4=baseline.read_after(2,0),b4=candidate.read_after(2,0);need(a4&&b4&&equal(*a4,*b4)&&counters(baseline.counters(),candidate.counters()),"new publication and consumer skip behavior exact");
}
void latest_reuse_and_skips(){
 pas::LatestFrame baseline(1280,720,2),candidate(1280,720,2);FixedMetadata reused;auto p=rgb(11),q=rgb(13);
 need(baseline.publish(p.data(),p.size(),fresh(1,10))&&candidate.publish(p.data(),p.size(),reused.for_attempt(1,10)),"reuse fixture first");auto held_a=baseline.read_after(0,0),held_b=candidate.read_after(0,0);
 need(baseline.publish(q.data(),q.size(),fresh(2,20))&&candidate.publish(q.data(),q.size(),reused.for_attempt(2,20)),"unconsumed latest second");
 need(baseline.publish(p.data(),p.size(),fresh(3,30))&&candidate.publish(p.data(),p.size(),reused.for_attempt(3,30)),"latest storage reused with other lease held");
 need(equal(*held_a,*held_b)&&held_b->sequence==1&&held_b->rgb==p,"old lease remains independent");auto a=baseline.read_after(1,0),b=candidate.read_after(1,0);
 need(a&&b&&equal(*a,*b)&&counters(baseline.counters(),candidate.counters()),"latest overwrite and skip behavior exact");need(candidate.counters().overwritten==1&&candidate.counters().consumer_skips==1,"existing reuse branch actually covered");
}
void invalid_sequence_payload_closed(){
 pas::LatestFrame baseline(1280,720,2),candidate(1280,720,2);FixedMetadata reused;auto p=rgb(17);
 need(baseline.publish(p.data(),p.size(),fresh(5,50))&&candidate.publish(p.data(),p.size(),reused.for_attempt(5,50)),"error fixture first");auto a=baseline.read_after(0,0),b=candidate.read_after(0,0);
 for(auto sequence:{std::uint64_t(5),std::uint64_t(4),std::uint64_t(0)}){
  invalid([&]{baseline.publish(p.data(),p.size(),fresh(sequence,60));},"sequence");invalid([&]{candidate.publish(p.data(),p.size(),reused.for_attempt(sequence,60));},"sequence");}
 invalid([&]{baseline.publish(p.data(),p.size()-1,fresh(6,60));},"invalid RGB frame");invalid([&]{candidate.publish(p.data(),p.size()-1,reused.for_attempt(6,60));},"invalid RGB frame");
 invalid([&]{baseline.publish(nullptr,p.size(),fresh(6,60));},"invalid RGB frame");invalid([&]{candidate.publish(nullptr,p.size(),reused.for_attempt(6,60));},"invalid RGB frame");
 need(equal(*a,*b)&&b->rgb==p&&counters(baseline.counters(),candidate.counters()),"invalid inputs cannot mutate current lease or counters");baseline.close();candidate.close();
 need(!baseline.publish(p.data(),p.size(),fresh(7,70))&&!candidate.publish(p.data(),p.size(),reused.for_attempt(7,70)),"closed return unchanged");
 need(!baseline.read_after(5,0)&&!candidate.read_after(5,0)&&equal(*a,*b),"closed state and retained leases exact");
}
void no_timestamp_feedback_and_lifetime(){
 pas::LatestFrame baseline(1280,720,2),candidate(1280,720,2);auto p=rgb(19);std::shared_ptr<const pas::Frame> a,b;
 {FixedMetadata reused;const auto& m1=reused.for_attempt(1,-17);need(baseline.publish(p.data(),p.size(),fresh(1,-17))&&candidate.publish(p.data(),p.size(),m1),"no new timestamp acceptance gate");
  a=baseline.read_after(0,0);b=candidate.read_after(0,0);const auto& m2=reused.for_attempt(2,300);
  need(m2.published_ns==0&&m2.capture_complete_ns==300&&m2.pixels_ready_ns==300&&m2.epoch==1,"publish cannot feed slot state back into helper or reset epoch");
  need(a&&b&&equal(*a,*b)&&b->capture_complete_ns==-17,"prior lease keeps original timestamp after helper update");}
 need(equal(*a,*b)&&b->capture_backend=="offline_memory_stub"&&b->rgb==p,"slot strings/pixels do not borrow helper lifetime");
}
struct Case {const char* id;void(*body)();};
const std::array<Case,case_count> cases{{
 {"full_public_metadata_original_baseline_1000",full_metadata},{"immutable_backend_storage_1000",stable_storage},
 {"typed_fake_clock_no_extra_reads_or_domain_gate",typed_clock_and_domains},{"latestframe_published_pixels_metadata_equal",published_pixels_and_metadata},
 {"all_leases_drop_release_equal",all_leases_and_release},{"latest_reuse_and_consumer_skips_equal",latest_reuse_and_skips},
 {"invalid_sequence_payload_and_closed_equal",invalid_sequence_payload_closed},{"no_slot_feedback_or_borrowed_helper_lifetime",no_timestamp_feedback_and_lifetime}
}};
}
int main(int argc,char** argv){try{
 if(argc!=2)throw std::runtime_error("controls FRESH_REPORT_JSON only");
 const std::string source=PAS_FRAME_METADATA_DELTA_SOURCE_COMMIT;const bool dirty=PAS_FRAME_METADATA_DELTA_SOURCE_DIRTY;
 if(source.size()!=40||source.find_first_not_of("0123456789abcdef")!=std::string::npos||dirty)throw std::runtime_error("clean compiled metadata delta source required");
 const auto report=fs::absolute(argv[1]);if(!fs::is_directory(report.parent_path())||fs::exists(report)||fs::is_symlink(fs::symlink_status(report)))throw std::runtime_error("fresh controls report path");
 auto reservation=report;reservation+=".reservation";if(!fs::create_directory(reservation))throw std::runtime_error("fresh controls report reservation");
 J checks=J::array();std::size_t failed=0;
 for(const auto& c:cases){const auto before=expectations;bool pass=true;std::string error;try{c.body();}catch(const std::exception& e){pass=false;error=e.what();if(error.size()>512)error.resize(512);}catch(...){pass=false;error="unknown exception";}
  failed+=!pass;checks.push_back({{"id",c.id},{"pass",pass},{"expectations",expectations-before},{"error",error}});if(!pass)std::cerr<<c.id<<": "<<error<<'\n';}
 const J value={{"schema","pas.cold-frame-metadata-reuse-controls.v1"},{"assertions",case_count},{"checks",checks},{"failed",failed},{"passed",case_count-failed},{"pass",failed==0},
  {"expectation_count",expectations},{"assertion_unit","one aggregate case; individual expectations separately counted"},
  {"source_build_commit",source},{"source_build_dirty",dirty},{"source_commit",source},{"source_dirty",dirty},
  {"synthetic_api_controls",true},{"component_memory_controls",true},{"device_endpoints",0},{"device_operations",0},{"etw_start_operations",0},{"etw_stop_operations",0},
  {"qualification_issued",false},{"cost_qualification",false},{"live_ready",false},{"performance_measurement",false},{"long_gap_root_cause_claim",false},
  {"helper_scope","AA cold chain only; no production capture/callback change"},{"metadata_width",1280},{"metadata_height",720},{"metadata_stride",3840},{"backend_size",19},
  {"storage_facts",{{"baseline_attempts",1000},{"baseline_backend_data_changed_assignments",baseline_pointer_changes},{"baseline_backend_capacity_growth_assignments",baseline_capacity_growth},
   {"candidate_stable_attempts",stable_attempts},{"raw_addresses_serialized",false},{"allocator_call_count_measured",false}}},{"pixels_written",0},{"original_control_denominators_modified",false}};
 const auto text=value.dump(2);if(text.size()+1>1024*1024)throw std::runtime_error("controls JSON1MiB cap");std::ofstream out(report,std::ios::binary);out<<text<<'\n';out.flush();if(!out)throw std::runtime_error("controls report write");return failed?1:0;
 }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 2;}}
