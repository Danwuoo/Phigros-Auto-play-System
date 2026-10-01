#include "pas/vision_cpu.hpp"
#include "pas/adb.hpp"
#include "pas/analysis.hpp"
#include "pas/game_dataset.hpp"
#include <gtest/gtest.h>
#include <fstream>

using namespace pas;
namespace {
namespace fs=std::filesystem;using json=nlohmann::json;
fs::path fresh(const std::string& name) {
    HostClock clock;return fs::path(PAS_TEST_SOURCE_DIR)/"out"/("vision-test-"+name+"-"+std::to_string(clock.now_ns()));
}
void save(const fs::path& p,const json& j) {std::ofstream out(p);out<<j.dump(2);}
void replace_test_masks(const fs::path& folder,const json& a) {
    fs::remove(folder/"semantic.png");fs::remove(folder/"instance.png");fs::remove(folder/"overlay.png");rasterize_annotation(folder,a);
}
fs::path packet(const std::string& status,const std::string& reviewer) {
    const auto root=fresh("packet");fs::create_directories(root/"sample");Frame f;f.width=f.height=64;f.stride=192;f.rgb.resize(64*64*3,32);
    write_diagnostic_png(root/"source.png",f);write_diagnostic_png(root/"sample/image.png",f);const auto hash=sha256_file(root/"source.png");
    const json roi={{"x",0},{"y",0},{"width",64},{"height",64},{"scale",1}};
    const json annotation={{"schema",1},{"image_sha256",hash},{"review_status",status},{"reviewer",reviewer},
        {"source_png",(root/"source.png").generic_string()},{"source_sha256",hash},{"roi",roi},
        {"objects",json::array()},{"polygons",json::array({{{"semantic",0},{"instance",0},{"points",{{0,0},{64,0},{64,64},{0,64}}}}})}};
    save(root/"sample/annotation.json",annotation);rasterize_annotation(root/"sample",annotation);
    save(root/"manifest.json",{{"schema",1},{"samples",json::array({{{"folder","sample"},{"image_sha256",hash},
        {"source_png",(root/"source.png").generic_string()},{"source_sha256",hash},{"roi",roi},{"song_family","test-family"},{"split","development"}}})}});
    return root;
}
}
TEST(CpuVision, OptimizerSerializationIgnoreAndInputBoundAreVerifiedOnCpu) {
    const auto r=cpu_vision_self_test(fresh("model"));EXPECT_TRUE(r.at("device_cpu").get<bool>());
    EXPECT_TRUE(r.at("optimizer_changed_weights").get<bool>());EXPECT_TRUE(r.at("finite_gradient_and_loss").get<bool>());
    EXPECT_LT(r.at("checkpoint_max_abs_difference").get<double>(),1e-6);
    EXPECT_LT(r.at("ignore_pixel_loss_difference").get<double>(),1e-6);
    EXPECT_TRUE(r.at("oversized_input_rejected").get<bool>());EXPECT_LT(r.at("parameters").get<int>(),10000);
}
TEST(CpuVision, ProposedMasksCannotBeTrainedAsHumanGold) {
    const auto root=packet("proposed","Codex proposal");const auto qa=audit_cpu_vision_packet(root);
    ASSERT_TRUE(qa.at("valid").get<bool>());EXPECT_FALSE(qa.at("development_training_ready").get<bool>());
    const auto output=fresh("must-not-exist");EXPECT_THROW(train_reviewed_cpu_vision(root,output,1),std::invalid_argument);
    EXPECT_FALSE(fs::exists(output));
}
TEST(CpuVision, ReviewerAndSourceHashAreRequired) {
    const auto root=packet("reviewed_human","");EXPECT_FALSE(audit_cpu_vision_packet(root).at("development_training_ready").get<bool>());
    std::ofstream out(root/"source.png",std::ios::app);out<<'x';out.close();EXPECT_FALSE(audit_cpu_vision_packet(root).at("valid").get<bool>());
}
TEST(CpuVision, DevelopmentReviewNeverBecomesCrossSongValidation) {
    const auto root=packet("reviewed_human","test human");const auto qa=audit_cpu_vision_packet(root);
    EXPECT_TRUE(qa.at("development_training_ready").get<bool>());EXPECT_FALSE(qa.at("cross_song_validation_ready").get<bool>());
}
TEST(CpuVision, CurrentPilotRejectsRandomFrameValidationAndEscapingPaths) {
    const auto root=packet("reviewed_human","test human");std::ifstream in(root/"manifest.json");auto manifest=json::parse(in);in.close();
    manifest["samples"][0]["split"]="validation";save(root/"manifest.json",manifest);EXPECT_FALSE(audit_cpu_vision_packet(root).at("valid").get<bool>());
    manifest["samples"][0]["split"]="development";manifest["samples"][0]["folder"]="../outside";save(root/"manifest.json",manifest);
    EXPECT_FALSE(audit_cpu_vision_packet(root).at("valid").get<bool>());
}
TEST(CpuVision, WrongNativeCropAndSavedMaskAreRejected) {
    const auto root=packet("reviewed_human","test human");
    auto mask=read_label_png(root/"sample/semantic.png",8);mask.values[0]=255;fs::remove(root/"sample/semantic.png");write_label_png(root/"sample/semantic.png",mask);
    EXPECT_FALSE(audit_cpu_vision_packet(root).at("valid").get<bool>());
    std::ifstream in(root/"sample/annotation.json");auto a=json::parse(in);in.close();replace_test_masks(root/"sample",a);
    auto image=load_diagnostic_png(root/"sample/image.png");image.rgb[0]=255;fs::remove(root/"sample/image.png");write_diagnostic_png(root/"sample/image.png",image);
    const auto hash=sha256_file(root/"sample/image.png");a["image_sha256"]=hash;save(root/"sample/annotation.json",a);
    std::ifstream mf(root/"manifest.json");auto manifest=json::parse(mf);mf.close();manifest["samples"][0]["image_sha256"]=hash;save(root/"manifest.json",manifest);
    EXPECT_FALSE(audit_cpu_vision_packet(root).at("valid").get<bool>());
}
TEST(CpuVision, NotePartCannotHaveUnspecifiedInstanceObject) {
    const auto root=packet("reviewed_human","test human");std::ifstream in(root/"sample/annotation.json");auto a=json::parse(in);in.close();
    a["polygons"][0]["semantic"]=4;a["polygons"][0]["instance"]=1;save(root/"sample/annotation.json",a);replace_test_masks(root/"sample",a);
    EXPECT_FALSE(audit_cpu_vision_packet(root).at("valid").get<bool>());
}
TEST(CpuVision, RebuildingEditedPolygonsWritesNewPacketAndLeavesSourceMask) {
    const auto root=packet("proposed","");std::ifstream in(root/"sample/annotation.json");auto a=json::parse(in);in.close();
    a["polygons"][0]["semantic"]=255;a["polygons"][0]["instance"]=65535;save(root/"sample/annotation.json",a);
    const auto before=sha256_file(root/"sample/semantic.png");const auto output=fresh("rasterized");
    const auto qa=rasterize_cpu_vision_packet(root,output);EXPECT_TRUE(qa.at("valid").get<bool>());EXPECT_FALSE(qa.at("development_training_ready").get<bool>());
    EXPECT_EQ(before,sha256_file(root/"sample/semantic.png"));EXPECT_NE(before,sha256_file(output/"sample/semantic.png"));
}
