#include "pas/diagnostic_snapshot.hpp"
#include "pas/journal.hpp"
#include "pas/session_archive.hpp"
#include "pas/lossless_event_stream.hpp"
#include "pas/analysis.hpp"
#include <gtest/gtest.h>
#include <fstream>
#include <sstream>
#include <type_traits>
namespace {
using J=nlohmann::json;
namespace fs=std::filesystem;
fs::path fixtures;

fs::path fresh(const char*name){const auto p=fixtures/name;
if(fs::exists(p))throw std::runtime_error("fresh fixture");
fs::create_directory(p);
return p;
}
J read(const fs::path&p){std::ifstream f(p);
return J::parse(f);
}
pas::DecisionSnapshot scene(){pas::DecisionSnapshot s;
s.context={1,2,3,4,500,1280,720,1};
s.sequence=7;
s.ui=pas::GameUi::playing;
s.playing_gate=true;
s.ui_basis="escaped \" \\ unicode \xE6\x9C\xAC";
pas::GameTarget t;
t.note_id=9;
t.note.kind=pas::NoteKind::hold;
t.note.center={1.25,-.5};
t.reason="\n\t\\\"";
t.crossing_ns=999;
t.line_id=14;
t.line_projection_only=true;
t.hit_velocity={1e-30,1e30};
s.targets.push_back(t);
return s;
}
TEST(DiagnosticSnapshot, FullValuesAreOwnedAndEncodeExactly){
 auto s=scene();
pas::CandidateBatch b;
b.context=s.context;
b.history_source="current only";
pas::TrackingCandidate c;
c.origin="\"\\\n";
c.note=s.targets[0].note;
c.shortened_hold=c.note;
b.candidates.push_back(c);

 const J expected_current={{"event","current_observer_scene"},{"scene",pas::decision_json(s)}},expected_filtered={{"event","filtered_owner_scene"},{"allow",false},{"scene",pas::decision_json(s)}},expected_batch={{"event","complete_raw_candidate_batch"},{"batch",pas::candidate_batch_json(b)}};

 auto current=pas::DiagnosticSnapshot::current(s),filtered=pas::DiagnosticSnapshot::filtered(s,false),decision=pas::DiagnosticSnapshot::decision(s),batch=pas::DiagnosticSnapshot::batch(b);
const auto expected_decision=pas::decision_json(s);

 s.targets.clear();
s.context.frame=100;
s.ui_basis="mutated";
b.candidates.clear();
b.history_source="mutated";

 EXPECT_EQ(current.encode().dump(),expected_current.dump());
EXPECT_EQ(filtered.encode().dump(),expected_filtered.dump());
EXPECT_EQ(decision.encode().dump(),expected_decision.dump());
EXPECT_EQ(batch.encode().dump(),expected_batch.dump());

 static_assert(!std::is_constructible_v<pas::DiagnosticSnapshot,pas::Frame>);
static_assert(!std::is_copy_constructible_v<pas::DiagnosticSnapshot>);

}
TEST(DiagnosticSnapshot, OversizedCurrentValuesAreRejectedBeforeCopy){auto s=scene();
s.targets.resize(129);
EXPECT_THROW(pas::DiagnosticSnapshot::current(s),std::invalid_argument);
s.targets.resize(1);
s.targets[0].reason.assign(513,'x');
EXPECT_THROW(pas::DiagnosticSnapshot::current(s),std::invalid_argument);
pas::CandidateBatch b;
b.lines.resize(17);
EXPECT_THROW(pas::DiagnosticSnapshot::batch(b),std::invalid_argument);
b.lines.clear();
b.candidates.resize(129);
EXPECT_THROW(pas::DiagnosticSnapshot::batch(b),std::invalid_argument);
}
TEST(DiagnosticJournal, MixedRowsPreserveOrderMetadataAndFullValues){const auto root=fresh("journal");
pas::Journal journal(root/"events.jsonl");
auto s=scene();
const auto expected=pas::decision_json(s);
ASSERT_TRUE(journal.push({{"event","first"}}));
ASSERT_TRUE(journal.push(pas::DiagnosticSnapshot::decision(s)));
s.targets.clear();
ASSERT_TRUE(journal.push(pas::Journal::prepare({{"event","last"}})));
journal.close();
ASSERT_FALSE(journal.faulted());
std::ifstream f(root/"events.jsonl");
std::string row;
std::vector<J> rows;
while(std::getline(f,row))rows.push_back(J::parse(row));
ASSERT_EQ(rows.size(),3);
EXPECT_EQ(rows[0]["event"],"first");
auto canonical=expected;
canonical["schema_version"]=2;
canonical["clock_domain"]="host_qpc_ns";
EXPECT_EQ(rows[1],canonical);
EXPECT_EQ(rows[2]["event"],"last");
EXPECT_EQ(journal.admitted_records(),3);
}
TEST(DiagnosticJournal, CompressedStressRowsDecodeExactly){const auto root=fresh("stress-journal");
const auto path=root/"events.jsonl.gz";
pas::Journal journal(path,8192,{},pas::Journal::Encoding::gzip);
auto s=scene();
auto expected=pas::decision_json(s);
expected["schema_version"]=2;
expected["clock_domain"]="host_qpc_ns";
ASSERT_TRUE(journal.push(pas::DiagnosticSnapshot::decision(s)));
ASSERT_TRUE(journal.push(pas::Journal::prepare({{"event","end"}})));
journal.close();
ASSERT_FALSE(journal.faulted());
pas::LosslessEventReader reader(path,1024*1024);
std::string row;
ASSERT_TRUE(reader.next(row));
EXPECT_EQ(J::parse(row),expected);
ASSERT_TRUE(reader.next(row));
EXPECT_EQ(J::parse(row).at("event"),"end");
EXPECT_FALSE(reader.next(row));
EXPECT_EQ(journal.admitted_records(),2);
}
TEST(DiagnosticJournal, ByteBoundDropsDebugAndFaultsCriticalWithoutDiscardingAdmittedRows){const auto root=fresh("pressure");
std::mutex mutex;
std::condition_variable cv;
bool entered=false,release=false;
pas::Journal journal(root/"events.jsonl",8192,[&]{std::unique_lock lock(mutex);
entered=true;
cv.notify_all();
cv.wait(lock,[&]{return release;
});
});
struct Unblock{std::mutex&m;
std::condition_variable&cv;
bool&release;
~Unblock(){std::lock_guard lock(m);
release=true;
cv.notify_all();
}}unblock{mutex,cv,release};
ASSERT_TRUE(journal.push({{"event","seed"}}));
{std::unique_lock lock(mutex);
ASSERT_TRUE(cv.wait_for(lock,std::chrono::seconds(2),[&]{return entered;
}));
}const auto s=scene();
for(int i=0;
i<64;
++i)ASSERT_TRUE(journal.push(pas::DiagnosticSnapshot::current(s)));
EXPECT_FALSE(journal.push(pas::DiagnosticSnapshot::current(s),false));
EXPECT_EQ(journal.debug_drops(),1);
EXPECT_FALSE(journal.faulted());
EXPECT_FALSE(journal.push(pas::DiagnosticSnapshot::current(s),true));
EXPECT_TRUE(journal.faulted());
{std::lock_guard lock(mutex);
release=true;
}cv.notify_all();
journal.close();
EXPECT_EQ(journal.admitted_records(),65);
}
TEST(DiagnosticArchive, GzipRetainsMoreThan32BoundedSegmentsAndExactRows){const auto root=fresh("gzip");
pas::SessionArchive archive(root,{{"test","gzip"}},512,256,pas::SessionArchive::Encoding::gzip);
std::string expected;
for(int i=0;
i<100;
++i){J v={{"event","encoded"},{"seq",i},{"payload",std::string(200,'x')}};
auto canonical=v;
canonical["schema_version"]=2;
canonical["clock_domain"]="host_qpc_ns";
canonical["round_id"]=1;
expected+=canonical.dump()+"\r\n";
archive.event(1,std::move(v));
}archive.complete(1,{{"status","result_confirmed"}});
archive.close();
ASSERT_FALSE(archive.faulted())<<archive.error();
const auto summary=read(root/"round-1/summary.json");
ASSERT_GT(summary.at("event_segments").size(),32);
ASSERT_LE(summary.at("event_segments").size(),256);
std::string actual,row;
std::uint64_t physical=0;
for(const auto&segment:summary.at("event_segments")){const auto p=root/"round-1"/segment.at("path").get<std::string>();
EXPECT_EQ(pas::sha256_file(p),segment["sha256"]);
physical+=fs::file_size(p);
pas::LosslessEventReader reader(p,512);
while(reader.next(row))actual+=row+'\n';
}EXPECT_EQ(actual,expected);
EXPECT_EQ(summary["event_decoded_bytes"],expected.size());
EXPECT_EQ(summary["event_physical_bytes"],physical);
const auto analysis=pas::analyze_game_round(root/"round-1");
EXPECT_TRUE(analysis.at("segment_integrity_verified").get<bool>());
}
TEST(DiagnosticArchive, CompressedCrcTruncationMagicAndDecodedLimitsReject){const auto root=fresh("corruption");
const auto source=root/"source";
{pas::SessionArchive a(source,{{"test","corruption"}},512,256,pas::SessionArchive::Encoding::gzip);
a.event(1,{{"event","source"},{"payload",std::string(200,'x')}});
a.close();
ASSERT_FALSE(a.faulted());
}const auto original=source/"round-1/events-0.jsonl.gz";
ASSERT_TRUE(fs::exists(original));
for(const auto*name:{"crc","truncated"}){const auto p=root/(std::string(name)+".jsonl.gz");
fs::copy_file(original,p);
if(std::string_view(name)=="truncated")fs::resize_file(p,fs::file_size(p)-8);
else{std::fstream f(p,std::ios::binary|std::ios::in|std::ios::out);
const auto offset=static_cast<std::streamoff>(fs::file_size(p)-8);
f.seekg(offset);
char c=0;
f.get(c);
f.seekp(offset);
f.put(c^1);
}const auto scan=[&]{pas::LosslessEventReader reader(p,512);
std::string row;
while(reader.next(row)){};
};
EXPECT_THROW(scan(),std::runtime_error);
}const auto bomb=[&]{pas::LosslessEventReader reader(original,128);
std::string row;
while(reader.next(row)){};
};
EXPECT_THROW(bomb(),std::runtime_error);
const auto magic=root/"plain.jsonl.gz";
{std::ofstream f(magic);
f<<"{}\n";
}EXPECT_THROW(pas::LosslessEventReader reader(magic,512),std::runtime_error);
}
TEST(DiagnosticArchive, PlainSegmentFailureRemainsTheFirstCause){const auto root=fresh("first-cause");
pas::SessionArchive archive(root,{{"test","first-cause"}},128,128);
for(int i=0;
i<100;
++i){try{archive.event(1,{{"event","row"},{"padding",std::string(20,'x')}});
}catch(const std::exception&){break;
}}archive.close();
ASSERT_TRUE(archive.faulted());
EXPECT_EQ(archive.error(),"round journal 32-segment limit reached");
try{archive.event(1,{{"event","later"}});
FAIL();
}catch(const std::exception&e){EXPECT_EQ(e.what(),archive.error());
}EXPECT_EQ(read(root/"archive-failure.json").at("first_error"),archive.error());
}
TEST(DiagnosticArchive, OwnedSnapshotUsesTheOriginalRoundAndSchema){const auto root=fresh("archive-snapshot");
auto s=scene();
auto expected=pas::decision_json(s);
expected["schema_version"]=2;
expected["clock_domain"]="host_qpc_ns";
expected["round_id"]=1;
pas::SessionArchive archive(root,{{"test","owned"}},1024*1024,256,pas::SessionArchive::Encoding::gzip);
archive.event(1,pas::DiagnosticSnapshot::decision(s));
s.targets.clear();
archive.close();
ASSERT_FALSE(archive.faulted());
pas::LosslessEventReader reader(root/"round-1/events-0.jsonl.gz");
std::string row;
ASSERT_TRUE(reader.next(row));
EXPECT_EQ(J::parse(row),expected);
EXPECT_FALSE(reader.next(row));
EXPECT_TRUE(pas::analyze_game_round(root/"round-1").at("segment_integrity_verified").get<bool>());
}
}
int main(int argc,char**argv){try{if(argc!=2||fs::exists(argv[1]))throw std::runtime_error("fresh test report argument");
const fs::path output=argv[1];
fixtures=output.string()+".fixtures";
if(fs::exists(fixtures))throw std::runtime_error("fresh fixture root");
fs::create_directory(fixtures);
int n=1;
char*args[]={argv[0]};
::testing::InitGoogleTest(&n,args);
const int result=RUN_ALL_TESTS();
const auto*u=::testing::UnitTest::GetInstance();
J cases=J::array();
for(int i=0;
i<u->total_test_suite_count();
++i){const auto*s=u->GetTestSuite(i);
for(int j=0;
j<s->total_test_count();
++j){const auto*t=s->GetTestInfo(j);
cases.push_back({{"suite",s->name()},{"test",t->name()},{"pass",t->result()->Passed()}});
}}std::ofstream f(output);
f<<J{{"schema","pas.diagnostic-writer-tests.v1"},{"tests_planned",u->total_test_count()},{"tests_run",u->test_to_run_count()},{"failed",u->failed_test_count()},{"cases",cases},{"cost_qualification",false},{"live_ready",false},{"device_operations",0}}.dump(2)<<'\n';
return f?result:2;
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';
return 2;
}}
