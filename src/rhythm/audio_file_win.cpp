#include "rhythm/audio_file_win.hpp"
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <bcrypt.h>
#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <span>
#include <stdexcept>

namespace rhythm {
namespace {
constexpr std::uint64_t max_file_bytes=512ULL*1024*1024;
constexpr std::uint32_t decode_chunk_frames=192000;
void checked(bool ok,const char* message) {if(!ok)throw std::runtime_error(message);}
struct File {
    HANDLE handle=INVALID_HANDLE_VALUE;
    std::uint64_t length=0,position=0;
    explicit File(const std::filesystem::path& path) {
        // Deny concurrent write/delete access; hash and decode use this same
        // handle and file identity. No second path-based open is performed.
        handle=CreateFileW(path.c_str(),GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,
                          FILE_ATTRIBUTE_NORMAL|FILE_FLAG_SEQUENTIAL_SCAN,nullptr);
        checked(handle!=INVALID_HANDLE_VALUE,"Cannot open WAV file for stable read");
        LARGE_INTEGER size{};
        if(!GetFileSizeEx(handle,&size)||size.QuadPart<12||static_cast<std::uint64_t>(size.QuadPart)>max_file_bytes) {
            CloseHandle(handle);handle=INVALID_HANDLE_VALUE;
            throw std::runtime_error("WAV file size is invalid or exceeds 512MiB");
        }
        length=static_cast<std::uint64_t>(size.QuadPart);
    }
    ~File() {if(handle!=INVALID_HANDLE_VALUE)CloseHandle(handle);}
    File(const File&)=delete;File& operator=(const File&)=delete;
    void seek(std::uint64_t offset) {
        checked(offset<=length,"WAV seek outside file bounds");
        LARGE_INTEGER distance{};distance.QuadPart=static_cast<LONGLONG>(offset);
        checked(SetFilePointerEx(handle,distance,nullptr,FILE_BEGIN)!=0,"WAV seek failed");position=offset;
    }
    void read(std::span<unsigned char> bytes) {
        checked(bytes.size()<=length-position,"Truncated WAV read");
        std::size_t offset=0;
        while(offset<bytes.size()) {
            const auto wanted=static_cast<DWORD>(std::min<std::size_t>(bytes.size()-offset,1024*1024));
            DWORD received=0;
            checked(ReadFile(handle,bytes.data()+offset,wanted,&received,nullptr)!=0&&received>0,"WAV read failed or truncated");
            offset+=received;position+=received;
        }
    }
};
struct Algorithm {BCRYPT_ALG_HANDLE handle=nullptr;~Algorithm(){if(handle)BCryptCloseAlgorithmProvider(handle,0);}};
struct Hash {BCRYPT_HASH_HANDLE handle=nullptr;~Hash(){if(handle)BCryptDestroyHash(handle);}};
std::string hash_file(File& file) {
    Algorithm algorithm;
    checked(BCryptOpenAlgorithmProvider(&algorithm.handle,BCRYPT_SHA256_ALGORITHM,nullptr,0)>=0,"SHA256 provider unavailable");
    DWORD size=0,received=0,digest_bytes=0;
    checked(BCryptGetProperty(algorithm.handle,BCRYPT_OBJECT_LENGTH,reinterpret_cast<PUCHAR>(&size),sizeof(size),&received,0)>=0&&
            received==sizeof(size)&&size>0&&size<=1024*1024,"Invalid SHA256 object size");
    checked(BCryptGetProperty(algorithm.handle,BCRYPT_HASH_LENGTH,reinterpret_cast<PUCHAR>(&digest_bytes),sizeof(digest_bytes),&received,0)>=0&&
            received==sizeof(digest_bytes)&&digest_bytes==32,"Invalid SHA256 digest size");
    std::vector<unsigned char> storage(size);
    Hash hash;
    checked(BCryptCreateHash(algorithm.handle,&hash.handle,storage.data(),size,nullptr,0,0)>=0,"SHA256 initialization failed");
    std::array<unsigned char,65536> buffer{};
    file.seek(0);
    while(file.position<file.length) {
        const auto count=static_cast<std::size_t>(std::min<std::uint64_t>(buffer.size(),file.length-file.position));
        file.read(std::span(buffer).first(count));
        checked(BCryptHashData(hash.handle,buffer.data(),static_cast<ULONG>(count),0)>=0,"SHA256 streaming update failed");
    }
    std::array<unsigned char,32> digest{};
    checked(BCryptFinishHash(hash.handle,digest.data(),static_cast<ULONG>(digest.size()),0)>=0,"SHA256 finish failed");
    constexpr char hex[]="0123456789abcdef";
    std::string result;result.reserve(64);
    for(auto byte:digest) {result.push_back(hex[byte>>4]);result.push_back(hex[byte&15]);}
    return result;
}
std::uint16_t u16(const unsigned char* p) {return static_cast<std::uint16_t>(p[0]|(static_cast<unsigned>(p[1])<<8));}
std::uint32_t u32(const unsigned char* p) {
    return static_cast<std::uint32_t>(p[0])|(static_cast<std::uint32_t>(p[1])<<8)|
           (static_cast<std::uint32_t>(p[2])<<16)|(static_cast<std::uint32_t>(p[3])<<24);
}
bool tag(const unsigned char* bytes,const char* text) {return std::memcmp(bytes,text,4)==0;}
bool sha_text(const std::string& text) {return text.size()==64&&std::all_of(text.begin(),text.end(),[](char c){return(c>='0'&&c<='9')||(c>='a'&&c<='f');});}
} // namespace

LoadedMonoWav load_audio_wav(const std::filesystem::path& path,std::optional<std::string> expected_sha256) {
    if(expected_sha256&&!sha_text(*expected_sha256))throw std::invalid_argument("Expected WAV SHA256 must be 64 lowercase hexadecimal characters");
    File file(path);
    LoadedMonoWav result;result.sha256=hash_file(file);
    if(expected_sha256&&result.sha256!=*expected_sha256)throw std::runtime_error("WAV SHA256 mismatch");
    file.seek(0);
    std::array<unsigned char,12> header{};file.read(header);
    checked(tag(header.data(),"RIFF")&&tag(header.data()+8,"WAVE"),"Only little-endian RIFF/WAVE is supported");
    checked(static_cast<std::uint64_t>(u32(header.data()+4))+8==file.length,"RIFF declared length does not match complete file");
    bool have_format=false,have_data=false;
    unsigned encoding=0,block_align=0;
    while(file.position<file.length) {
        checked(file.length-file.position>=8,"Truncated WAV chunk header");
        std::array<unsigned char,8> chunk{};file.read(chunk);
        const auto count=static_cast<std::uint64_t>(u32(chunk.data()+4));
        const auto payload=file.position;
        checked(count<=file.length-payload,"WAV chunk exceeds RIFF bounds");
        const auto end=payload+count;
        const auto padded=end+(count&1);
        checked(padded<=file.length,"WAV odd chunk is missing padding");
        if(tag(chunk.data(),"fmt ")) {
            checked(!have_format,"Duplicate WAV format chunk");
            checked(count==16||count==18,"Only canonical PCM/float WAV format chunks are supported");
            std::array<unsigned char,18> format{};file.read(std::span(format).first(static_cast<std::size_t>(count)));
            encoding=u16(format.data());result.channels=u16(format.data()+2);result.sample_rate=u32(format.data()+4);
            block_align=u16(format.data()+12);result.bits=u16(format.data()+14);
            checked((encoding==1&&result.bits==16)||(encoding==3&&result.bits==32),"Only PCM16 or IEEE float32 WAV is supported");
            checked(result.channels>=1&&result.channels<=8&&result.sample_rate>=8000&&result.sample_rate<=192000,"WAV channel or sample-rate limit exceeded");
            checked(block_align==result.channels*(result.bits/8)&&u32(format.data()+8)==result.sample_rate*block_align,"WAV block alignment or byte rate is inconsistent");
            checked(count==16||u16(format.data()+16)==0,"Unsupported nonempty WAV format extension");
            have_format=true;
        } else if(tag(chunk.data(),"data")) {
            checked(have_format&&!have_data,"WAV needs exactly one data chunk after its format");
            checked(count>0&&count%block_align==0,"WAV PCM is empty or has a partial sample frame");
            const auto frames=count/block_align;
            checked(frames<=static_cast<std::uint64_t>(result.sample_rate)*600,"WAV duration exceeds 600 seconds");
            result.frames=static_cast<std::size_t>(frames);result.mono.reserve(result.frames);
            std::vector<unsigned char> pcm(static_cast<std::size_t>(decode_chunk_frames)*block_align);
            std::uint64_t remaining=frames;
            while(remaining) {
                const auto batch=static_cast<std::size_t>(std::min<std::uint64_t>(remaining,decode_chunk_frames));
                file.read(std::span(pcm).first(batch*block_align));
                for(std::size_t frame=0;frame<batch;++frame) {
                    double sum=0;
                    for(unsigned channel=0;channel<result.channels;++channel) {
                        const auto* source=pcm.data()+(frame*result.channels+channel)*(result.bits/8);
                        double value=0;
                        if(encoding==1) {
                            const auto scalar=u16(source);
                            value=static_cast<double>(scalar<32768 ? scalar : static_cast<int>(scalar)-65536)/32768;
                        } else value=std::bit_cast<float>(u32(source));
                        // Match decode_capture_mono's finite/amplitude budget
                        // and double-precision arithmetic channel mean.
                        checked(std::isfinite(value)&&std::abs(value)<=8,"WAV contains nonfinite or out-of-budget PCM");
                        sum+=value;
                    }
                    result.mono.push_back(static_cast<float>(sum/result.channels));
                }
                remaining-=batch;
            }
            have_data=true;
        }
        // Unknown chunks are bounded and skipped, not executed or decoded.
        file.seek(padded);
    }
    checked(have_format&&have_data&&result.mono.size()==result.frames,"WAV is missing format or decoded PCM");
    return result;
}
} // namespace rhythm
