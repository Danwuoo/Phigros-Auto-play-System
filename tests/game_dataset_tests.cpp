#include "pas/game_dataset.hpp"
#include "pas/adb.hpp"
#include "pas/analysis.hpp"
#include <gtest/gtest.h>
#include <chrono>
#include <fstream>

using namespace pas;
namespace {
using json=nlohmann::json;
struct TempFolder {std::filesystem::path path=std::filesystem::temp_directory_path()/("pas-dataset-test-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
 TempFolder(){std::filesystem::create_directory(path);}~TempFolder(){std::error_code ignored;std::filesystem::remove_all(path,ignored);}};
void save(const std::filesystem::path& path,const json& j){std::ofstream out(path);out<<j.dump(2);}
Frame frame(std::uint64_t seq,Nanoseconds t){Frame f;f.width=1280;f.height=720;f.stride=3840;f.source_rotation=1;f.sequence=seq;f.capture_complete_ns=t;f.epoch=f.generation=f.geometry_version=1;f.rgb.resize(1280*720*3,static_cast<unsigned char>(seq));return f;}
DecisionSnapshot scene(const Frame& f){DecisionSnapshot s;s.sequence=f.sequence;s.context={f.epoch,f.generation,f.geometry_version,f.sequence,f.capture_complete_ns,f.width,f.height,1};s.ui=GameUi::playing;s.playing_gate=true;return s;}
struct StepClock : Clock {mutable Nanoseconds t=0;Nanoseconds now_ns()const override{return t+=1'000'000'000;}};
json annotation(const Frame& f,const std::filesystem::path& root){return {{"schema",1},{"run_id","run-1"},{"clip_id","clip-1"},
 {"context",{{"epoch",1},{"generation",1},{"geometry",1},{"frame",1},{"capture_ns",0},{"width",f.width},{"height",f.height},{"rotation",1}}},
 {"roi",{{"x",0},{"y",0},{"width",f.width},{"height",f.height},{"scale",1}}},{"image_sha256",sha256_file(root/"sample/image.png")},
 {"review_status","proposed"},{"author","test"},{"reviewer",""},{"objects",json::array({{{"instance",1},{"type","hold"},{"amodal",false},{"visibility","visible"},{"truncated",false},{"type_uncertainty","none"}}})},
 {"polygons",json::array({{{"semantic",3},{"instance",1},{"points",{{2,2},{8,2},{8,4},{2,4}}}},{{"semantic",4},{"instance",1},{"points",{{2,4},{8,4},{8,12},{2,12}}}},{{"semantic",5},{"instance",1},{"points",{{2,12},{8,12},{8,14},{2,14}}}}})}};}
}
TEST(GameDataset, NativeEightAndSixteenBitMasksRoundTripWithoutLoss){
 TempFolder temp;for(int bits:{8,16}){LabelMask m{4,2,bits,{0,1,2,3,4,5,255,static_cast<std::uint16_t>(bits==16?65535:128)}};
  const auto path=temp.path/(std::to_string(bits)+".png");write_label_png(path,m);const auto read=read_label_png(path,bits);EXPECT_EQ(read.values,m.values);EXPECT_EQ(read.bits,bits);EXPECT_THROW(read_label_png(path,bits==8?16:8),std::invalid_argument);}
}
TEST(GameDataset, MissingPreFramesUseExactlyCurrentAndTwoPostFrames){
 GamePixelSampler sampler(1280,720,{true,false});for(int i=0;i<10;++i){auto f=frame(i+1,i*16'000'000);sampler.consume(f,scene(f),{},i==0);}EXPECT_EQ(sampler.images_retained(),3);
 TempFolder temp;FakeClock clock;const auto report=sampler.flush(temp.path/"run",json::object(),clock);EXPECT_EQ(report.at("samples").size(),3);EXPECT_FALSE(report.at("partial").get<bool>());
 const auto qa=validate_vision_dataset(temp.path/"run");EXPECT_TRUE(qa.at("valid").get<bool>());EXPECT_EQ(qa.at("annotated"),0);EXPECT_FALSE(qa.at("training_ready").get<bool>());
 EXPECT_THROW(sampler.flush(temp.path/"another",json::object(),clock),std::logic_error);
}
TEST(GameDataset, ClipHasFixedCropAndContextChangeCutsOffRealFrames){
 GamePixelSampler sampler(1280,720,{true,false});for(int i=0;i<3;++i){auto f=frame(i+1,i*16'000'000);sampler.consume(f,scene(f),{},i==2);}
 auto f=frame(4,48'000'000);f.geometry_version=2;sampler.consume(f,scene(f),{},false);EXPECT_EQ(sampler.images_retained(),2);
 TempFolder temp;FakeClock clock;sampler.flush(temp.path/"run",json::object(),clock);const auto qa=validate_vision_dataset(temp.path/"run");EXPECT_TRUE(qa.at("valid").get<bool>());
}
TEST(GameDataset, TimeDeadlinePreservesPartialManifestAndNoFakeImage){
 GamePixelSampler sampler(1280,720,{true,false});auto f=frame(1,0);sampler.consume(f,scene(f),{},true);TempFolder temp;StepClock clock;
 const auto report=sampler.flush(temp.path/"run",json::object(),clock,1);EXPECT_TRUE(report.at("partial").get<bool>());EXPECT_TRUE(report.at("samples").empty());EXPECT_EQ(report.at("stats").at("images_retained"),1);EXPECT_TRUE(std::filesystem::exists(temp.path/"run/manifest.json"));
}
TEST(GameDataset, MemoryImageQuotaAndDetectorIndependentBackgroundAreBounded){
 EXPECT_THROW(GamePixelSampler(4096,4096,{true,true}),std::invalid_argument);
 GamePixelSampler sampler(1280,720,{true,true});EXPECT_LE(sampler.allocated_raw_bytes(),120*1024*1024);
 std::uint64_t seq=0;for(int window=0;window<24;++window)for(int i=0;i<10;++i){auto f=frame(++seq,window*10'000'000'000LL+i*16'000'000);auto s=scene(f);sampler.consume(f,s,{},i==0);}
 const auto stats=sampler.stats();EXPECT_LE(sampler.images_retained(),136);EXPECT_LE(stats.at("diagnostic_clips").get<int>(),2);EXPECT_LE(stats.at("hard_clips").get<int>(),8);EXPECT_EQ(stats.at("background_clips"),4);EXPECT_EQ(stats.at("normal_clips"),0);
}
TEST(GameDataset, PolygonLabelsObjectsAndSourceHashesAreCheckedPerPixel){
 TempFolder temp;std::filesystem::create_directory(temp.path/"sample");Frame f;f.width=32;f.height=16;f.stride=96;f.rgb.resize(1536);write_diagnostic_png(temp.path/"sample/image.png",f);
 auto a=annotation(f,temp.path);save(temp.path/"sample/annotation.json",a);rasterize_annotation(temp.path/"sample",a);
 const json manifest={{"schema",1},{"dataset_id","test"},{"samples",json::array({{{"folder","sample"},{"run_id","run-1"},{"clip_id","clip-1"},{"split","development"},{"image_sha256",a.at("image_sha256")}}})}};save(temp.path/"manifest.json",manifest);
 auto qa=validate_vision_dataset(temp.path);EXPECT_TRUE(qa.at("valid").get<bool>())<<qa.dump();EXPECT_EQ(qa.at("visible_instances_per_sample_sum"),1);EXPECT_EQ(qa.at("reviewed_human"),0);EXPECT_GT(qa.at("ignore_pixels").get<int>(),0);
 a["polygons"][0]["points"][0][0]=3;save(temp.path/"sample/annotation.json",a);qa=validate_vision_dataset(temp.path);EXPECT_FALSE(qa.at("valid").get<bool>());
 a=annotation(f,temp.path);a["objects"]=json::array();save(temp.path/"sample/annotation.json",a);EXPECT_FALSE(validate_vision_dataset(temp.path).at("valid").get<bool>());
}
TEST(GameDataset, SplitLeakageAndPathTraversalAreRejected){
 TempFolder temp;std::filesystem::create_directory(temp.path/"sample");Frame f;f.width=32;f.height=16;f.stride=96;f.rgb.resize(1536);write_diagnostic_png(temp.path/"sample/image.png",f);auto a=annotation(f,temp.path);save(temp.path/"sample/annotation.json",a);rasterize_annotation(temp.path/"sample",a);
 auto entry=json{{"folder","sample"},{"run_id","run-1"},{"clip_id","clip-1"},{"split","train"},{"image_sha256",a.at("image_sha256")}};
 auto j=json{{"schema",1},{"dataset_id","test"},{"samples",json::array({entry})}};save(temp.path/"manifest.json",j);EXPECT_TRUE(validate_vision_dataset(temp.path).at("valid").get<bool>());
 std::filesystem::copy(temp.path/"sample",temp.path/"sample2",std::filesystem::copy_options::recursive);entry["folder"]="sample2";entry["split"]="validation";auto a2=a;a2["clip_id"]="clip-2";save(temp.path/"sample2/annotation.json",a2);entry["clip_id"]="clip-2";j["samples"].push_back(entry);save(temp.path/"manifest.json",j);EXPECT_FALSE(validate_vision_dataset(temp.path).at("valid").get<bool>());
 entry["folder"]="../sample";j["samples"].push_back(entry);save(temp.path/"manifest.json",j);EXPECT_FALSE(validate_vision_dataset(temp.path).at("valid").get<bool>());
}
