#include "pas/lossless_event_stream.hpp"
#include "../prelive_current/evidence_hash.hpp"
#include <nlohmann/json.hpp>
#include <iostream>
#include <map>
#include <set>
using J=nlohmann::json;namespace fs=std::filesystem;
struct Count {std::uint64_t rows=0,bytes=0,max_bytes=0;};
int main(int argc,char**argv){try{
 if(argc!=4||fs::exists(argv[3]))throw std::runtime_error("usage input-gzip expected-SHA fresh-output");
 const fs::path input=fs::absolute(argv[1]);const std::string expected=argv[2];
 if(fs::file_size(input)>512ull*1024*1024||pas::evidence::sha256(input)!=expected)throw std::runtime_error("bound input SHA/bytes");
 pas::LosslessEventReader reader(input,16*1024*1024);std::string line;
 std::map<std::string,Count> events;std::map<std::uint64_t,std::map<std::string,std::uint64_t>> frames;
 std::uint64_t rows=0;
 while(reader.next(line)){
  if(++rows>1000000)throw std::runtime_error("row bound");
  std::vector<std::set<std::string>> keys;const auto v=J::parse(line,[&](int depth,J::parse_event_t e,J& val){
   if(depth>64)throw std::runtime_error("depth bound");
   if(e==J::parse_event_t::object_start)keys.emplace_back();
   if(e==J::parse_event_t::key&&!keys.back().insert(val.get<std::string>()).second)throw std::runtime_error("duplicate key");
   if(e==J::parse_event_t::object_end)keys.pop_back();return true;});
  const auto name=v.value("event",std::string("(no event field)"));
  if(name.size()>128||(!events.contains(name)&&events.size()==128))throw std::runtime_error("event key bound");
  auto& c=events[name];++c.rows;c.bytes+=line.size();c.max_bytes=std::max(c.max_bytes,std::uint64_t(line.size()));
  std::uint64_t frame=v.value("source_frame",std::uint64_t{0});
  if(frame){if(frame>1000000||(!frames.contains(frame)&&frames.size()==10000))throw std::runtime_error("frame bound");++frames[frame][name];}
 }
 if(pas::evidence::sha256(input)!=expected)throw std::runtime_error("input changed while read");
 J counts=J::array(),per_frame=J::array();for(const auto&[name,c]:events)counts.push_back({{"event",name},{"rows",c.rows},{"encoded_row_bytes",c.bytes},{"max_row_bytes",c.max_bytes}});
 for(const auto&[frame,c]:frames)per_frame.push_back({{"frame",frame},{"events",c}});
 const J result={{"schema","pas.partial-archive-diagnosis.v1"},{"input",{{"path",input.string()},{"sha256",expected},{"bytes",fs::file_size(input)}}},{"rows",rows},{"decoded_bytes",reader.decoded_bytes()},{"events",counts},{"source_frame_groups",per_frame},{"scope","full CRC-checked retained segment only; partial round, no complete attempt, cost, physical or live qualification"},{"device_operations",0},{"live_ready",false}};
 std::ofstream output(argv[3],std::ios::binary);output<<result.dump(2)<<'\n';if(!output)throw std::runtime_error("write output");
 std::cout<<"rows="<<rows<<" events="<<events.size()<<" source_frames="<<frames.size()<<'\n';return 0;
 }catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 2;}}
