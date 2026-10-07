#pragma once
#include <array>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <limits>
#include <stdexcept>
#include <string>
#include <zlib.h>

namespace pas {
// One canonical gzip member, fixed 64 KiB input/output buffers, and one bounded
// row. EOF is valid only after inflate verifies the complete CRC/size trailer.
class LosslessEventReader final {
 std::ifstream source_;
 std::array<char,65536> input_{},output_{};
 z_stream stream_{};
 std::size_t at_=0,size_=0;
 std::uint64_t decoded_=0,cap_,physical_=0,read_=0;
 bool compressed_=false,initialized_=false,eof_=false,input_finished_=false;

 void fill(){
  at_=size_=0;
  if(eof_)return;
  if(!compressed_){
   source_.read(output_.data(),static_cast<std::streamsize>(output_.size()));
   size_=static_cast<std::size_t>(source_.gcount());
   if(!source_.eof()&&!source_)throw std::runtime_error("plain event read error");
   if(size_==0)eof_=true;
  }else{
   stream_.next_out=reinterpret_cast<Bytef*>(output_.data());
   stream_.avail_out=static_cast<uInt>(output_.size());
   while(stream_.avail_out==output_.size()){
    if(stream_.avail_in==0&&!input_finished_){
     source_.read(input_.data(),static_cast<std::streamsize>(input_.size()));
     const auto n=static_cast<std::uint64_t>(source_.gcount());
     if(!source_.eof()&&!source_)throw std::runtime_error("gzip event read error");
     if(n>physical_-read_)throw std::runtime_error("gzip event physical source changed");
     read_+=n;
     input_finished_=n==0;
     stream_.next_in=reinterpret_cast<Bytef*>(input_.data());
     stream_.avail_in=static_cast<uInt>(n);
    }
    const auto before_in=stream_.avail_in,before_out=stream_.avail_out;
    const int result=inflate(&stream_,Z_NO_FLUSH);
    if(result==Z_STREAM_END){
     if(stream_.avail_in!=0||read_!=physical_||source_.peek()!=std::char_traits<char>::eof())
      throw std::runtime_error("gzip event trailing data or extra member");
     if(source_.bad())throw std::runtime_error("gzip event final read error");
     eof_=true;
     break;
    }
    if(result!=Z_OK)throw std::runtime_error("gzip event CRC/format/truncation error");
    if(stream_.avail_in==before_in&&stream_.avail_out==before_out)
     throw std::runtime_error("gzip event inflate made no progress");
   }
   size_=output_.size()-stream_.avail_out;
  }
  if(size_>cap_-decoded_)throw std::runtime_error("event decoded byte bound");
  decoded_+=size_;
 }
 int byte(){
  if(at_==size_){fill();if(size_==0)return -1;}
  return static_cast<unsigned char>(output_[at_++]);
 }
public:
 explicit LosslessEventReader(const std::filesystem::path&path,std::uint64_t decoded_cap=16ull*1024*1024):cap_(decoded_cap){
  if(!std::filesystem::is_regular_file(path)||decoded_cap>std::numeric_limits<std::uint64_t>::max()-65536)
   throw std::runtime_error("event physical segment bound");
  physical_=std::filesystem::file_size(path);
  if(physical_>decoded_cap+65536)throw std::runtime_error("event physical segment bound");
  source_.open(path,std::ios::binary);
  if(!source_)throw std::runtime_error("event source open");
  compressed_=path.extension()==".gz";
  if(compressed_){
   std::array<unsigned char,3> magic{};
   source_.read(reinterpret_cast<char*>(magic.data()),static_cast<std::streamsize>(magic.size()));
   if(magic!=std::array<unsigned char,3>{0x1f,0x8b,8})throw std::runtime_error("gzip event magic");
   source_.clear();source_.seekg(0);
   if(!source_||inflateInit2(&stream_,MAX_WBITS+16)!=Z_OK)throw std::runtime_error("gzip event inflate initialization");
   initialized_=true;
  }
 }
 ~LosslessEventReader(){if(initialized_)inflateEnd(&stream_);}
 LosslessEventReader(const LosslessEventReader&)=delete;
 bool next(std::string&row){row.clear();for(;;){const int c=byte();if(c<0){if(!row.empty())throw std::runtime_error("event truncated final row");return false;}if(c=='\n')return true;if(row.size()>=1024*1024)throw std::runtime_error("event row byte bound");row.push_back(static_cast<char>(c));}}
 std::uint64_t decoded_bytes()const{return decoded_;}
};
}
