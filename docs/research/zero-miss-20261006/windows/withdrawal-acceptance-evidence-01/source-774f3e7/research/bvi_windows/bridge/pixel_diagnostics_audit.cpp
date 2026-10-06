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
    if(argc!=5){std::cerr<<"audit REFERENCE_TRACE REVIEW_TRACE SELECTION FRESH_REPORT\n";return 2;}
    if(std::filesystem::exists(argv[4])){std::cerr<<"fresh audit report required\n";return 2;}
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
            auto decision=row;remove_diagnostics(decision);remove_diagnostics(reference);
            require(decision==reference,"audit decision field changed");
        }
        require(!before.next(reference)&&rows==frames.size(),"audit complete row denominator");
        require(witness_frames==2&&queries==9&&points==queries*185,"audit complete fixed witness denominator");
        report["passed"]=true;report["compared_rows"]=rows;report["witness_queries"]=queries;
        report["raw_png_points_verified"]=points;report["current_witness_frames"]=witness_frames;
        report["excluded_fields"]={"five current-host timing fields","post_dispatch_witness_review","query_width","query_angle"};
        report["scope"]="frozen 256-frame selection; current raw PNG coordinates and policy fields; no physical gold";
    }catch(const std::exception& e){exit=1;report["error"]=e.what();}
    std::ofstream out(argv[4]);out<<report.dump(2)<<'\n';if(!out)return 2;
    std::cout<<"independent pixel audit passed="<<report.at("passed")<<'\n';return exit;
}
