#pragma once
#include "pas/game.hpp"
#include <chrono>

namespace pas {
struct DatasetSamplingOptions {bool diagnostics=false,dataset=false;};
// Independent bounded copies; no backend, no clock scheduling, no pool lease.
// consume must run AFTER publishing the normal decision; flush after input stops.
class GamePixelSampler final {
public:
 GamePixelSampler(int width,int height,DatasetSamplingOptions options);
 void consume(const Frame&,const DecisionSnapshot&,const std::optional<HoldDisappearance>&,
              bool combo_disappeared);
 nlohmann::json flush(const std::filesystem::path&,const nlohmann::json& provenance,
                     const Clock&,Nanoseconds timeout_ns=15'000'000'000);
 std::size_t allocated_raw_bytes() const {return arena_.size();}
 std::size_t images_retained() const {return image_count_;}
 nlohmann::json stats() const;
private:
 struct Saved {SceneContext context;std::size_t offset=0;int x=0,y=0,width=0,height=0;std::string reason;};
 struct Clip {bool diagnostic=false;std::string reason;int x=0,y=0,width=0,height=0;SceneContext trigger;std::vector<Saved> images;std::size_t wanted=0;};
 int width_,height_,roi_width_,roi_height_;
 DatasetSamplingOptions options_;
 std::vector<std::uint8_t> arena_;
 std::array<SceneContext,2> history_{};
 std::size_t history_count_=0,history_cursor_=0,image_count_=0;
 std::vector<Clip> clips_;
 std::optional<std::size_t> active_;
 std::size_t diagnostic_count_=0,hard_count_=0,normal_count_=0,background_count_=0;
 std::uint64_t dropped_triggers_=0,rejected_frames_=0;
 Nanoseconds first_playing_ns_=0;
 bool started_=false,flushed_=false;
 void append(Clip&,const SceneContext&,const std::uint8_t*);
};
struct LabelMask {int width=0,height=0,bits=0;std::vector<std::uint16_t> values;};
void write_label_png(const std::filesystem::path&,const LabelMask&);
LabelMask read_label_png(const std::filesystem::path&,int bits);
std::pair<LabelMask,LabelMask> annotation_masks(int width,int height,const nlohmann::json&);
void rasterize_annotation(const std::filesystem::path& sample_folder,const nlohmann::json& annotation);
nlohmann::json validate_vision_dataset(const std::filesystem::path& root);
nlohmann::json export_vision_dataset(const std::filesystem::path& root,const std::filesystem::path& output);
nlohmann::json create_v75_dataset_pilot(const std::filesystem::path& source,const std::filesystem::path& output);
nlohmann::json benchmark_sampling_copy(int updates);
} // namespace pas
