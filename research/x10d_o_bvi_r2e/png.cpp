#include "bvi.hpp"
#include <windows.h>
#include <wincodec.h>
#include <bcrypt.h>
#include <wrl/client.h>
#include <nlohmann/json.hpp>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iomanip>
#include <sstream>
using J=nlohmann::json;namespace fs=std::filesystem;using Microsoft::WRL::ComPtr;
namespace {
J read(const fs::path&p){std::ifstream f(p);if(!f)throw std::runtime_error("missing input");return J::parse(f);}
void need(HRESULT code){if(FAILED(code))throw std::runtime_error("WIC HRESULT:"+std::to_string(code));}
std::string sha(const fs::path&p){BCRYPT_ALG_HANDLE alg{};BCRYPT_HASH_HANDLE h{};if(BCryptOpenAlgorithmProvider(&alg,BCRYPT_SHA256_ALGORITHM,nullptr,0)<0)throw std::runtime_error("hash-provider");try{if(BCryptCreateHash(alg,&h,nullptr,0,nullptr,0,0)<0)throw std::runtime_error("hash-create");std::ifstream f(p,std::ios::binary);if(!f)throw std::runtime_error("hash-open");std::array<unsigned char,65536>b{};while(f){f.read(reinterpret_cast<char*>(b.data()),b.size());auto n=f.gcount();if(n&&BCryptHashData(h,b.data(),static_cast<ULONG>(n),0)<0)throw std::runtime_error("hash-update");}if(!f.eof())throw std::runtime_error("hash-read");std::array<unsigned char,32>out{};if(BCryptFinishHash(h,out.data(),out.size(),0)<0)throw std::runtime_error("hash-final");std::ostringstream s;for(auto x:out)s<<std::hex<<std::setw(2)<<std::setfill('0')<<static_cast<int>(x);BCryptDestroyHash(h);BCryptCloseAlgorithmProvider(alg,0);return s.str();}catch(...){if(h)BCryptDestroyHash(h);BCryptCloseAlgorithmProvider(alg,0);throw;}}
}
int main(int argc,char**argv){try{
 if(argc!=3)throw std::runtime_error("packet output required");auto packet=read(argv[1]);if(packet["frames"].size()!=52)throw std::runtime_error("52 denominator");auto pending=read(argv[2]);if(pending.value("attempt",std::string{})!="bvi-r2e-20261005-01"||!pending.value("pending",false))throw std::runtime_error("output-reservation");
 need(CoInitializeEx(nullptr,COINIT_MULTITHREADED));ComPtr<IWICImagingFactory>factory;need(CoCreateInstance(CLSID_WICImagingFactory,nullptr,CLSCTX_INPROC_SERVER,IID_PPV_ARGS(factory.GetAddressOf())));
 J rows=J::array();bvi::Candidate candidate;std::int64_t previous{};int segment=0;
 for(const auto&f:packet["frames"]){const fs::path path=f["path"].get<std::string>();if(fs::file_size(path)>4194304||sha(path)!=f["png_sha256"].get<std::string>())throw std::runtime_error("PNG SHA/cap");ComPtr<IWICBitmapDecoder>decoder;need(factory->CreateDecoderFromFilename(path.c_str(),nullptr,GENERIC_READ,WICDecodeMetadataCacheOnLoad,&decoder));ComPtr<IWICBitmapFrameDecode>frame;need(decoder->GetFrame(0,&frame));UINT w{},h{};need(frame->GetSize(&w,&h));if(w!=1280||h!=720)throw std::runtime_error("geometry");ComPtr<IWICFormatConverter>convert;need(factory->CreateFormatConverter(&convert));need(convert->Initialize(frame.Get(),GUID_WICPixelFormat24bppRGB,WICBitmapDitherTypeNone,nullptr,0,WICBitmapPaletteTypeCustom));std::vector<std::uint8_t>rgb(w*h*3);need(convert->CopyPixels(nullptr,w*3,static_cast<UINT>(rgb.size()),rgb.data()));
  const std::int64_t capture=f["capture_complete_ns"];if(!previous||capture-previous>40000000){candidate=bvi::Candidate{};++segment;}previous=capture;
  // No geometric information is inferred from authoring reasons, physical labels or past actions.
  // This packet explicitly records a provenance gap. Discovery is outside this local recovery.
  if(!f["legal_current_rois"].is_null()||!f["legal_current_lines"].is_null())throw std::runtime_error("undeclared geometry mode");
  rows.push_back({{"ordinal",f["ordinal"]},{"png_sha256",f["png_sha256"]},{"source_frame",f["source_frame"]},{"capture_complete_ns",capture},{"pixels_ready_ns",f["pixels_ready_ns"]},{"clock_domain","host_qpc_ns"},{"source_timestamp_domain",f["source_timestamp_domain"]},{"segment",segment},{"history_samples",0},{"history_span_ns",0},{"decoded_rgb_bytes",rgb.size()},{"status","unknown"},{"reason","missing_provenance_qualified_current_ROI_and_all_line_geometry"},{"candidate_executed",false},{"physical_owner",nullptr},{"action_eligibility",nullptr}});
 }
 J result={{"schema","bvi.r2e.png.v1"},{"mode","current-pixels-provenance-gap"},{"rows",rows},{"supported",0},{"invalid",0},{"unknown",52},{"integrity_failures",0},{"core_counterexamples",0},{"candidate_executed",false},{"png_audits_used_this_attempt",1},{"human_gold",0},{"touch_backend",false},{"full_prefix",false},{"max_samples",6},{"max_span_ns",90000000},{"packet_sha256",sha(argv[1])}};auto s=result.dump(2)+"\n";if(s.size()>2097152)throw std::runtime_error("report-cap");std::ofstream out(argv[2],std::ios::trunc);out<<s;out.flush();if(!out)throw std::runtime_error("report-write");factory.Reset();CoUninitialize();std::cout<<"frames=52 unknown=52 legal_geometry_gap=52\n";return 0;
 }catch(const std::exception&e){std::cerr<<e.what()<<"\n";return 1;}}
