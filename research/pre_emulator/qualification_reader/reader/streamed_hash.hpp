#pragma once
#include <windows.h>
#include <bcrypt.h>
#include <array>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <stdexcept>
namespace pas::preflight_graph {
inline std::string streamed_sha256(const std::filesystem::path& path,std::uint64_t cap){
 if(!std::filesystem::is_regular_file(path)||std::filesystem::file_size(path)>cap)throw std::runtime_error("streamed hash file bound/type");
 BCRYPT_ALG_HANDLE alg{};BCRYPT_HASH_HANDLE hash{};
 const auto cleanup=[&]{if(hash)BCryptDestroyHash(hash);if(alg)BCryptCloseAlgorithmProvider(alg,0);};
 try{
  if(BCryptOpenAlgorithmProvider(&alg,BCRYPT_SHA256_ALGORITHM,nullptr,0)<0||BCryptCreateHash(alg,&hash,nullptr,0,nullptr,0,0)<0)throw std::runtime_error("streamed SHA provider");
  std::ifstream file(path,std::ios::binary);if(!file)throw std::runtime_error("streamed hash open");
  std::array<unsigned char,65536> buffer{};std::uint64_t total=0;
  while(file){file.read(reinterpret_cast<char*>(buffer.data()),buffer.size());const auto n=static_cast<std::uint64_t>(file.gcount());
   if(n>cap-total)throw std::runtime_error("streamed hash growing file");total+=n;
   if(n&&BCryptHashData(hash,buffer.data(),static_cast<ULONG>(n),0)<0)throw std::runtime_error("streamed hash update");}
  if(!file.eof()||total!=std::filesystem::file_size(path))throw std::runtime_error("streamed hash unstable read");
  std::array<unsigned char,32> digest{};if(BCryptFinishHash(hash,digest.data(),digest.size(),0)<0)throw std::runtime_error("streamed hash final");
  std::ostringstream out;out<<std::hex<<std::setfill('0');for(const auto c:digest)out<<std::setw(2)<<static_cast<int>(c);cleanup();return out.str();
 }catch(...){cleanup();throw;}
}
}
