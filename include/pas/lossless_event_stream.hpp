#pragma once
#include <array>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>
#include <zlib.h>

namespace pas {
// Streaming reader with a fixed 64 KiB decode buffer, one <=1 MiB row and a
// per-segment decoded bound. CRC/truncation errors cannot become normal EOF.
class LosslessEventReader final {
 std::ifstream plain_;gzFile gzip_=nullptr;std::array<char,65536> buffer_{};
 std::size_t at_=0,size_=0;std::uint64_t decoded_=0,cap_;bool eof_=false;
 int byte(){
  if(at_==size_){if(eof_)return -1;int n=0;
   if(gzip_){n=gzread(gzip_,buffer_.data(),static_cast<unsigned>(buffer_.size()));int error=Z_OK;gzerror(gzip_,&error);if(n<0||(n==0&&error!=Z_OK&&error!=Z_STREAM_END))throw std::runtime_error("gzip event CRC/truncation error");}
   else{plain_.read(buffer_.data(),buffer_.size());n=static_cast<int>(plain_.gcount());if(!plain_.eof()&&!plain_)throw std::runtime_error("plain event read error");}
   if(n==0){eof_=true;return -1;}if(static_cast<std::uint64_t>(n)>cap_-decoded_)throw std::runtime_error("event decoded byte bound");decoded_+=n;at_=0;size_=n;
  }return static_cast<unsigned char>(buffer_[at_++]);
 }
public:
 explicit LosslessEventReader(const std::filesystem::path&path,std::uint64_t decoded_cap=16ull*1024*1024):cap_(decoded_cap){
  if(!std::filesystem::is_regular_file(path)||std::filesystem::file_size(path)>decoded_cap+65536)throw std::runtime_error("event physical segment bound");
  if(path.extension()==".gz"){
   std::ifstream magic(path,std::ios::binary);std::array<unsigned char,3> m{};magic.read(reinterpret_cast<char*>(m.data()),m.size());if(m!=std::array<unsigned char,3>{0x1f,0x8b,8})throw std::runtime_error("gzip event magic");
   gzip_=gzopen(path.string().c_str(),"rb");if(!gzip_)throw std::runtime_error("gzip event open");
  }else{plain_.open(path,std::ios::binary);if(!plain_)throw std::runtime_error("plain event open");}
 }
 ~LosslessEventReader(){if(gzip_)gzclose(gzip_);}
 LosslessEventReader(const LosslessEventReader&)=delete;
 bool next(std::string&row){row.clear();for(;;){const int c=byte();if(c<0){if(!row.empty())throw std::runtime_error("event truncated final row");return false;}if(c=='\n')return true;if(row.size()>=1024*1024)throw std::runtime_error("event row byte bound");row.push_back(static_cast<char>(c));}}
 std::uint64_t decoded_bytes()const{return decoded_;}
};
}
