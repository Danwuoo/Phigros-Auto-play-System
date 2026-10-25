#include "../apps/chart_experiment/hud_timeline.hpp"
#include "../apps/chart_experiment/image_retention.hpp"
#include <array>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <new>

namespace {bool track_allocations=false;std::size_t allocations=0;}
void* operator new(std::size_t size) {
    if(track_allocations)++allocations;
    if(void* pointer=std::malloc(size?size:1))return pointer;
    throw std::bad_alloc();
}
void* operator new[](std::size_t size) {return ::operator new(size);}
void operator delete(void* pointer) noexcept {std::free(pointer);}
void operator delete[](void* pointer) noexcept {std::free(pointer);}
void operator delete(void* pointer,std::size_t) noexcept {std::free(pointer);}
void operator delete[](void* pointer,std::size_t) noexcept {std::free(pointer);}

namespace {
using namespace rhythm;using namespace rhythm::experimental;
unsigned checks=0,groups=0;
void check(bool value,const char* message){++checks;if(!value)throw std::runtime_error(message);}
HudTimelineMeta meta(std::uint64_t sequence,std::int64_t received) {
    HudTimelineMeta m;m.sequence=sequence;m.epoch=7;m.generation=3;m.geometry_version=2;
    m.received_ns=received;m.processed_ns=received+123;m.source_timestamp_us=456;m.source_valid=true;return m;
}
}
int main() {
    try {
        std::vector<std::uint8_t> rgb(1280u*720u*3u,0);
        const RgbImageView image{1280,720,3840,rgb};
        const auto pixel=[&](int x,int y,std::array<std::uint8_t,3> color){const auto offset=(static_cast<std::size_t>(y)*1280+static_cast<std::size_t>(x))*3;std::copy(color.begin(),color.end(),rgb.begin()+static_cast<std::ptrdiff_t>(offset));};
        pixel(1076,20,{255,0,0});pixel(1255,59,{0,0,255});pixel(560,8,{0,255,0});pixel(719,79,{73,73,73});
        ++groups;{
            HudTimeline timeline(4);const auto m=meta(1,1'000'000'000);
            check(timeline.observe(image,m,true)==HudTimelineOutcome::sampled,"valid unknown frame not saved");
            check(timeline.samples().size()==1&&timeline.bytes().size()==18720,"ROI byte layout wrong");
            check(timeline.bytes()[0]==77&&timeline.bytes()[7199]==29&&timeline.bytes()[7200]==149&&timeline.bytes().back()==73,"RGB grayscale/crop boundary wrong");
            const auto& f=timeline.samples()[0].frame;
            check(f.raw_class==ObservedGameState::unknown&&!f.effective_HUD&&f.source_valid,"unknown source silently promoted to HUD");
            check(f.sequence==1&&f.epoch==7&&f.generation==3&&f.geometry_version==2&&f.received_ns==m.received_ns&&f.processed_ns==m.processed_ns&&f.source_timestamp_us==456,"actual metadata changed");
            auto early=meta(2,m.received_ns+49'999'999);early.processed_ns+=2'000'000;
            check(timeline.observe(image,early,true)==HudTimelineOutcome::interval_not_elapsed,"poll/processed time substituted for received cadence");
            auto boundary=meta(3,m.received_ns+50'000'000);boundary.raw_class=ObservedGameState::paused;boundary.effective_HUD=false;
            check(timeline.observe(image,boundary,true)==HudTimelineOutcome::sampled,"exact 50ms boundary rejected");
            check(timeline.samples()[1].byte_offset==18720&&timeline.samples()[1].frame.raw_class==ObservedGameState::paused,"recording overrode original class or offset");
            check(timeline.stats().max_sample_gap_ns==50'000'000,"actual sample gap missing");
        }
        ++groups;{
            HudTimeline timeline(2);const auto first=meta(10,100'000'000);
            check(timeline.observe(image,first,false)==HudTimelineOutcome::not_armed,"unarmed sample saved");
            auto invalid=first;invalid.source_valid=false;
            check(timeline.observe(image,invalid,true)==HudTimelineOutcome::invalid_source,"invalid source saved");
            check(timeline.observe(image,first,true)==HudTimelineOutcome::sampled,"rejected source mutated progression");
            check(timeline.observe(image,first,true)==HudTimelineOutcome::non_increasing_sequence,"duplicate frame saved");
            check(timeline.observe(image,meta(11,99'000'000),true)==HudTimelineOutcome::non_increasing_received,"receipt regression accepted");
            check(timeline.observe(image,meta(11,100'000'000),true)==HudTimelineOutcome::non_increasing_received,"equal receipt accepted");
            check(timeline.observe(image,meta(11,150'000'000),true)==HudTimelineOutcome::sampled,"invalid receipt poisoned next valid sequence");
            check(timeline.observe(image,meta(12,200'000'000),true)==HudTimelineOutcome::capacity_exhausted,"full buffer grew");
            check(timeline.observe(image,meta(13,210'000'000),true)==HudTimelineOutcome::interval_not_elapsed,"full buffer loss counted every frame instead of sample cadence");
            check(timeline.observe(image,meta(14,250'000'000),true)==HudTimelineOutcome::capacity_exhausted,"second budget loss missing");
            check(timeline.samples().size()==2&&timeline.bytes().size()==37440&&timeline.samples()[1].frame.sequence==11,"full buffer overwrote retained evidence");
            check(timeline.stats().capacity_drops==2&&timeline.stats().first_capacity_drop_received_ns==200'000'000&&timeline.stats().last_capacity_drop_received_ns==250'000'000,"budget loss not reported");
            check(timeline.stats().sequence_rejects==1&&timeline.stats().timestamp_rejects==2,"sequence/timestamp losses missing");
        }
        ++groups;{
            HudTimeline timeline(2);const auto valid=meta(1,100);
            for(const int stride:{0,3839,4097,std::numeric_limits<int>::max()}) {
                auto bad=image;bad.stride=stride;check(timeline.observe(bad,valid,true)==HudTimelineOutcome::invalid_geometry,"invalid stride read");
            }
            auto truncated=image;truncated.pixels=truncated.pixels.first(truncated.pixels.size()-1);
            check(timeline.observe(truncated,valid,true)==HudTimelineOutcome::invalid_geometry,"truncated input read");
            auto bad=image;bad.width=1279;check(timeline.observe(bad,valid,true)==HudTimelineOutcome::invalid_geometry,"wrong geometry read");
            auto f=valid;f.processed_ns=99;check(timeline.observe(image,f,true)==HudTimelineOutcome::invalid_metadata,"future receipt accepted");
            f=valid;f.epoch=0;check(timeline.observe(image,f,true)==HudTimelineOutcome::invalid_metadata,"missing frame identity accepted");
            f=valid;f.source_timestamp_us=-1;check(timeline.observe(image,f,true)==HudTimelineOutcome::invalid_metadata,"invalid source timestamp accepted");
            check(timeline.samples().empty()&&timeline.bytes().empty(),"invalid input created partial sample");
            check(timeline.observe(image,valid,true)==HudTimelineOutcome::sampled,"invalid image mutated progression");
        }
        ++groups;{
            HudTimeline timeline;
            check(timeline.capacity()==3601&&timeline.allocated_pixel_bytes()==67'410'720,"default pixel budget changed");
            allocations=0;track_allocations=true;
            const auto a=timeline.observe(image,meta(1,0),true);
            const auto b=timeline.observe(image,meta(2,50'000'000),true);
            const auto c=timeline.observe(image,meta(2,50'000'000),true);
            const auto bytes=timeline.bytes().size();const auto entries=timeline.samples().size();
            track_allocations=false;
            check(allocations==0&&a==HudTimelineOutcome::sampled&&b==HudTimelineOutcome::sampled&&c==HudTimelineOutcome::non_increasing_sequence,"observe/access allocated heap or changed outcome");
            check(bytes==37440&&entries==2,"preallocation exposed unused pixels/index slots");
            for(const auto capacity:{std::size_t{0},std::size_t{3602},std::numeric_limits<std::size_t>::max()}) {
                bool rejected=false;try {HudTimeline invalid(capacity);}catch(const std::invalid_argument&) {rejected=true;}
                check(rejected,"invalid capacity allowed large/overflow allocation");
            }
        }
        ++groups;{
            HudTimeline metadata(3,false);
            check(!metadata.pixels_retained()&&metadata.allocated_pixel_bytes()==0,"metadata-only mode allocated image storage");
            allocations=0;track_allocations=true;
            const auto a=metadata.observe(image,meta(1,0),true);
            const auto b=metadata.observe(image,meta(2,50'000'000),true);
            track_allocations=false;
            check(a==HudTimelineOutcome::sampled&&b==HudTimelineOutcome::sampled&&allocations==0,"metadata-only sampling allocated or failed");
            check(metadata.bytes().empty()&&metadata.samples().size()==2&&metadata.samples()[1].frame.sequence==2,
                "metadata-only mode retained pixels or lost actual receipts");
            check(parse_image_retention("none")==ImageRetention::none&&!retain_result_image(ImageRetention::none)&&
                !retain_diagnostic_images(ImageRetention::none),"none policy permits images");
            check(retain_result_image(parse_image_retention("result"))&&!retain_diagnostic_images(ImageRetention::result),"result-only policy permits diagnostics");
            check(retain_diagnostic_images(parse_image_retention("diagnostic")),"explicit diagnostics unavailable");
            bool rejected=false;try{(void)parse_image_retention("all");}catch(const std::invalid_argument&){rejected=true;}
            check(rejected,"unknown image policy accepted");
            const char* ordinary[]={"program","run-adaptive"};int ordinary_count=2;
            check(image_retention_suffix(ordinary_count,ordinary)==ImageRetention::none&&ordinary_count==2,"ordinary run default saves images or consumes arguments");
            const char* explicit_result[]={"program","run-adaptive","--image-retention","result"};int result_count=4;
            check(image_retention_suffix(result_count,explicit_result)==ImageRetention::result&&result_count==2,"explicit suffix did not preserve base argv");
            const char* missing[]={"program","--image-retention"};int missing_count=2;
            rejected=false;try{(void)image_retention_suffix(missing_count,missing);}catch(const std::invalid_argument&){rejected=true;}
            check(rejected,"missing retention argument accepted");
        }
        std::cout<<groups<<" groups / "<<checks<<" HUD timeline checks passed; diagnostic memory only.\n";return 0;
    }catch(const std::exception& e){track_allocations=false;std::cerr<<"FAILED: "<<e.what()<<'\n';return 1;}
}
