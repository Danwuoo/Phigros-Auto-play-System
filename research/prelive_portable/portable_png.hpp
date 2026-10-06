#pragma once
// Contract/bounds wrapper only. Pixel decompression/filtering is the installed
// official libpng 1.6.48 implementation, never a home-grown PNG decoder.
#include <png.h>
#include <zlib.h>
#include "evidence_hash.hpp"
#include <algorithm>
#include <span>
#include <vector>
namespace pas::portable_png {
constexpr std::size_t rgb_bytes=1280*720*3,compressed_cap=4*1024*1024;
inline std::uint32_t be(const std::uint8_t*p){return (std::uint32_t(p[0])<<24)|(std::uint32_t(p[1])<<16)|(std::uint32_t(p[2])<<8)|p[3];}
inline std::string digest(std::span<const std::uint8_t>b){evidence::Sha256 h;h.update(reinterpret_cast<const char*>(b.data()),b.size());return h.finish();}
inline std::vector<std::uint8_t> read(const std::filesystem::path&p){
 const auto n=std::filesystem::file_size(p);if(n<57||n>compressed_cap)throw std::runtime_error("PNG file bound");
 std::vector<std::uint8_t>b(n);std::ifstream f(p,std::ios::binary);f.read(reinterpret_cast<char*>(b.data()),b.size());if(!f||f.peek()!=std::char_traits<char>::eof())throw std::runtime_error("PNG unstable read");return b;
}
inline std::vector<std::uint8_t> decode(std::span<const std::uint8_t>b){
 if(std::string_view(png_get_libpng_ver(nullptr))!="1.6.48")throw std::runtime_error("libpng version mismatch");
 constexpr std::array<std::uint8_t,8> signature={137,80,78,71,13,10,26,10};
 if(b.size()<57||b.size()>compressed_cap||!std::equal(signature.begin(),signature.end(),b.begin()))throw std::runtime_error("PNG signature/bound");
 std::size_t at=8,chunks=0;bool idat=false,end=false;
 while(at<b.size()){
  if(++chunks>4096||b.size()-at<12)throw std::runtime_error("PNG chunk bound");const auto n=be(b.data()+at);if(n>b.size()-at-12)throw std::runtime_error("PNG chunk length");
  const std::string_view type(reinterpret_cast<const char*>(b.data()+at+4),4);
  const auto crc=crc32(0,b.data()+at+4,n+4);if(crc!=be(b.data()+at+8+n))throw std::runtime_error("PNG CRC");
  if(chunks==1){if(type!="IHDR"||n!=13||be(b.data()+at+8)!=1280||be(b.data()+at+12)!=720||b[at+16]!=8||b[at+17]!=2||b[at+18]||b[at+19]||b[at+20])throw std::runtime_error("PNG exact RGB24 profile");}
  else if(type=="IHDR")throw std::runtime_error("PNG duplicate IHDR");
  // Reject color-management metadata rather than silently reinterpret the
  // original unannotated RGB byte contract through a color conversion.
  if(type=="gAMA"||type=="cHRM"||type=="iCCP"||type=="sRGB")throw std::runtime_error("PNG color metadata requires explicit mapping");
  if(type=="IDAT")idat=true;
  at+=n+12;if(type=="IEND"){if(n||!idat||at!=b.size())throw std::runtime_error("PNG IEND order/trailing");end=true;break;}
 }
 if(!end)throw std::runtime_error("PNG missing IEND");
 png_image image{};image.version=PNG_IMAGE_VERSION;
 if(!png_image_begin_read_from_memory(&image,b.data(),b.size())){const std::string reason=image.message;png_image_free(&image);throw std::runtime_error("libpng begin: "+reason);}
 if(image.width!=1280||image.height!=720){png_image_free(&image);throw std::runtime_error("libpng geometry");}
 image.format=PNG_FORMAT_RGB;std::vector<std::uint8_t>rgb(rgb_bytes);
 if(!png_image_finish_read(&image,nullptr,rgb.data(),3840,nullptr)){const std::string reason=image.message;png_image_free(&image);throw std::runtime_error("libpng decode: "+reason);}
 png_image_free(&image);return rgb;
}
}
