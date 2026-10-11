// Independent cold TU: compile this source instead of src/game.cpp, then
// link only its supporting core/tracking/motion sources. Never link a second
// game.cpp object. The oracle switch/counters do not exist in normal pas_core.
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#endif
#define PAS_GAME_COMPONENT_MASK_ORACLE_CONTROLS 1
#include "../../../src/game.cpp"
#ifdef _WIN32
#include <windows.h>
#endif
#include "../tap_reset_workload.hpp"
#include "../streaming_workload.hpp"
#include <bit>
#include <iostream>

using namespace pas;
using J=nlohmann::json;
namespace {
struct Tests {
    J checks=J::array();int failed=0;
    void check(const char* id,bool pass,J detail=J::object()) {
        checks.push_back({{"id",id},{"pass",pass},{"detail",std::move(detail)}});failed+=!pass;
    }
};
struct OracleScope {
    bool previous=component_mask_oracle_controls.scalar_oracle;
    explicit OracleScope(bool scalar) {component_mask_oracle_controls.scalar_oracle=scalar;}
    ~OracleScope() {component_mask_oracle_controls.scalar_oracle=previous;}
};
// Frozen pre-repair classifier and mask loop, independent of the new helper.
int scalar_classify(const std::uint8_t* p) {
    const int r=p[0],g=p[1],b=p[2];
    if(b>165&&g>115&&b>r+35)return 1;
    if(r>180&&g>160&&b<g-55)return 2;
    if(r>190&&r>g+45&&b>75)return 3;
    if(r>195&&g>195&&b>195&&std::max({r,g,b})-std::min({r,g,b})<35)return 4;
    return 0;
}
void scalar_mask(const Frame& f,std::vector<std::uint8_t>& mask) {
    const int w=(f.width+1)/2,h=(f.height+1)/2;
    for(int y=0;y<h;++y)for(int x=0;x<w;++x)
        mask[std::size_t(y)*w+x]=static_cast<std::uint8_t>(scalar_classify(f.rgb.data()+std::size_t(y*2)*f.stride+x*6));
}
Frame image(int width,int height,std::uint8_t background=0) {
    Frame f;f.width=width;f.height=height;f.stride=width*3;f.epoch=f.generation=f.geometry_version=1;
    f.sequence=1;f.capture_complete_ns=f.pixels_ready_ns=f.published_ns=1'000'000'000;
    f.rgb.assign(std::size_t(f.stride)*height,background);return f;
}
void pixel(Frame& f,int x,int y,std::array<std::uint8_t,3> c) {
    if(x<0||x>=f.width||y<0||y>=f.height)throw std::runtime_error("bounded control pixel");
    std::copy(c.begin(),c.end(),f.rgb.data()+std::size_t(y)*f.stride+x*3);
}
void rect(Frame& f,int x,int y,int w,int h,std::array<std::uint8_t,3> c) {
    for(int py=y;py<y+h;++py)for(int px=x;px<x+w;++px)pixel(f,px,py,c);
}
struct MaskResult {bool exact=false,source_unchanged=false,guard_unchanged=false;std::uint64_t dark_blocks=0,scalar_samples=0;};
MaskResult mask_control(const Frame& f) {
    const auto before=f.rgb;
    const int w=(f.width+1)/2,h=(f.height+1)/2;const auto n=std::size_t(w)*h;
    std::vector<std::uint8_t> original(n+8,0xa7),candidate(n+8,0xa7);
    scalar_mask(f,original);component_mask_oracle_controls={};
    {OracleScope scope(false);fill_component_mask(f,candidate,w,h);}
    const auto stats=component_mask_oracle_controls;
    return {original==candidate,before==f.rgb,
        std::all_of(candidate.begin()+n,candidate.end(),[](auto b){return b==0xa7;}),stats.dark_blocks,stats.scalar_samples};
}
bool same_double(double a,double b) {return std::bit_cast<std::uint64_t>(a)==std::bit_cast<std::uint64_t>(b);}
bool same_component(const Component& a,const Component& b) {
    return a.color==b.color&&a.count==b.count&&a.x0==b.x0&&a.y0==b.y0&&a.x1==b.x1&&a.y1==b.y1&&
        same_double(a.sx,b.sx)&&same_double(a.sy,b.sy)&&same_double(a.xx,b.xx)&&same_double(a.yy,b.yy)&&same_double(a.xy,b.xy)&&
        same_double(a.ribbon_center.x,b.ribbon_center.x)&&same_double(a.ribbon_center.y,b.ribbon_center.y)&&
        same_double(a.ribbon_width,b.ribbon_width)&&same_double(a.ribbon_height,b.ribbon_height)&&a.split_pixels==b.split_pixels;
}
struct ComponentsResult {bool exact=false,capacity=false;std::vector<Component> value;};
ComponentsResult component_control(const Frame& f) {
    std::vector<std::uint8_t> a,b;std::vector<int> qa,qb;bool ca=true,cb=true;
    std::vector<Component> original,candidate;
    {OracleScope scope(true);original=components(f,ca,a,qa,true);}
    {OracleScope scope(false);candidate=components(f,cb,b,qb,true);}
    const bool exact=ca==cb&&a==b&&qa==qb&&original.size()==candidate.size()&&
        std::equal(original.begin(),original.end(),candidate.begin(),same_component);
    return {exact,cb,std::move(candidate)};
}
void local_controls(Tests& t) {
    struct Witness {std::array<std::uint8_t,3> rgb;int color;};
    const std::array<Witness,12> witnesses{{{{0,116,166},1},{{181,161,0},2},{{191,0,76},3},{{196,196,196},4},
        {{181,161,255},1},{{255,170,100},2},{{180,255,165},0},{{0,115,255},0},{{190,0,76},0},
        {{191,0,75},0},{{195,195,195},0},{{220,196,255},0}}};
    bool ok=true;for(const auto& v:witnesses)ok&=classify(v.rgb.data())==v.color&&scalar_classify(v.rgb.data())==v.color;
    t.check("classifier_thresholds_and_original_priority",ok);
    constexpr std::array<std::uint8_t,17> values{0,75,76,115,116,160,161,165,166,180,181,190,191,195,196,220,255};
    auto threshold_image=image(int(values.size()*values.size()*values.size()*2),2);int column=0;ok=true;
    for(auto r:values)for(auto g:values)for(auto b:values){std::array<std::uint8_t,3> c{r,g,b};ok&=classify(c.data())==scalar_classify(c.data());pixel(threshold_image,column,0,c);column+=2;}
    const auto threshold=mask_control(threshold_image);
    t.check("4913_RGB_threshold_partition_full_mask_oracle",ok&&threshold.exact&&threshold.guard_unchanged&&threshold.source_unchanged,{{"RGB_cases",4913}});
    for(const auto level:{0,165}) {
        auto f=image(16,2,static_cast<std::uint8_t>(level));const auto m=mask_control(f);
        t.check(level==0?"dark_zero_block_exact_zero_mask":"dark_165_inclusive_unsigned_block",m.exact&&m.guard_unchanged&&m.source_unchanged&&
            m.dark_blocks==(component_mask_dark_fast_path_enabled?1u:0u)&&m.scalar_samples==(component_mask_dark_fast_path_enabled?0u:8u));
    }
    ok=true;for(int byte=0;byte<48;++byte){auto f=image(16,2,165);f.rgb[byte]=166;const auto m=mask_control(f);ok&=m.exact&&m.dark_blocks==0&&m.scalar_samples==8&&m.guard_unchanged;}
    t.check("every_source_byte_166_rejects_conservative_block",ok,{{"positions",48}});
    ok=true;for(int x=1;x<16;x+=2){auto f=image(16,2,0);pixel(f,x,0,{255,255,255});const auto m=mask_control(f);ok&=m.exact&&m.dark_blocks==0&&m.scalar_samples==8;}
    t.check("unsampled_bright_pixels_only_force_scalar_fallback",ok,{{"positions",8}});
    ok=true;for(int x=0;x<16;x+=2)for(const auto& v:witnesses){auto f=image(16,2);pixel(f,x,0,v.rgb);const auto m=mask_control(f);ok&=m.exact&&m.guard_unchanged&&m.source_unchanged;}
    t.check("every_sample_position_preserves_colored_and_priority_witnesses",ok,{{"sample_positions",8},{"witnesses",witnesses.size()}});
    ok=true;for(int width:{2,3,15,16,17,31,32,33,127,128,129})for(int height:{2,3,5}) {
        auto f=image(width,height,30);pixel(f,width-1,height-1,{255,255,255});
        if(width>2)pixel(f,1,0,{40,190,255});const auto m=mask_control(f);
        ok&=m.exact&&m.guard_unchanged&&m.source_unchanged&&m.dark_blocks*8+m.scalar_samples==std::uint64_t((width+1)/2)*((height+1)/2);
    }
    t.check("odd_width_height_and_partial_blocks_exact_and_bounded",ok,{{"geometries",33}});
    auto row=image(32,3,165);rect(row,0,1,32,1,{255,255,255});const auto row_mask=mask_control(row);
    t.check("unsampled_source_row_cannot_change_mask_or_block_bounds",row_mask.exact&&row_mask.guard_unchanged&&row_mask.dark_blocks==(component_mask_dark_fast_path_enabled?4u:0u));
    auto poison=image(32,3);const int pw=(poison.width+1)/2,ph=(poison.height+1)/2;std::vector<std::uint8_t> reused(std::size_t(pw)*ph,4);
    {OracleScope scope(false);fill_component_mask(poison,reused,pw,ph);}
    t.check("dark_blocks_clear_previously_colored_scratch_mask",std::all_of(reused.begin(),reused.end(),[](auto b){return b==0;}));
    auto colors=image(160,80);rect(colors,4,4,8,8,{40,190,255});rect(colors,30,4,8,8,{230,210,20});rect(colors,56,4,8,8,{210,0,90});rect(colors,82,4,8,8,{255,255,255});
    const auto components_colors=component_control(colors);
    t.check("component_scan_BFS_order_and_float_bits_all_four_colors",components_colors.exact&&components_colors.capacity&&components_colors.value.size()==4&&
        components_colors.value[0].color==1&&components_colors.value[1].color==2&&components_colors.value[2].color==3&&components_colors.value[3].color==4);
    auto crossing=image(200,160);rect(crossing,10,78,180,4,{255,255,255});rect(crossing,98,10,4,140,{255,255,255});const auto cross=component_control(crossing);
    t.check("joined_white_cross_retains_exact_split_pixels_and_BFS",cross.exact&&cross.capacity&&cross.value.size()==1&&!cross.value[0].split_pixels.empty());
    auto yellow=image(256,100);rect(yellow,50,10,6,60,{230,210,20});const auto ribbon=component_control(yellow);
    t.check("yellow_ribbon_PCA_projection_and_float_bits_unchanged",ribbon.exact&&ribbon.capacity&&ribbon.value.size()==1&&ribbon.value[0].ribbon_height>0);
    auto noise=image(32,20);pixel(noise,2,2,{40,190,255});pixel(noise,4,2,{40,190,255});const auto tiny=component_control(noise);
    t.check("original_less_than_three_pixel_component_rejection",tiny.exact&&tiny.capacity&&tiny.value.empty());
    auto crowded=image(256,520);for(int i=0;i<2049;++i)for(int dy=0;dy<3;++dy)pixel(crowded,(i%32)*8,(i/32)*8+dy*2,{210,0,90});const auto full=component_control(crowded);
    t.check("original_2048_component_limit_and_capacity_false",full.exact&&!full.capacity&&full.value.size()==2048);
    Frame too_large;too_large.width=2'073'601;too_large.height=4;const auto limited=component_control(too_large);
    t.check("original_sample_capacity_rejects_before_RGB_read",limited.exact&&!limited.capacity&&limited.value.empty());
#if defined(_WIN32) && (defined(_M_X64) || defined(__x86_64__) || (defined(_M_IX86_FP) && _M_IX86_FP >= 2))
    SYSTEM_INFO info{};GetSystemInfo(&info);const auto page=std::size_t(info.dwPageSize);
    auto* memory=static_cast<std::uint8_t*>(VirtualAlloc(nullptr,page*2,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE));bool guard_ok=false;
    if(memory){DWORD previous=0;if(VirtualProtect(memory+page,page,PAGE_NOACCESS,&previous)){
        auto* last=memory+page-48;std::fill_n(last,48,std::uint8_t{165});guard_ok=component_mask_dark_block(last);
        last[47]=166;guard_ok&=!component_mask_dark_block(last);
    }VirtualFree(memory,0,MEM_RELEASE);}
    t.check("exact_48_byte_SSE2_read_ends_before_NOACCESS_page",guard_ok);
#else
    t.check("exact_48_byte_SSE2_read_ends_before_NOACCESS_page",!component_mask_dark_fast_path_enabled,{{"applicable",false},{"reason","portable original scalar path"}});
#endif
    component_mask_oracle_controls.scalar_oracle=false;{OracleScope scalar(true);{OracleScope candidate(false);ok=!component_mask_oracle_controls.scalar_oracle;}ok&=component_mask_oracle_controls.scalar_oracle;}
    t.check("control_only_oracle_scope_restores_nested_mode",ok&&!component_mask_oracle_controls.scalar_oracle);
}
J workload_controls(Tests& t,bool hold) {
    FakeClock clock;GameObserver original(clock),candidate(clock);const int frames=hold?243:96;
    std::uint64_t blocks=0,samples=0;int playing=0,lines=0,targets=0;
    for(int i=1;i<=frames;++i) {
        auto f=image(1280,720);f.sequence=i;f.capture_complete_ns=f.pixels_ready_ns=f.published_ns=1'000'000'000+Nanoseconds(i)*20'000'000;
        if(hold)current_rails::streaming_workload::render(f.rgb,i);else current_rails::tap_reset_workload::render(f.rgb,i);
        const auto mask=mask_control(f);blocks+=mask.dark_blocks;samples+=mask.scalar_samples;
        clock.set(f.capture_complete_ns);DecisionSnapshot a,b;
        {OracleScope scope(true);a=original.process(f);}{OracleScope scope(false);b=candidate.process(f);}
        const bool exact=mask.exact&&mask.source_unchanged&&mask.guard_unchanged&&decision_json(a)==decision_json(b)&&
            a.recognition_start_ns==b.recognition_start_ns&&a.recognition_end_ns==b.recognition_end_ns&&
            a.recognition_start_ns==f.capture_complete_ns&&a.recognition_end_ns==f.capture_complete_ns&&
            !a.compute_timing_host_qpc&&!b.compute_timing_host_qpc;
        t.check(hold?"hold243_complete_mask_and_fakeclock_decision":"tap96_complete_mask_and_fakeclock_decision",exact,{{"attempt",i}});
        playing+=b.playing_gate;lines+=!b.lines.empty();targets+=!b.targets.empty();
    }
    t.check(hold?"hold_sequence_exercises_playing_lines_and_targets":"tap_sequence_exercises_playing_lines_and_targets",playing>0&&lines>0&&targets>0,{{"playing_frames",playing},{"line_frames",lines},{"target_frames",targets}});
    return {{"frames",frames},{"dark_blocks",blocks},{"scalar_samples",samples},{"avoided_scalar_classifications",blocks*8},
        {"total_sampled_pixels",std::uint64_t(frames)*640*360},{"pixels_unchanged",true},{"clock","fake business timestamps; existing host compute durations excluded by existing decision_json contract"}};
}
J mask_benchmark() {
    // Diagnostic only: fixed 24 interleaved pairs per image, no selection,
    // no warm-run deletion and no R2/cost qualification claim.
    J rows=J::array();HostClock clock;
    for(int kind=0;kind<3;++kind) {
        auto f=image(1280,720);if(kind==0)current_rails::tap_reset_workload::render_phase(f.rgb,20,0);
        else if(kind==1)current_rails::streaming_workload::render(f.rgb,30);else std::fill(f.rgb.begin(),f.rgb.end(),std::uint8_t{200});
        std::vector<std::uint8_t> a(640*360),b(640*360);std::vector<double> original,candidate;bool exact=true;
        for(int i=0;i<24;++i) {
            const auto run=[&](bool scalar){const auto begin=clock.now_ns();if(scalar)scalar_mask(f,a);else{OracleScope mode(false);fill_component_mask(f,b,640,360);}const auto elapsed=(clock.now_ns()-begin)/1e6;(scalar?original:candidate).push_back(elapsed);};
            if(i%2==0){run(true);run(false);}else{run(false);run(true);}exact&=a==b;
        }
        const auto metric=[](std::vector<double> values){std::sort(values.begin(),values.end());const auto p=[&](double q){return values[std::size_t(std::ceil(q*values.size()))-1];};return J{{"n",values.size()},{"p50_ms",p(.5)},{"p95_ms",p(.95)},{"p99_ms",p(.99)},{"max_ms",values.back()}};};
        rows.push_back({{"image",kind==0?"declared Tap phase20 offset0":kind==1?"declared Hold attempt30":"all RGB200 conservative fallback"},{"original_scalar",metric(original)},{"candidate",metric(candidate)},{"exact_masks",exact}});
    }
    return {{"qualification",false},{"scope","isolated component mask only; control counters enabled; not whole observer, R2, AA or physical"},{"rows",std::move(rows)}};
}
}
int main(int argc,char** argv) {
    try {
        if((argc!=2&&argc!=3)||(argc==3&&std::string(argv[2])!="--benchmark")||std::filesystem::exists(argv[1]))throw std::runtime_error("fresh output and optional --benchmark required");
        Tests tests;local_controls(tests);J workloads=J::array();workloads.push_back(workload_controls(tests,false));workloads.push_back(workload_controls(tests,true));
        J report={{"schema","pas.component-mask-tests.v1"},{"assertions",tests.checks.size()},{"checks",tests.checks},{"failed",tests.failed},{"pass",tests.failed==0},
            {"device_endpoints",0},{"device_operations",0},{"cost_qualification",false},{"live_ready",false},{"gameplay_archive",false},
            {"dark_block_sample_count",8},{"dark_block_source_bytes",48},{"dark_block_max_byte",165},{"SSE2_fast_path",component_mask_dark_fast_path_enabled},
            {"scalar_oracle","frozen pre-repair scalar classifier and full mask; control-only original mask path for complete decisions"},{"workloads",std::move(workloads)}};
        if(argc==3)report["diagnostic_mask_benchmark"]=mask_benchmark();
        std::ofstream output(argv[1]);output<<report.dump(2)<<'\n';if(!output)throw std::runtime_error("write control report");
        std::cout<<"assertions="<<tests.checks.size()<<" failed="<<tests.failed<<'\n';return tests.failed?1:0;
    }catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 2;}
}
