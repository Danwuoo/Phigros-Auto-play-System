#include "pas/journal.hpp"
#include "pas/session_archive.hpp"
#include <gtest/gtest.h>
#include <fstream>
#include <sstream>
#include <type_traits>
namespace {
using J=nlohmann::json;namespace fs=std::filesystem;
fs::path fresh(const char* name){return fs::temp_directory_path()/(std::string("pas-journal-encoding-")+name+"-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));}
std::string bytes(const fs::path& path){std::ifstream f(path,std::ios::binary);return {std::istreambuf_iterator<char>(f),std::istreambuf_iterator<char>()};}
TEST(PreparedJournal, CannotConstructFromRawSerializedInput){static_assert(!std::is_constructible_v<pas::PreparedJournalRecord,std::string>);SUCCEED();}
TEST(PreparedJournal, MixedEncodingsPreserveExactBytesAndMetadata){
 const auto root=fresh("bytes");fs::create_directory(root);const auto a=root/"legacy.jsonl",b=root/"mixed.jsonl";
 std::vector<J> records={{{"event","plain"},{"n",0}},{{"event","escaped"},{"value","\"\\\n\r\t"}},{{"event","unicode"},{"value","\xE6\x9C\xAC\xE8\xBC\xAA"}},{{"event","numbers"},{"values",{-1,1.25,1e-30,1e30,std::uint64_t(-1)}}},{{"event","nested"},{"value",{{"array",{true,false,nullptr,J::object(),J::array()}}}}},{{"event","reserved"},{"schema_version",99},{"clock_domain","wrong"}}};
 std::string expected;{pas::Journal legacy(a),mixed(b);for(std::size_t i=0;i<records.size();++i){auto canonical=records[i];canonical["schema_version"]=2;canonical["clock_domain"]="host_qpc_ns";const auto row=canonical.dump();expected+=row+'\n';EXPECT_TRUE(legacy.push(records[i]));if(i%2)EXPECT_TRUE(mixed.push(records[i]));else {auto prepared=pas::Journal::prepare(records[i]);EXPECT_EQ(prepared.bytes(),row.size());EXPECT_TRUE(mixed.push(std::move(prepared)));}}legacy.close();mixed.close();EXPECT_FALSE(legacy.faulted());EXPECT_FALSE(mixed.faulted());}
 EXPECT_EQ(bytes(a),expected);EXPECT_EQ(bytes(b),expected);fs::remove_all(root);
}
TEST(PreparedJournal, CapacityDropFaultAndDrainAreUnchanged){
 for(bool critical:{false,true}){const auto root=fresh(critical?"critical":"debug");fs::create_directory(root);std::mutex mutex;std::condition_variable cv;bool entered=false,release=false;const auto path=root/"events.jsonl";
  pas::Journal journal(path,2,[&]{std::unique_lock lock(mutex);entered=true;cv.notify_all();cv.wait(lock,[&]{return release;});});
  struct Unblock {std::mutex& m;std::condition_variable& c;bool& release;~Unblock(){std::lock_guard lock(m);release=true;c.notify_all();}} unblock{mutex,cv,release};
  ASSERT_TRUE(journal.push(pas::Journal::prepare({{"seq",1}}),false));{std::unique_lock lock(mutex);ASSERT_TRUE(cv.wait_for(lock,std::chrono::seconds(2),[&]{return entered;}));}
  EXPECT_TRUE(journal.push({{"seq",2}},false));EXPECT_TRUE(journal.push(pas::Journal::prepare({{"seq",3}}),false));EXPECT_FALSE(journal.push(pas::Journal::prepare({{"seq",4}}),critical));EXPECT_EQ(journal.faulted(),critical);EXPECT_EQ(journal.debug_drops(),critical?0u:1u);
  {std::lock_guard lock(mutex);release=true;}cv.notify_all();journal.close();EXPECT_FALSE(journal.push(pas::Journal::prepare({{"seq",5}})));
  std::istringstream in(bytes(path));std::string line;int count=0;while(std::getline(in,line)){const auto row=J::parse(line);EXPECT_EQ(row.at("seq"),++count);}EXPECT_EQ(count,3);fs::remove_all(root);
 }
}
TEST(PreparedJournal, InvalidUtf8FailsBeforeQueueAdmission){const auto root=fresh("invalid");fs::create_directory(root);pas::Journal journal(root/"events.jsonl");EXPECT_THROW(pas::Journal::prepare({{"value",std::string(1,char(0xff))}}),J::type_error);EXPECT_TRUE(journal.push(pas::Journal::prepare({{"seq",1}})));journal.close();EXPECT_FALSE(journal.faulted());EXPECT_EQ(J::parse(bytes(root/"events.jsonl")).at("seq"),1);fs::remove_all(root);}
TEST(CachedArchive, EscapedRowsAndIncompleteSummaryRetainTheirOriginalBytes){
 const auto root=fresh("archive");pas::SessionArchive archive(root,{{"source","test"}});std::string expected;
 for(int i=0;i<8;++i){J value={{"event","encoded"},{"seq",i},{"text","\"\\\n\t"},{"array",{true,false,nullptr,1.25}}};auto canonical=value;canonical["schema_version"]=2;canonical["clock_domain"]="host_qpc_ns";canonical["round_id"]=1;expected+=canonical.dump()+"\r\n";archive.event(1,std::move(value));}
 archive.close();EXPECT_FALSE(archive.faulted());EXPECT_EQ(bytes(root/"round-1/events-0.jsonl"),expected);const auto summary=J::parse(bytes(root/"round-1/summary.json"));EXPECT_EQ(summary.at("status"),"aborted");EXPECT_TRUE(summary.at("partial").get<bool>());fs::remove_all(root);
}
TEST(CachedArchive, MailboxByteLimitStillRejectsOversizedAdmission){const auto root=fresh("bound");pas::SessionArchive archive(root,{{"source","test"}});EXPECT_THROW(archive.event(1,{{"event","large"},{"payload",std::string(16*1024*1024,'x')}}),std::runtime_error);archive.close();EXPECT_TRUE(archive.faulted());EXPECT_EQ(archive.error(),"session archive mailbox overrun");EXPECT_FALSE(fs::exists(root/"round-1"));fs::remove_all(root);}
}
