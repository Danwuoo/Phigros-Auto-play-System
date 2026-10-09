#include "source382_diagnostic_reference.hpp"
#include "source2082_result_ui_reference.hpp"
#include <limits>
#include "pas/diagnostic_snapshot.hpp"
#include "pas/journal.hpp"
#include "pas/session_archive.hpp"
#include "pas/lossless_event_stream.hpp"
#include "pas/analysis.hpp"
#include <gtest/gtest.h>
#include <fstream>
#include <iostream>
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
EXPECT_EQ(rows[0]["event"].get<std::string>(),"first");
auto canonical=expected;
canonical["schema_version"]=2;
canonical["clock_domain"]="host_qpc_ns";
EXPECT_EQ(rows[1],canonical);
EXPECT_EQ(rows[2]["event"].get<std::string>(),"last");
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
EXPECT_EQ(J::parse(row).at("event").get<std::string>(),"end");
EXPECT_FALSE(reader.next(row));
EXPECT_EQ(journal.admitted_records(),2);
for(const auto*name:{"exact-buffer","multiple-input-buffers"}){SCOPED_TRACE(name);
const auto p=root/(std::string(name)+".jsonl.gz");
std::string expected_row(std::string_view(name)=="exact-buffer"?65535:196607,'x');
if(std::string_view(name)=="multiple-input-buffers"){std::uint32_t state=123456789;
for(auto&c:expected_row){state=state*1664525u+1013904223u;
c=static_cast<char>(33+(state>>24)%90);
}}
const std::string bytes=expected_row+'\n';
gzFile output=gzopen(p.string().c_str(),"wb1");
ASSERT_NE(output,nullptr);
const int written=gzwrite(output,bytes.data(),static_cast<unsigned>(bytes.size()));
const int closed=gzclose(output);
ASSERT_EQ(written,static_cast<int>(bytes.size()));
ASSERT_EQ(closed,Z_OK);
pas::LosslessEventReader bounded(p,bytes.size());
ASSERT_TRUE(bounded.next(row));
EXPECT_EQ(row,expected_row);
EXPECT_FALSE(bounded.next(row));
EXPECT_EQ(bounded.decoded_bytes(),bytes.size());
}
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
EXPECT_EQ(pas::sha256_file(p),segment["sha256"].get<std::string>());
physical+=fs::file_size(p);
pas::LosslessEventReader reader(p,512);
while(reader.next(row))actual+=row+'\n';
}EXPECT_EQ(actual,expected);
EXPECT_EQ(summary["event_decoded_bytes"].get<std::uint64_t>(),expected.size());
EXPECT_EQ(summary["event_physical_bytes"].get<std::uint64_t>(),physical);
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
for(const auto*name:{"crc","truncated","trailing","concatenated"}){SCOPED_TRACE(name);
const auto p=root/(std::string(name)+".jsonl.gz");
fs::copy_file(original,p);
if(std::string_view(name)=="truncated")fs::resize_file(p,fs::file_size(p)-8);
else if(std::string_view(name)=="trailing"){std::ofstream f(p,std::ios::binary|std::ios::app);
f<<"unexpected trailing bytes";
}
else if(std::string_view(name)=="concatenated"){std::ifstream member(original,std::ios::binary);
std::ofstream f(p,std::ios::binary|std::ios::app);
f<<member.rdbuf();
}
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
}EXPECT_EQ(read(root/"archive-failure.json").at("first_error").get<std::string>(),archive.error());
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
TEST(DiagnosticArchive, NoPersistenceCreatesNoDirectoryOrImageAndHasNoQueue){
 const auto root=fixtures/"score-only-no-directory";
 ASSERT_FALSE(fs::exists(root));
 {
  pas::SessionArchive archive(root,{{"private","not persisted"}},16*1024*1024,1024*1024,
      pas::SessionArchive::Encoding::gzip,pas::SessionArchive::Persistence::none);
  for(int i=0;i<20000;++i){
   archive.event(1,{{"event","discarded"},{"ordinal",i}});
   archive.event(1,pas::DiagnosticSnapshot::current(scene()));
  }
  auto invalid_image=std::make_shared<pas::Frame>();
  invalid_image->width=1280;invalid_image->height=720;invalid_image->rgb={1,2,3};
  archive.complete(1,{{"status","result_confirmed"}},invalid_image);
  archive.complete(2,{{"status","aborted"}},invalid_image);
  EXPECT_EQ(archive.peak_queue(),0);
  EXPECT_FALSE(archive.faulted());
  EXPECT_TRUE(archive.error().empty());
  EXPECT_FALSE(fs::exists(root));
  archive.close();
  EXPECT_THROW(archive.event(0,{{"event","after-close"}}),std::runtime_error);
 }
 EXPECT_FALSE(fs::exists(root));
}
TEST(DiagnosticArchive, NoPersistenceDoesNotSerializeInvalidUtf8OrTouchExistingFiles){
 const auto root=fresh("score-only-existing-sentinel");
 const auto sentinel=root/"manifest.json";
 {std::ofstream f(sentinel,std::ios::binary);f<<"must stay byte-exact";}
 const auto sha=pas::sha256_file(sentinel);
 pas::SessionArchive archive(root,{{"private","discarded"}},128,128,
     pas::SessionArchive::Encoding::plain,pas::SessionArchive::Persistence::none);
 EXPECT_NO_THROW(archive.event(1,{{"invalid_utf8",std::string(1,char(0xff))}}));
 EXPECT_THROW(archive.complete(0,{{"status","zero-round"}}),std::invalid_argument);
 archive.close();
 EXPECT_EQ(pas::sha256_file(sentinel),sha);
 EXPECT_EQ(std::distance(fs::directory_iterator(root),fs::directory_iterator()),1);
 EXPECT_FALSE(archive.faulted());
}
TEST(DiagnosticArchive, NoPersistenceKeepsConstructorBounds){
 const auto root=fixtures/"score-only-invalid-bounds";
 EXPECT_THROW((pas::SessionArchive(root,J::object(),127,128,pas::SessionArchive::Encoding::gzip,
     pas::SessionArchive::Persistence::none)),std::invalid_argument);
 EXPECT_THROW((pas::SessionArchive(root,J::object(),16*1024*1024+1,128,pas::SessionArchive::Encoding::gzip,
     pas::SessionArchive::Persistence::none)),std::invalid_argument);
 EXPECT_FALSE(fs::exists(root));
}
}
TEST(DiagnosticSerialization, Source382FullByteOracle){
 J cases=J::array();pas::DecisionSnapshot dense;pas::CandidateBatch batch;
 for(int variant=0;variant<12;++variant){
  pas::DecisionSnapshot s;s.context={1,2,3,4,500,1280,720,1};s.sequence=7;
  s.ui=static_cast<pas::GameUi>(variant%6);s.playing_gate=variant%2;s.capacity_valid=variant%3;
  s.ui_basis="escaped \" \\ unicode \xE6\x9C\xAC\n\t";s.compute_timing_host_qpc=variant%2;
  if(variant%2)s.play_button=pas::Vec2{1.5,-.5};
  s.recognition_start_ns=1;s.recognition_end_ns=2;s.combo_digit_glyphs=3;
  for(int i=0;i<(variant==0?0:variant<3?1:16);++i){
   pas::LineCandidate l;l.center={i*1.5,-i*.5};l.tangent={.6,.8};l.length=900;l.thickness=2;l.confidence=.75;
   l.track_id=std::uint64_t(i+1);l.observed_ns=500;l.velocity={1e-30,1e30};l.angular_velocity=-.25;
   l.association_valid=i%2;l.motion_valid=i%3;l.motion_samples=i;l.motion_span_ns=123;l.motion_residual=.25;l.angular_residual=.5;s.lines.push_back(l);
  }
  for(int i=0;i<(variant==0?0:variant<3?1:128);++i){
   pas::GameTarget t;t.note_id=std::uint64_t(i+1);t.revision=i;t.line_id=1;t.note.kind=static_cast<pas::NoteKind>(i%5);
   t.note.center={i*1.25,-.5};t.note.tangent={.6,.8};t.note.width=72;t.note.height=24;t.note.confidence=.5;
   t.note.outline_evidence=i%2;t.note.rails_geometry=i%3;t.note.head_on_line=i%2;t.note.held_body_evidence=i%3;
   t.note.held_body_patch=i%2;t.note.direct_rails_evidence=i%3;t.note.recent_identity=std::uint64_t(i+100);
   if(i%2)t.note.tail=pas::Vec2{40,50};
   t.evidence_ns=500;t.expires_ns=900;t.uncertainty_ns=30;t.hit={1.25,-.5};t.hit_velocity={1e-30,1e30};
   if(i%2){t.crossing_ns=999;t.tail_crossing_ns=1999;}
   t.current_contact=static_cast<pas::GameTarget::CurrentContact>(i%3);
   if(i%3==0){t.tail_terminal_proof.emplace();t.tail_terminal_proof->context=s.context;t.tail_terminal_proof->note_id=t.note_id;}
   t.line_projection_only=i%2;t.last_line_observed_ns=123;t.projected_line_along_px=-2.5;t.projected_line_half_length_px=450;
   t.distance=-.5;t.velocity=1e-30;t.residual=.25;t.prediction_error_px=.5;t.fit_residual_limit_px=3;t.samples=4;t.history_span_ns=5;
   t.reason="\n\t\\\"";if(variant==10)t.distance=std::numeric_limits<double>::infinity();if(variant==11)t.velocity=std::numeric_limits<double>::quiet_NaN();
   s.targets.push_back(t);
  }
  pas::CandidateBatch b;b.context=s.context;b.lines=s.lines;b.history_source=s.ui_basis;b.ui=s.ui;
  b.playing_gate=s.playing_gate;b.capacity_valid=s.capacity_valid;b.source_valid=variant%2;b.extraction_start_ns=123;b.extraction_end_ns=456;
  for(const auto& t:s.targets){pas::TrackingCandidate c;c.candidate_id=t.note_id;c.note=t.note;
   c.quality=static_cast<pas::ObservationQuality>(c.candidate_id%3);c.head_visible=c.candidate_id%2;c.body_visible=c.candidate_id%3;
   c.left_rail=c.candidate_id%2;c.right_rail=c.candidate_id%3;c.action_support=c.candidate_id%2;c.origin=s.ui_basis;c.hint_source_frame=4;c.hint_age_ns=5;
   if(c.candidate_id%2)c.shortened_hold=c.note;b.candidates.push_back(c);}
  ASSERT_TRUE(pas::diagnostic_reference::decision_json(s).dump()==pas::decision_json(s).dump()) << "decision bytes differ from original frozen core";
  ASSERT_TRUE(pas::diagnostic_reference::candidate_batch_json(b).dump()==pas::candidate_batch_json(b).dump()) << "batch bytes differ from original frozen core";
  cases.push_back({{"id","full_byte_parity_"+std::to_string(variant)},{"pass",true}});
  if(variant==9){dense=s;batch=b;}
 }
 dense.targets[0].reason=std::string(1,char(0xff));bool old_bad=false,new_bad=false;
 try{pas::diagnostic_reference::decision_json(dense).dump();}catch(const J::type_error&){old_bad=true;}
 try{pas::decision_json(dense).dump();}catch(const J::type_error&){new_bad=true;}
 ASSERT_TRUE(old_bad&&new_bad) << "same invalid UTF8 rejection";dense.targets[0].reason="\n\t\\\"";
 cases.push_back({{"id","invalid_utf8_same_rejection"},{"pass",true}});
 EXPECT_EQ(cases.size(),13);
}

TEST(ResultUiParity, ExactOriginalLabelsTranslationThresholdNoiseAndGates) {
 pas::Frame f;f.width=1280;f.height=720;f.stride=3840;f.source_valid=true;f.rgb.resize(1280*720*3);
 pas::DecisionSnapshot s;
 const auto check=[&] {
  const auto a=pas::result_ui_reference::session_ui_pixels(f,s),b=pas::session_ui_pixels(f,s);
  EXPECT_EQ(a.hud,b.hud);EXPECT_EQ(a.menu,b.menu);EXPECT_EQ(a.result,b.result);
  EXPECT_EQ(a.result_labels,b.result_labels);EXPECT_EQ(a.result_similarity,b.result_similarity);
 };
 const auto paint=[&](int dx,int dy,int omit,std::array<std::uint8_t,3> color) {
  std::fill(f.rgb.begin(),f.rgb.end(),30);
  for(std::size_t i=0;i<pas::result_ui::labels.size();++i) {
   if(static_cast<int>(i)==omit)continue;const auto& l=pas::result_ui::labels[i];
   for(int y=0;y<l.height;++y)for(int x=0;x<l.width;++x)
    if(l.bits[std::size_t(y)*l.width+x]=='#')for(int c=0;c<3;++c)
     f.rgb[std::size_t(l.y+y+dy)*f.stride+(l.x+x+dx)*3+c]=color[c];
  }
 };
 // All 49 translations include word-boundary shifts of the 88/71-pixel labels.
 for(int dy=-3;dy<=3;++dy)for(int dx=-3;dx<=3;++dx){paint(dx,dy,-1,{255,255,255});check();EXPECT_TRUE(pas::session_ui_pixels(f,s).result);}
 for(int omit=0;omit<6;++omit){paint(0,0,omit,{255,255,255});check();EXPECT_FALSE(pas::session_ui_pixels(f,s).result);}
 for(const auto color:std::array<std::array<std::uint8_t,3>,6>{{{90,90,90},{91,91,91},{255,255,255},{100,134,100},{100,135,100},{90,255,90}}}){
  paint(0,0,-1,color);check();
 }
 for(int seed=0;seed<8;++seed){
  paint(seed%7-3,(seed*3)%7-3,-1,{245,245,245});
  std::uint32_t rng=std::uint32_t(seed+1);
  for(const auto& l:pas::result_ui::labels)for(int k=0;k<64;++k){
   rng=rng*1664525u+1013904223u;const auto x=l.x-3+int(rng%std::uint32_t(l.width+6));
   rng=rng*1664525u+1013904223u;const auto y=l.y-3+int(rng%std::uint32_t(l.height+6));
   for(int c=0;c<3;++c)f.rgb[std::size_t(y)*f.stride+x*3+c]=k%2?30:255;
  }
  check();
 }
 for(const auto value:{0,30,255}){std::fill(f.rgb.begin(),f.rgb.end(),std::uint8_t(value));check();}
 paint(0,0,-1,{255,255,255});s.ui_basis="pause_bars=2 score_glyphs=4";check();EXPECT_FALSE(pas::session_ui_pixels(f,s).result);
 s.ui_basis.clear();s.ui=pas::GameUi::menu;s.capacity_valid=true;check();EXPECT_FALSE(pas::session_ui_pixels(f,s).result);
 s=pas::DecisionSnapshot{};f.source_valid=false;check();f.source_valid=true;f.width=720;check();f.width=1280;f.rgb.pop_back();check();
}

int main(int argc,char**argv){try{if(argc!=2||fs::exists(argv[1]))throw std::runtime_error("fresh test report argument");
const fs::path output=argv[1];
fixtures=output.string()+".fixtures";
if(fs::exists(fixtures))throw std::runtime_error("fresh fixture root");
fs::create_directory(fixtures);
int n=1;
char*args[]={argv[0],nullptr};
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
