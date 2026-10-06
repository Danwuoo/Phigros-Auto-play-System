#include "pixel_diagnostics.hpp"
#include <cmath>
#include <fstream>
#include <iostream>
#include <limits>
#include <memory>

using namespace pas;
using namespace pas::bvi_offline::diagnostic;
using J=nlohmann::json;
int main(int argc,char** argv) {try {
    if(argc!=2||std::filesystem::exists(argv[1]))throw std::runtime_error("fresh diagnostic report required");
    J checks=J::array();int failed=0;
    auto check=[&](std::string id,bool pass){checks.push_back({{"id",id},{"pass",pass}});failed+=!pass;};
    Frame f;f.width=f.height=64;f.stride=192;f.rgb.resize(64*64*3);
    for(int y=0;y<64;++y)for(int x=0;x<64;++x){const auto i=y*f.stride+x*3;f.rgb[i]=x;f.rgb[i+1]=y;f.rgb[i+2]=200;}
    check("rounding_uses_actual_integer_pixel",rgb_at(f,10.49,20.5)==J::array({10,21,200}));
    check("fractional_inside_border",rgb_at(f,-.49,0)==J::array({0,0,200}));
    check("negative_half_rounds_outside",rgb_at(f,-.5,0).is_null());
    check("right_half_rounds_outside",rgb_at(f,63.5,0).is_null());
    check("nonfinite_pixel_abstains",rgb_at(f,std::numeric_limits<double>::quiet_NaN(),0).is_null());
    check("huge_pixel_abstains_before_integer_cast",rgb_at(f,std::numeric_limits<double>::max(),0).is_null());
    auto bad=f;bad.stride=-1;check("negative_stride_abstains",rgb_at(bad,0,0).is_null());
    bad=f;bad.rgb.pop_back();check("truncated_storage_abstains",rgb_at(bad,0,0).is_null());
    bad=f;bad.width=1281;check("oversized_width_abstains",rgb_at(bad,0,0).is_null());
    bvi::Query q{{32,32},10,20,std::acos(-1.)/2,false};
    const auto rows=witness_review(f,q);
    std::size_t points=0;bool geometry=true;
    for(const auto& row:rows){
        const auto normal=row.at("normal").get<int>();points+=row.at("points").size();
        for(const auto& p:row.at("points")){
            const auto along=p.at("along").get<double>();
            geometry&=p.at("pixel")==J::array({32-normal,std::llround(32+along)})&&
                p.at("rgb")==rgb_at(f,32-normal,32+along);
        }
    }
    check("bounded_exact_37_by_5_points",rows.size()==37&&points==185&&points==points_per_query);
    check("rotated_query_uses_note_local_normal",geometry);
    q.front={0,0};const auto border=witness_review(f,q);
    bool outside=false;for(const auto& row:border)for(const auto& p:row.at("points"))
        if(p.at("rgb").is_null())outside|=!p.at("v3_blue").get<bool>()&&!p.at("v3_white").get<bool>()&&
            !p.at("v3_black").get<bool>()&&p.at("v3_blue_failures")==J::array({"outside_frame"});
    check("outside_frame_not_color_evidence",outside);
    q.front.x=std::numeric_limits<double>::infinity();bool rejected=false;
    try{witness_review(f,q);}catch(const std::invalid_argument&){rejected=true;}
    check("nonfinite_query_rejected",rejected);
    q={{32,32},4097,20,0,false};rejected=false;
    try{witness_review(f,q);}catch(const std::invalid_argument&){rejected=true;}
    check("oversized_query_rejected",rejected);
    // Cross-check diagnostic blue against the unchanged v3 pixel extractor,
    // rather than a second expected implementation of the new helper.
    const std::array<std::array<int,3>,17> colors{{{19,130,180},{20,130,180},{100,130,180},{101,130,180},
        {20,129,180},{20,130,180},{20,230,250},{20,231,255},{20,130,179},{20,160,180},
        {20,161,180},{10,195,255},{192,226,237},{193,227,238},{156,233,255},{255,255,255},{12,12,12}}};
    auto candidate=std::make_unique<bvi::Candidate>();
    for(std::size_t i=0;i<colors.size();++i){
        for(std::size_t p=0;p<f.rgb.size();p+=3)for(int c=0;c<3;++c)f.rgb[p+c]=static_cast<std::uint8_t>(colors[i][c]);
        bvi::Query tap{{32,32},4,1,0,true};const bvi::Key key{{1,1,1,1},0,0,1,true};
        const auto extraction=candidate->extract({f.rgb,f.width,f.height,f.stride,key},{&tap,1},{});
        const auto review=witness_review(f,tap);
        check("frozen_v3_blue_boundary_"+std::to_string(i),!extraction.invalid&&
            review[16]["points"][2]["v3_blue"].get<bool>()==extraction.parts[0].front_end);
    }
    const auto serialized=rows.dump();check("sample_json_has_bounded_size",serialized.size()<65536);
    J report={{"schema","pas.windows-pixel-diagnostics-contract.v1"},{"assertions",checks.size()},
        {"failed_assertions",failed},{"checks",checks},{"points_per_query",points_per_query},
        {"trace_cap_bytes",trace_cap_bytes},{"physical_human_gold",0},{"device_commands",0}};
    std::ofstream out(argv[1]);out<<report.dump(2)<<'\n';if(!out)return 2;
    std::cout<<"diagnostic assertions="<<checks.size()<<" failed="<<failed<<'\n';return failed?1:0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 2;}}
