// Isolated algorithm controls compile the actual cooperative implementation.
// Link only the unchanged cold-v3 donor; do not link own_bridge as well.
#include "../extract.cpp"
#include "observation_equal.hpp"
#include <nlohmann/json.hpp>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#ifdef _WIN32
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

namespace {
using J=nlohmann::json;
using bvi::owner_cooperative::cold_verification::observation_equal;
constexpr std::uint64_t prime=1099511628211ULL,initial=1469598103934665603ULL;
struct Tests{J checks=J::array();int failed=0;void check(std::string id,bool pass){checks.push_back({{"id",std::move(id)},{"pass",pass}});failed+=!pass;}};
std::uint64_t scalar(std::uint64_t h,const std::uint8_t* bytes,std::size_t size){for(std::size_t i=0;i<size;++i)h=(h^bytes[i])*prime;return h;}
std::uint64_t actual(std::uint64_t h,const std::uint8_t* bytes,std::size_t size){return bvi::owner_cooperative::hash_rgb_bytes(h,bytes,size);}
void byte_controls(Tests& t){
 t.check("fixed_constexpr_table_512kib",sizeof(bvi::owner_cooperative::uniform_table)==524288);
 bool pass=true;std::array<std::uint8_t,8> bytes{};
 constexpr std::array<std::uint64_t,4> high={0,0x0123456789abcd00ULL,0xffffffffffffff00ULL,initial&~255ULL};
 for(unsigned byte=0;byte<256;++byte){bytes.fill(static_cast<std::uint8_t>(byte));for(unsigned low=0;low<256;++low)for(auto upper:high){const auto h=upper|low;pass&=actual(h,bytes.data(),bytes.size())==scalar(h,bytes.data(),bytes.size());}}
 t.check("all_256_bytes_all_256_low_states_four_high_states",pass);
 pass=true;std::array<std::uint8_t,80> storage{};
 for(unsigned byte=0;byte<256;++byte){storage.fill(static_cast<std::uint8_t>(byte));for(std::size_t offset=0;offset<8;++offset)for(std::size_t size=0;size<=65;++size)pass&=actual(initial,storage.data()+offset,size)==scalar(initial,storage.data()+offset,size);}
 t.check("all_bytes_unaligned_and_every_tail_length",pass);
 pass=true;std::uint64_t random=0x4c3e280719af52d1ULL;std::array<std::uint8_t,1032> mixed{};
 for(auto& byte:mixed){random=random*6364136223846793005ULL+1442695040888963407ULL;byte=static_cast<std::uint8_t>(random>>32);}
 for(std::size_t offset=0;offset<8;++offset)for(std::size_t size=0;size<=1024;++size)pass&=actual(initial^size,mixed.data()+offset,size)==scalar(initial^size,mixed.data()+offset,size);
 t.check("random_nonuniform_original_scalar_fallback",pass);
 pass=true;for(unsigned byte=0;byte<256;++byte)for(std::size_t lane=0;lane<8;++lane){bytes.fill(30);bytes[lane]=static_cast<std::uint8_t>(byte);pass&=actual(initial,bytes.data(),8)==scalar(initial,bytes.data(),8);}
 t.check("each_lane_mutation_is_read",pass);
 pass=true;for(std::size_t i=0;i<mixed.size();++i)mixed[i]=(i/8)%3==0?0:(i/8)%3==1?30:static_cast<std::uint8_t>(i*37+i/67);
 for(std::size_t offset=0;offset<8;++offset)for(std::size_t size=0;size<=1024;++size)pass&=actual(~initial,mixed.data()+offset,size)==scalar(~initial,mixed.data()+offset,size);
 t.check("mixed_uniform_zero_nonzero_and_scalar_blocks",pass);
#ifdef _WIN32
 SYSTEM_INFO info{};GetSystemInfo(&info);const auto page=static_cast<std::size_t>(info.dwPageSize);
 if(page<4096||page>65536)throw std::runtime_error("bounded guard-page size");
 auto* allocation=static_cast<std::uint8_t*>(VirtualAlloc(nullptr,page*2,MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE));if(!allocation)throw std::runtime_error("guard-page allocation");
 DWORD previous{};if(!VirtualProtect(allocation+page,page,PAGE_NOACCESS,&previous)){VirtualFree(allocation,0,MEM_RELEASE);throw std::runtime_error("guard-page protection");}
 pass=true;for(std::size_t size=0;size<=65;++size){auto* start=allocation+page-size;for(std::size_t i=0;i<size;++i)start[i]=i/8%2?static_cast<std::uint8_t>(i*31):30;pass&=actual(initial,start,size)==scalar(initial,start,size);}
 if(!VirtualFree(allocation,0,MEM_RELEASE))throw std::runtime_error("guard-page release");
 t.check("guard_page_last_byte_no_overread",pass);
#else
 t.check("bounded_exact_span_end",actual(initial,mixed.data()+mixed.size()-65,65)==scalar(initial,mixed.data()+mixed.size()-65,65));
#endif
}
struct Fixture{
 bvi::Candidate donor;const bvi::Key key{{1,1,1,1},10000000,10000000,1,true};
 const std::array<bvi::Query,4> queries={bvi::Query{{400,500},100,300,0,false},bvi::Query{{500,500},100,8,0,true},bvi::Query{{600,400},80,260,.7,false},bvi::Query{{800,500},70,400,-.6,false}};
 const std::array<bvi::Line,2> lines={bvi::Line{{640,500},0,1200,7},bvi::Line{{800,400},.7,500,8}};
 bool parity(std::span<const std::uint8_t> rgb,int width,int height,int stride){const bvi::View view{rgb,width,height,stride,key};std::size_t polls=0;const auto expected=donor.extract(view,queries,lines);const auto result=bvi::owner_cooperative::extract(donor,view,queries,lines,[&](){++polls;return true;});return observation_equal(result,expected)&&polls>=static_cast<std::size_t>((height+31)/32)+queries.size();}
};
void extraction_controls(Tests& t){
 Fixture f;bool pass=true;
 for(unsigned byte=0;byte<256;++byte){std::vector<std::uint8_t> storage(32*33+7,static_cast<std::uint8_t>(byte));pass&=f.parity({storage.data()+7,32*33},9,33,32);}
 t.check("all_byte_values_complete_donor_observation",pass);
 std::vector<std::uint8_t> storage(3848*720+7,0);auto rgb=std::span<std::uint8_t>(storage).subspan(7);
 for(const auto byte:{0,30,255}){std::fill(rgb.begin(),rgb.end(),static_cast<std::uint8_t>(byte));t.check("full_1280x720_donor_"+std::to_string(byte),f.parity(rgb,1280,720,3848));}
 std::uint64_t random=0x736491ac48210edfULL;for(auto& byte:rgb){random=random*6364136223846793005ULL+1442695040888963407ULL;byte=static_cast<std::uint8_t>(random>>32);}
 t.check("full_1280x720_random_donor",f.parity(rgb,1280,720,3848));
 for(std::size_t i=0;i<rgb.size();++i)rgb[i]=(i/8)%3==0?0:(i/8)%3==1?30:static_cast<std::uint8_t>(i*37+i/67);
 t.check("full_1280x720_mixed_donor",f.parity(rgb,1280,720,3848));
 pass=true;for(const auto width:{1,2,3,5,7,8,9,1279}){const int stride=width*3+5;std::vector<std::uint8_t> odd(static_cast<std::size_t>(stride)*33+3,30);for(std::size_t i=3;i<odd.size();++i)if(i/8%3==1)odd[i]=static_cast<std::uint8_t>(i*41);pass&=f.parity({odd.data()+3,odd.size()-3},width,33,stride);}
 t.check("odd_width_unaligned_row_tail_complete_donor",pass);
 std::fill(rgb.begin(),rgb.end(),30);const bvi::View view{rgb,1280,720,3848,f.key};const auto before=bvi::owner_cooperative::extract(f.donor,view,f.queries,f.lines,[](){return true;});
 for(int y=0;y<720;++y)for(int x=3840;x<3848;++x)rgb[static_cast<std::size_t>(y)*3848+x]=255;
 const auto padded=bvi::owner_cooperative::extract(f.donor,view,f.queries,f.lines,[](){return true;});
 t.check("padding_never_enters_complete_observation",observation_equal(before,padded));
 rgb[719*3848+3839]^=1;const auto changed=bvi::owner_cooperative::extract(f.donor,view,f.queries,f.lines,[](){return true;});
 t.check("last_rgb_byte_changes_exact_donor_signature",changed.rgb_signature!=padded.rgb_signature&&observation_equal(changed,f.donor.extract(view,f.queries,f.lines)));
 std::size_t polls=0;const auto stopped=bvi::owner_cooperative::extract(f.donor,view,f.queries,f.lines,[&](){return ++polls<3;});const bvi::View prefix{rgb.first(3848*64),1280,64,3848,f.key};
 t.check("abort_exact_same_64_rows_probes_signature",stopped.invalid&&stopped.reason=="owner checkpoint stopped"&&stopped.probes==64*1280&&polls==3&&stopped.rgb_signature==f.donor.extract(prefix,{},{}).rgb_signature);
 bool threw=false;try{bvi::owner_cooperative::extract(f.donor,view,f.queries,f.lines,[]()->bool{throw std::runtime_error("checkpoint fault");});}catch(const std::runtime_error&){threw=true;}
 t.check("checkpoint_exception_unchanged",threw);
 polls=0;const bvi::View invalid{{},1280,720,3848,f.key};const auto rejected=bvi::owner_cooperative::extract(f.donor,invalid,f.queries,f.lines,[&](){++polls;return true;});
 t.check("invalid_shape_rejected_before_read_or_poll",rejected.invalid&&rejected.reason=="frame/source invalid"&&polls==0&&rejected.probes==0);
}
#ifdef _MSC_VER
#define PAS_NOINLINE __declspec(noinline)
#else
#define PAS_NOINLINE __attribute__((noinline))
#endif
PAS_NOINLINE std::uint64_t bench_scalar(std::uint64_t h,const std::uint8_t* bytes,std::size_t size){return scalar(h,bytes,size);}
PAS_NOINLINE std::uint64_t bench_actual(std::uint64_t h,const std::uint8_t* bytes,std::size_t size){return actual(h,bytes,size);}
J distribution(std::vector<double> values){std::sort(values.begin(),values.end());return {{"n",values.size()},{"p50_ms",values[values.size()/2]},{"p95_ms",values.back()},{"p99_ms",values.back()},{"max_ms",values.back()}};}
J microbench(){
 J results=J::array();std::vector<std::uint8_t> rgb(1280*720*3);std::uint64_t checksum=0;
 for(const auto pattern:{"uniform0","uniform30","random","mixed"}){
  std::uint64_t random=0x71547a1be09d623fULL;
  for(std::size_t i=0;i<rgb.size();++i){random=random*6364136223846793005ULL+1442695040888963407ULL;rgb[i]=pattern==std::string_view("uniform0")?0:pattern==std::string_view("uniform30")?30:pattern==std::string_view("mixed")&&i/8%3!=2?static_cast<std::uint8_t>(i/8%3?30:0):static_cast<std::uint8_t>(random>>32);}
  for(int i=0;i<3;++i){checksum^=bench_scalar(initial^i,rgb.data(),rgb.size());checksum^=bench_actual(initial^i,rgb.data(),rgb.size());}
  std::vector<double> old_times,new_times;
  const auto measure=[&](bool optimized,int iteration){const auto start=std::chrono::steady_clock::now();const auto h=optimized?bench_actual(initial^iteration,rgb.data(),rgb.size()):bench_scalar(initial^iteration,rgb.data(),rgb.size());const auto end=std::chrono::steady_clock::now();checksum^=h;(optimized?new_times:old_times).push_back(std::chrono::duration<double,std::milli>(end-start).count());};
  for(int i=0;i<9;++i){measure(i%2!=0,i);measure(i%2==0,i);}
  results.push_back({{"pattern",pattern},{"bytes_per_iteration",rgb.size()},{"scalar",distribution(old_times)},{"uniform8",distribution(new_times)}});
 }
 return {{"scope","isolated memory algorithm only; no candidate/cost qualification"},{"samples_each",9},{"checksum",checksum},{"patterns",results}};
}
}
int main(int argc,char** argv){try{
 if(argc!=2||std::filesystem::exists(argv[1]))throw std::runtime_error("one fresh output path required");Tests t;byte_controls(t);extraction_controls(t);
 J report={{"schema","pas.uniform-rgb-fnv-controls.v1"},{"pass",t.failed==0},{"assertions",t.checks.size()},{"checks",t.checks},{"failed",t.failed},{"uniform_bytes",256},{"low_states",256},{"high_states",4},{"uniform_state_comparisons",262144},{"table_bytes",524288},{"frame_cache_bytes",0},{"device_endpoints",0},{"device_operations",0},{"cost_qualification",false},{"live_ready",false},{"microbench",microbench()}};
 std::ofstream output(argv[1]);output<<report.dump(2)<<'\n';if(!output)throw std::runtime_error("report write");std::cout<<"assertions="<<t.checks.size()<<" failed="<<t.failed<<'\n';return t.failed?1:0;
 }catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 2;}}
