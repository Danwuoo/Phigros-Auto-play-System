#include "contract.hpp"
#include "meter_touch.hpp"
#include "archive_verify.hpp"
#include "pas/session_archive.hpp"
#include "pas/analysis.hpp"
#include <gtest/gtest.h>
#include <cstdlib>
#include <thread>
using namespace r3;
namespace fs=std::filesystem;
TEST(R3,AdmissionKeepsFullPayloadAndRejectsAtomically){json payload={{"event","full"},{"nested",{{"text","\n\r\n"},{"steps",{1,2,3}}}}};json envelope=payload;envelope["schema_version"]=2;envelope["clock_domain"]="host_qpc_ns";envelope["round_id"]=1;auto expected=envelope.dump()+"\n";Admission a;a.byte_limit=expected.size()*2;a.count_limit=2;
 EXPECT_EQ(a.admit(payload),expected.size());
 EXPECT_EQ(a.admit(payload),expected.size());
 EXPECT_THROW(a.admit(payload),std::runtime_error);
 EXPECT_EQ(a.bytes,expected.size()*2);
 EXPECT_EQ(a.count,2);a=Admission{};a.byte_limit=expected.size()-1;
 EXPECT_THROW(a.admit(payload),std::runtime_error);
 EXPECT_EQ(a.count,0);
 EXPECT_EQ(a.bytes,0);a=Admission{};json oversize={{"event","full"},{"text",std::string(row_bytes,'x')}};
 EXPECT_THROW(a.admit(oversize),std::runtime_error);
 EXPECT_EQ(a.count,0);
 EXPECT_EQ(payload.at("nested").at("steps").size(),3);
 EXPECT_EQ(std::count(expected.begin(),expected.end(),'\n'),1);}
fs::path root(const std::string& n){const auto p=std::getenv("R3_TEST_ROOT");if(!p)throw std::runtime_error("R3_TEST_ROOT required");auto r=fs::path(p)/n;require(!fs::exists(r),"existing fixture");fs::create_directories(r);return r;}
TEST(R3,BoundedReaderExactlyAtAndAboveCount){std::ostringstream s;for(std::size_t i=0;i<events;++i)s<<"{}\n";std::istringstream good(s.str());std::size_t n=0;
 EXPECT_NO_THROW(rows(good,events,3,[&](auto){++n;}));
 EXPECT_EQ(n,events);std::istringstream bad(s.str()+"{}\n");
 EXPECT_THROW(rows(bad,events,3,[](auto){}),std::runtime_error);}
TEST(R3,ReaderRowBoundaryTruncationEmptyAndMalformed){std::istringstream exact("{}\n");
 EXPECT_NO_THROW(rows(exact,1,2,[](auto){}));std::istringstream too("{ }\n");
 EXPECT_THROW(rows(too,1,2,[](auto){}),std::runtime_error);for(auto s:{"{}","\n","{\n"}){std::istringstream in(s);
 EXPECT_ANY_THROW(rows(in,1,20,[](auto){}));}std::istringstream crlf("{}\r\n");
 EXPECT_NO_THROW(rows(crlf,1,3,[](auto){}));}
TEST(R3,MetricMissingEmptyWrongCountAndNonfiniteFail){auto d=distribution({1,2,3});
 EXPECT_NO_THROW(check(d,{1,2,3}));
 EXPECT_THROW(check(d,{1,2}),std::runtime_error);auto bad=d;bad["p99"]=nullptr;
 EXPECT_ANY_THROW(check(bad,{1,2,3}));
 EXPECT_TRUE(!r1::metric(distribution({}),0));
 EXPECT_THROW(distribution({std::numeric_limits<double>::infinity()}),std::runtime_error);}
json valid_run(){auto m=[](int n){return json{{"n",n},{"p50",1},{"p95",1},{"p99",1},{"max",1},{"jitter_p95_minus_p5",0}};};return {{"attempts",2560},{"published",2560},{"pool_drops",0},{"consumed",2560},{"owner_seen",2560},{"receipt_n",1},{"release_n",1},{"action_expectation","required"},{"hard_gate",true},{"raw_complete",true},{"metrics_ms",{{"recognition",m(2560)},{"owner",m(2560)},{"capture_to_owner",m(2560)},{"lateness",m(1)}}},{"windows",{{"measurement",{{"attempts",2304},{"consumed",2304},{"owner_seen",2304},{"metrics_ms",{{"recognition",m(2304)},{"owner",m(2304)},{"capture_to_owner",m(2304)}}}}}}}};}
TEST(R3,LongWindowGateRejectsMissingSamplesAndWarmupHardFailures){auto r=valid_run();
 EXPECT_TRUE(normal(r));r["windows"]["measurement"]["owner_seen"]=1999;
 EXPECT_FALSE(normal(r));r=valid_run();r["windows"]["measurement"]["metrics_ms"]["recognition"]["n"]=0;
 EXPECT_FALSE(normal(r));r=valid_run();r["metrics_ms"]["lateness"]["max"]=101;
 EXPECT_FALSE(normal(r));r=valid_run();r["raw_complete"]=false;
 EXPECT_FALSE(normal(r));r=valid_run();r["action_expectation"]="none";r["receipt_n"]=0;
 EXPECT_FALSE(normal(r));}
TEST(R3,WindowBoundariesAndAllFailureDenominators){Windows w;for(std::size_t i=1;i<=attempts;++i)w.frame({{"attempt",i},{"published",true},{"consumed",false},{"owned",false},{"publish_cost_ns",100}});auto j=w.report();
 EXPECT_EQ(j["warmup"]["attempts"],256);
 EXPECT_EQ(j["measurement"]["attempts"],2304);for(auto b:j["blocks"]){
 EXPECT_EQ(b["attempts"],576);
 EXPECT_EQ(b["consumer_skips"],576);
 EXPECT_EQ(b["metrics_ms"]["owner"]["n"],0);}
 EXPECT_EQ(j["all"]["metrics_ms"]["publish"]["n"],2560);}
TEST(R3,TouchReceiptAndReleaseHardBoundary){pas::FakeClock c;r3::MeterTouch t(c,0);for(std::size_t i=0;i<receipts;++i){pas::TouchCommand cmd{};cmd.contact_id=0;cmd.phase=i%2?pas::Phase::up:pas::Phase::down;
 EXPECT_NO_THROW(t.inject(cmd));}pas::TouchCommand extra{};extra.contact_id=0;extra.phase=pas::Phase::down;
 EXPECT_THROW(t.inject(extra),std::runtime_error);
 EXPECT_TRUE(t.contacts.empty());for(std::size_t i=0;i<releases;++i)
 EXPECT_NO_THROW(t.release_all());
 EXPECT_THROW(t.release_all(),std::runtime_error);
 EXPECT_EQ(t.receipts.size(),receipts);
 EXPECT_EQ(t.release_calls.size(),releases);}
void produce(pas::SessionArchive& a,std::size_t count){for(std::size_t i=0;i<count;++i){a.event(1,{{"event","boundary"},{"index",i}});if(i%256==255){for(int k=0;k<10000;++k){if(a.meter_stats().written>=i+1||a.faulted())break;std::this_thread::sleep_for(std::chrono::milliseconds(1));}if(a.faulted())break;}}}
TEST(R3,ArchiveExactly65536SamplesFullRows){auto p=root("exact");pas::HostClock c;pas::SessionArchive a(p,{},16*1024*1024,1024*1024,{&c,0});produce(a,events);a.complete(1,{{"status","offline_complete"}});a.close();auto s=a.meter_stats();
 EXPECT_FALSE(a.faulted())<<a.error();
 EXPECT_EQ(s.written,events);
 EXPECT_EQ(s.serialize_ms.size(),events);
 EXPECT_EQ(s.write_ms.size(),events);
 EXPECT_TRUE(verify_archive(p,{{"boundary",events}}));}
TEST(R3,ArchiveAbove65536FailsWithoutSampling){auto p=root("over");pas::HostClock c;pas::SessionArchive a(p,{},16*1024*1024,1024*1024,{&c,0});produce(a,events+1);a.close();auto s=a.meter_stats();
 EXPECT_TRUE(a.faulted());
 EXPECT_EQ(a.error(),"archive meter sample limit");
 EXPECT_EQ(s.serialize_ms.size(),events);
 EXPECT_EQ(s.write_ms.size(),events);
 EXPECT_EQ(s.written,events+1);
 EXPECT_FALSE(verify_archive(p,{{"boundary",events+1}}));}
TEST(R3,ArchiveFullContentDeterminismAndMissingRow){auto p=root("determinism");std::vector<std::string> content;for(int run=0;run<2;++run){auto q=p/std::to_string(run);pas::FakeClock c;pas::SessionArchive a(q,{},1024,128,{&c,0});a.event(1,{{"event","payload"},{"nested",{{"all",{1,2,3}},{"string","\n"}}}});a.complete(1,{{"status","offline_complete"}});a.close();
 EXPECT_FALSE(a.faulted());
 EXPECT_TRUE(verify_archive(q,{{"payload",1}}));std::ifstream f(q/"round-1/events-0.jsonl",std::ios::binary);content.emplace_back(std::istreambuf_iterator<char>(f),std::istreambuf_iterator<char>());}
 EXPECT_EQ(content[0],content[1]);std::ofstream(p/"0/round-1/events-0.jsonl",std::ios::trunc).close();
 EXPECT_FALSE(verify_archive(p/"0",{{"payload",1}}));}
