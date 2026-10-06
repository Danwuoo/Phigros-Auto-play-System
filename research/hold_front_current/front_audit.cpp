#define NOMINMAX
#include <windows.h>
#include <wincodec.h>
#include <wrl/client.h>
#include "pixel_diagnostics.hpp"
#include <array>
#include <cmath>
#include <fstream>
#include <iostream>
#include <stdexcept>

using J=nlohmann::json;
using Microsoft::WRL::ComPtr;
namespace {
void require(bool pass,const char* reason){if(!pass)throw std::runtime_error(reason);}
void checked(HRESULT value){require(SUCCEEDED(value),"audit WIC failure");}
struct Reader {
    std::ifstream file;
    explicit Reader(const std::filesystem::path& path):file(path,std::ios::binary){
        require(std::filesystem::file_size(path)<=32*1024*1024,"audit trace file bound");
        require(bool(file),"audit trace open");
    }
    bool next(J& row){
        std::string text;
        // The entire file is capped at 32 MiB; a parsed row is capped at 8 MiB.
        if(!std::getline(file,text)){require(file.eof(),"audit trace read");return false;}
        require(text.size()<=8*1024*1024,"audit trace row bound");
        row=J::parse(text);return true;
    }
};
void remove_diagnostics(J& row){
    for(const auto* key:{"host_decode_start_ns","host_pixels_ready_ns","host_recognition_complete_ns",
                        "host_bridge_complete_ns","host_complete_ns"})row.erase(key);
    row.erase("current_front_sidecar");
    for(auto& roi:row.at("roi"))for(const auto* key:{"post_dispatch_witness_review","query_width","query_angle"})roi.erase(key);
}
std::vector<std::uint8_t> decode(IWICImagingFactory* factory,const std::filesystem::path& path){
    ComPtr<IWICBitmapDecoder> decoder;checked(factory->CreateDecoderFromFilename(path.c_str(),nullptr,
        GENERIC_READ,WICDecodeMetadataCacheOnDemand,&decoder));
    ComPtr<IWICBitmapFrameDecode> image;checked(decoder->GetFrame(0,&image));
    UINT width=0,height=0;checked(image->GetSize(&width,&height));require(width==1280&&height==720,"audit pixel profile");
    ComPtr<IWICFormatConverter> converter;checked(factory->CreateFormatConverter(&converter));
    checked(converter->Initialize(image.Get(),GUID_WICPixelFormat24bppRGB,WICBitmapDitherTypeNone,nullptr,0,WICBitmapPaletteTypeCustom));
    std::vector<std::uint8_t> bytes(1280*720*3);
    checked(converter->CopyPixels(nullptr,3840,static_cast<UINT>(bytes.size()),bytes.data()));return bytes;
}
}
int main(int argc,char** argv){
    if(argc!=6){std::cerr<<"audit REFERENCE_TRACE REVIEW_TRACE SELECTION ANNOTATION FRESH_REPORT\n";return 2;}
    if(std::filesystem::exists(argv[5])){std::cerr<<"fresh audit report required\n";return 2;}
    J report={{"schema","pas.windows-independent-pixel-diagnostic-audit.v1"},{"passed",false},
        {"compared_rows",0},{"witness_queries",0},{"raw_png_points_verified",0},{"physical_human_gold",0},{"device_commands",0}};
    int exit=0;
    try {
        require(std::filesystem::file_size(argv[3])<=2*1024*1024,"audit selection bound");
        std::ifstream selected(argv[3]);const auto selection=J::parse(selected);
        const auto& frames=selection.at("frames");
        require(!frames.empty()&&frames.size()<=256&&frames.size()==selection.at("selected_count"),"audit frame count");
        Reader before(argv[1]),after(argv[2]);J reference,row;
        checked(CoInitializeEx(nullptr,COINIT_MULTITHREADED));
        ComPtr<IWICImagingFactory> factory;checked(CoCreateInstance(CLSID_WICImagingFactory,nullptr,
            CLSCTX_INPROC_SERVER,IID_PPV_ARGS(&factory)));
        std::size_t rows=0,queries=0,points=0,witness_frames=0;
        std::array<int,2> witness_ordinals{};
        J kinds=J::object(),reasons=J::object(),witness=nullptr;std::size_t raw_count=0,boundaries=0,edge_points=0;
        while(after.next(row)) {
            require(++rows<=256&&before.next(reference),"audit row denominator");
            const int ordinal=row.at("ordinal");
            const auto& entry=frames.at(rows-1);
            require(ordinal==entry.at("index").at("ordinal")&&row.at("source_frame")==entry.at("index").at("source_frame")&&
                row.at("recorded_capture_ns")==entry.at("index").at("capture_complete_ns"),"audit exact current-frame provenance");
            require(row.at("roi").size()<=128&&row.at("raw_candidates").size()<=128,"audit ROI capacity");
            bool has_witness=false;
            for(const auto& roi:row.at("roi"))has_witness|=roi.contains("post_dispatch_witness_review");
            std::vector<std::uint8_t> rgb;
            if(has_witness){
                require(ordinal==1519||ordinal==3030,"audit diagnostic ordinal scope");
                require(witness_frames<2,"audit diagnostic frame capacity");
                for(std::size_t i=0;i<witness_frames;++i)require(witness_ordinals[i]!=ordinal,"audit duplicate witness frame");
                witness_ordinals[witness_frames++]=ordinal;
                rgb=decode(factory.Get(),entry.at("path").get<std::string>());
            }
            for(const auto& roi:row.at("roi")) {
                if(!roi.contains("post_dispatch_witness_review"))continue;
                ++queries;const J* raw=nullptr;std::size_t matches=0;
                for(const auto& item:row.at("raw_candidates"))if(item.at("candidate_id")==roi.at("candidate_id")){raw=&item;++matches;}
                require(matches==1&&raw,"audit unique current proposal binding");
                require(raw->at("center")==roi.at("query_front"),"audit front differs from current proposal");
                const double width=raw->at("width"),tx=raw->at("tangent")[0],ty=raw->at("tangent")[1];
                require(std::isfinite(width)&&width>=4&&width<=4096&&std::isfinite(tx)&&std::isfinite(ty),"audit finite query geometry");
                double angle=std::atan2(ty,tx);
                const double fx=roi.at("query_front")[0],fy=roi.at("query_front")[1];
                require(std::isfinite(fx)&&std::isfinite(fy)&&fx>=0&&fx<1280&&fy>=0&&fy<720,"audit current front bound");
                if(!roi.at("query_tap").get<bool>()) {
                    require(raw->at("tail").is_array()&&raw->at("tail").size()==2,"audit current hold tail");
                    const double dx=fx-raw->at("tail")[0].get<double>(),dy=fy-raw->at("tail")[1].get<double>();
                    double depth=-std::sin(angle)*dx+std::cos(angle)*dy;
                    if(depth<0){angle+=std::acos(-1.);depth=-depth;}
                    require(std::isfinite(depth)&&std::abs(depth-roi.at("query_depth").get<double>())<1e-8,"audit hold normal depth");
                }
                if(roi.contains("query_width"))require(width==roi.at("query_width"),"audit declared width");
                if(roi.contains("query_angle"))require(std::abs(angle-roi.at("query_angle").get<double>())<1e-12,"audit declared angle");
                const auto& normal_rows=roi.at("post_dispatch_witness_review");
                require(normal_rows.size()==37,"audit normal sample bound");
                const double ux=std::cos(angle),uy=std::sin(angle),nx=-uy,ny=ux;
                const std::array<double,5> positions{-width/2,-.35*width,0.,.35*width,width/2};
                for(std::size_t n=0;n<37;++n) {
                    const auto& part=normal_rows[n];const int normal=static_cast<int>(n)-16;
                    require(part.at("normal")==normal&&part.at("points").size()==5,"audit exact normal/tangent coverage");
                    for(std::size_t t=0;t<5;++t){
                        ++points;const auto& point=part.at("points")[t];
                        require(point.at("along")==positions[t],"audit tangent location");
                        const auto x=std::llround(fx+positions[t]*ux+normal*nx),y=std::llround(fy+positions[t]*uy+normal*ny);
                        require(point.at("pixel")==J::array({x,y}),"audit rotated rounded coordinate");
                        J expected=nullptr,failures=J::array();bool blue=false,white=false,black=false;
                        if(x>=0&&x<1280&&y>=0&&y<720){
                            const auto index=static_cast<std::size_t>(y)*3840+static_cast<std::size_t>(x)*3;
                            const int r=rgb[index],g=rgb[index+1],b=rgb[index+2];expected=J::array({r,g,b});
                            if(r<20||r>100)failures.push_back("R20..100");
                            if(g<130||g>230)failures.push_back("G130..230");
                            if(b<180||b>255)failures.push_back("B180..255");
                            if(b<g+20)failures.push_back("B>=G+20");
                            blue=failures.empty();white=r>=240&&g>=240&&b>=240;black=r<=12&&g<=12&&b<=12;
                        }else failures.push_back("outside_frame");
                        require(point.at("rgb")==expected,"audit raw PNG pixel mismatch");
                        require(point.at("v3_blue")==blue&&point.at("v3_white")==white&&point.at("v3_black")==black&&
                            point.at("v3_blue_failures")==failures,"audit diagnostic predicate mismatch");
                    }
                }
            }
            if(rgb.empty())rgb=decode(factory.Get(),entry.at("path").get<std::string>());
            const auto& side=row.at("current_front_sidecar");const auto& results=side.at("results");
            require(side.at("flow")=="post_dispatch_geometry_only"&&!side.at("action_authorized").get<bool>(),"audit no-action flow");
            require(results.size()==row.at("raw_candidates").size()&&results.size()==side.at("complete_candidate_count"),"audit complete raw sidecar denominator");
            raw_count+=results.size();
            for(std::size_t i=0;i<results.size();++i){
                const auto& r=results[i];const auto& raw=row.at("raw_candidates")[i];
                const auto k=r.at("kind").get<std::string>(),why=r.at("reason").get<std::string>();
                kinds[k]=kinds.value(k,0)+1;reasons[why]=reasons.value(why,0)+1;
                require(r.at("candidate_id")==raw.at("candidate_id")&&r.at("origin")==raw.at("origin")&&
                    r.at("observer_front")==raw.at("center"),"audit current proposal provenance");
                require(r.at("context").at("frame")==row.at("source_frame")&&r.at("context").at("capture_ns")==row.at("recorded_capture_ns")&&
                    r.at("pixels_ready_ns")==row.at("recorded_pixels_ready_ns"),"audit boundary frame provenance");
                require(!r.at("action_authorized").get<bool>()&&r.at("tail_role")=="unknown; current tail proposal or missing; not measured here", "audit roles are not physical gold");
                require(r.at("probes").get<int>()<=975,"audit front probe bound");
                if(r.at("boundary").is_null()){
                    require(!r.at("typed_seam_verified").get<bool>()&&r.at("edge_samples").empty(),"audit abstention projection");continue;
                }
                ++boundaries;require(k=="visible_terminal_proposed_head"&&r.at("head_role")=="proposed"&&
                    r.at("typed_seam_verified").get<bool>()&&raw.at("kind")==1&&!raw.at("held_body_patch").get<bool>(),"audit end is proposed geometry only");
                const double fx=raw.at("center")[0],fy=raw.at("center")[1],width=raw.at("width");
                const double ux=raw.at("tangent")[0],uy=raw.at("tangent")[1];double nx=-uy,ny=ux;
                require(raw.at("tail").is_array(),"audit current normal proposal");
                const double dx=fx-raw.at("tail")[0].get<double>(),dy=fy-raw.at("tail")[1].get<double>();
                if(dx*nx+dy*ny<0){nx=-nx;ny=-ny;}
                require(std::abs(nx-r.at("normal")[0].get<double>())<1e-12&&std::abs(ny-r.at("normal")[1].get<double>())<1e-12,"audit local normal");
                auto sample=[&](double along,int offset){
                    const auto x=std::llround(fx+along*ux+offset*nx),y=std::llround(fy+along*uy+offset*ny);
                    require(x>=0&&x<1280&&y>=0&&y<720,"audit supporting pixels inside raw PNG");
                    const auto at=std::size_t(y)*3840+std::size_t(x)*3;
                    return J{{"pixel",{x,y}},{"rgb",{rgb[at],rgb[at+1],rgb[at+2]}}};};
                const std::array<double,5> lanes{-.30,-.15,0.,.15,.30};std::array<int,5> offsets{};
                require(r.at("edge_samples").size()==5,"audit five separate cross-section lanes");
                const int anchor=r.at("support_anchor_offset");require(anchor>=-20&&anchor<=20,"audit bounded support anchor");
                for(int t=0;t<5;++t){const auto& lane=r.at("edge_samples")[t];const int last=lane.at("last_inside_offset");
                    require(last>=anchor-1&&last<=anchor+1&&lane.at("along")==lanes[t]*width,"audit measured lane interval");offsets[t]=last;
                    const auto inside=sample(lanes[t]*width,last),outside=sample(lanes[t]*width,last+1);
                    require(inside==lane.at("inside")&&outside==lane.at("outside"),"audit front raw RGB mismatch");edge_points+=2;
                    int sum=0,largest=0;for(int channel=0;channel<3;++channel){const int diff=inside.at("rgb")[channel].get<int>()-outside.at("rgb")[channel].get<int>();sum+=diff;largest=std::max(largest,diff);}
                    require(sum>=60&&largest>=40,"audit interior exterior contrast");
                }
                std::sort(offsets.begin(),offsets.end());const double middle=offsets[2]+.5;
                require(r.at("offset_interval")==J::array({double(offsets.front()),double(offsets.back()+1)})&&
                    std::abs(r.at("boundary")[0].get<double>()-(fx+middle*nx))<1e-10&&
                    std::abs(r.at("boundary")[1].get<double>()-(fy+middle*ny))<1e-10,"audit transition interval geometry");
                int left=0,right=0,outside_rows=0;
                auto rail=[&](int sign,int offset){bool supported=false;for(int d=-2;d<=2;++d){const auto p=sample(sign*width/2+d,offset).at("rgb");
                    supported|=p[0].get<int>()>=240&&p[1].get<int>()>=240&&p[2].get<int>()>=240;}return supported;};
                for(int d=4;d<=12;++d){left+=rail(-1,anchor-d);right+=rail(1,anchor-d);}
                for(int d=2;d<=4;++d)outside_rows+=rail(-1,anchor+d)||rail(1,anchor+d);
                require(left>=6&&right>=6&&outside_rows<=1&&left==r.at("left_rail_rows")&&right==r.at("right_rail_rows"),"audit independent raw rails support");
                if(ordinal==3030&&r.at("candidate_id")==2)witness=r;
            }
            auto decision=row;remove_diagnostics(decision);remove_diagnostics(reference);
            require(decision==reference,"audit decision field changed");
        }
        require(!before.next(reference)&&rows==frames.size(),"audit complete row denominator");
        require(witness_frames==2&&queries==9&&points==queries*185,"audit complete fixed witness denominator");
        require(std::filesystem::file_size(argv[4])<=65536,"audit annotation bound");
        std::ifstream annotation_file(argv[4]);const auto annotation=J::parse(annotation_file);
        require(!witness.is_null()&&annotation.at("labels")[0].at("policy_use")==false,"audit external annotation only");
        const auto point=annotation.at("labels")[0].at("approximate_point");
        report["front_raw_candidates"]=raw_count;report["visible_terminal_proposals"]=boundaries;
        report["front_edge_raw_points_verified"]=edge_points;report["front_kind_counts"]=kinds;report["front_reason_counts"]=reasons;
        report["frame3030_candidate2"]=witness;
        report["external_annotation_comparison"]={{"annotation_status",annotation.at("labels")[0].at("status")},
            {"measured_minus_approximate",{witness.at("boundary")[0].get<double>()-point[0].get<double>(),witness.at("boundary")[1].get<double>()-point[1].get<double>()}},
            {"policy_use",false},{"precision","approximate semantics; no exact touch or legal Down gold"}};
        report["passed"]=true;report["compared_rows"]=rows;report["witness_queries"]=queries;
        report["raw_png_points_verified"]=points;report["current_witness_frames"]=witness_frames;
        report["excluded_fields"]={"five current-host timing fields","post_dispatch_witness_review","query_width","query_angle"};
        report["scope"]="frozen 256-frame selection; current raw PNG coordinates and policy fields; no physical gold";
    }catch(const std::exception& e){exit=1;report["error"]=e.what();}
    std::ofstream out(argv[5]);out<<report.dump(2)<<'\n';if(!out)return 2;
    std::cout<<"independent pixel audit passed="<<report.at("passed")<<'\n';return exit;
}
