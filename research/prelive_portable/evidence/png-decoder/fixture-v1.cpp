#include "portable_png.hpp"
#include <nlohmann/json.hpp>
#include <iostream>
using J=nlohmann::json;using namespace pas::portable_png;
void append(std::vector<std::uint8_t>&b,std::uint32_t v){for(int i=3;i>=0;--i)b.push_back(static_cast<std::uint8_t>(v>>(8*i)));}
void chunk(std::vector<std::uint8_t>&b,std::string_view type,const std::vector<std::uint8_t>&payload){append(b,payload.size());const auto at=b.size();b.insert(b.end(),type.begin(),type.end());b.insert(b.end(),payload.begin(),payload.end());append(b,crc32(0,b.data()+at,payload.size()+4));}
std::vector<std::uint8_t> fixture(const std::vector<std::uint8_t>&rgb){std::vector<std::uint8_t>raw;for(int y=0;y<720;++y){raw.push_back(0);raw.insert(raw.end(),rgb.begin()+y*3840,rgb.begin()+(y+1)*3840);}uLongf n=compressBound(raw.size());std::vector<std::uint8_t>z(n);if(compress2(z.data(),&n,raw.data(),raw.size(),6)!=Z_OK)throw std::runtime_error("fixture compression");z.resize(n);std::vector<std::uint8_t>b={137,80,78,71,13,10,26,10};std::vector<std::uint8_t>h;append(h,1280);append(h,720);h.insert(h.end(),{8,2,0,0,0});chunk(b,"IHDR",h);chunk(b,"IDAT",z);chunk(b,"IEND",{});return b;}
int main(int argc,char**argv){try{if(argc!=3||std::filesystem::exists(argv[2]))throw std::runtime_error("usage ACTUAL_PNG FRESH_REPORT");J checks=J::array();int failed=0;auto check=[&](std::string id,bool ok){checks.push_back({{"id",id},{"pass",ok}});failed+=!ok;};
 std::vector<std::uint8_t>rgb(rgb_bytes);for(int y=0;y<720;++y)for(int x=0;x<1280;++x){const auto p=(std::size_t(y)*1280+x)*3;rgb[p]=x%256;rgb[p+1]=y%256;rgb[p+2]=(x+3*y)%256;}
 auto original=fixture(rgb);check("full_rgb8_gradient_exact_top_down_channels",decode(original)==rgb);
 auto reject=[&](std::string id,std::vector<std::uint8_t>b){bool threw=false;try{decode(b);}catch(const std::exception&){threw=true;}check(id,threw);};
 auto b=original;b[0]=0;reject("signature",b);b=original;b.pop_back();reject("truncated",b);b=original;b.push_back(0);reject("trailing",b);b=original;b[50]^=1;reject("IDAT_CRC",b);b=original;b[20]=0;reject("header_CRC",b);b=original;b.resize(25);reject("short_header",b);b=original;b.resize(compressed_cap+1);reject("compressed_capacity",b);
 b=original;b[24]=16;const auto crc=crc32(0,b.data()+12,17);b[29]=crc>>24;b[30]=crc>>16;b[31]=crc>>8;b[32]=crc;reject("valid_CRC_wrong_bitdepth",b);
 const auto actual=read(argv[1]);const auto decoded=decode(actual);check("actual_archived_RGB_profile",decoded.size()==rgb_bytes);check("installed_library_version",std::string(png_get_libpng_ver(nullptr))=="1.6.48");
 J j={{"schema","pas.portable-libpng.contract.v1"},{"assertions",checks.size()},{"failed",failed},{"checks",checks},{"libpng_runtime",png_get_libpng_ver(nullptr)},{"actual_png_sha256",digest(actual)},{"actual_rgb_sha256",digest(decoded)},{"device_endpoints",0}};std::ofstream out(argv[2]);out<<j.dump(2)<<'\n';std::cout<<"png checks="<<checks.size()<<" failed="<<failed<<'\n';return failed?1:0;
 }catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 2;}}
