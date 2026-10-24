#include "rhythm/audio_file_win.hpp"
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <algorithm>
#include <bit>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>

namespace {
using Bytes=std::vector<unsigned char>;
int checks=0;
void require(bool ok,const char* text) {if(!ok)throw std::runtime_error(text);++checks;}
template<class F> void rejects(F function,const char* text) {
    bool rejected=false;try{function();}catch(const std::exception&){rejected=true;}require(rejected,text);
}
void text(Bytes& b,const char* s) {b.insert(b.end(),s,s+4);}
void u16(Bytes& b,std::uint16_t x) {b.push_back(static_cast<unsigned char>(x));b.push_back(static_cast<unsigned char>(x>>8));}
void u32(Bytes& b,std::uint32_t x) {for(unsigned shift:{0,8,16,24})b.push_back(static_cast<unsigned char>(x>>shift));}
void patch32(Bytes& b,std::size_t offset,std::uint32_t value) {for(unsigned shift:{0,8,16,24})b.at(offset+shift/8)=static_cast<unsigned char>(value>>shift);}
Bytes chunk(const char* tag,const Bytes& payload,bool padding=true) {
    Bytes b;text(b,tag);u32(b,static_cast<std::uint32_t>(payload.size()));b.insert(b.end(),payload.begin(),payload.end());
    if(padding&&(payload.size()&1))b.push_back(0);return b;
}
Bytes format(unsigned channels=2,unsigned rate=8000,unsigned tag=1,unsigned bits=16) {
    Bytes b;u16(b,static_cast<std::uint16_t>(tag));u16(b,static_cast<std::uint16_t>(channels));u32(b,rate);
    u32(b,rate*channels*(bits/8));u16(b,static_cast<std::uint16_t>(channels*(bits/8)));u16(b,static_cast<std::uint16_t>(bits));return b;
}
Bytes riff(const std::vector<Bytes>& chunks) {
    Bytes b;text(b,"RIFF");u32(b,0);text(b,"WAVE");for(const auto& c:chunks)b.insert(b.end(),c.begin(),c.end());
    patch32(b,4,static_cast<std::uint32_t>(b.size()-8));return b;
}
Bytes wav(const Bytes& pcm,unsigned channels=2,unsigned rate=8000,unsigned tag=1,unsigned bits=16) {return riff({chunk("fmt ",format(channels,rate,tag,bits)),chunk("data",pcm)});}
struct Scratch {
    std::filesystem::path root;
    std::vector<std::filesystem::path> files;
    Scratch() {
        root=std::filesystem::current_path()/("audio-file-tests-"+std::to_string(GetCurrentProcessId())+"-"+
             std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
        if(!std::filesystem::create_directory(root))throw std::runtime_error("Could not exclusively create test scratch directory");
    }
    ~Scratch() {
        std::error_code error;for(const auto& p:files)std::filesystem::remove(p,error);std::filesystem::remove(root,error);
    }
    std::filesystem::path save(const Bytes& b) {
        const auto p=root/(std::to_string(files.size())+".wav");files.push_back(p);
        std::ofstream out(p,std::ios::binary);out.write(reinterpret_cast<const char*>(b.data()),static_cast<std::streamsize>(b.size()));
        if(!out)throw std::runtime_error("Test fixture write failed");return p;
    }
};
}
int main() {
    using namespace rhythm;
    try {
        Scratch scratch;
        Bytes pcm;for(auto sample:{32767,-32768,16384,16384})u16(pcm,static_cast<std::uint16_t>(sample));
        const auto good=scratch.save(wav(pcm));const auto loaded=load_audio_wav(good);
        require(loaded.frames==2&&loaded.mono.size()==2&&loaded.channels==2&&loaded.bits==16&&loaded.sample_rate==8000,"PCM16 metadata");
        require(std::abs(loaded.mono[0]+1.0F/65536)<1e-8F&&loaded.mono[1]==.5F,"PCM16 arithmetic channel mean and signed normalization");
        require(loaded.sha256=="0283c5d1b5409641f337211e50187b851b63f7bb7079ce6bef2a562454153cee","independent Python SHA256 fixture");
        require(load_audio_wav(good,loaded.sha256).mono==loaded.mono,"expected actual-file SHA accepted");
        rejects([&]{load_audio_wav(good,std::string(64,'0'));},"wrong SHA rejected");
        for(const auto& bad:{std::string{},std::string(63,'a'),std::string(64,'G'),std::string(64,'A')})
            rejects([&]{load_audio_wav(good,bad);},"malformed expected SHA rejected");
        const auto unicode=scratch.root/L"\u97f3\u8a0a.wav";std::filesystem::copy_file(good,unicode);scratch.files.push_back(unicode);
        require(load_audio_wav(unicode).sha256==loaded.sha256,"Unicode path stable read");
        Bytes floats;for(float value:{.5F,.25F,-1.F,1.F,8.F,-8.F})u32(floats,std::bit_cast<std::uint32_t>(value));
        auto f=load_audio_wav(scratch.save(wav(floats,2,48000,3,32)));
        require(f.bits==32&&f.frames==3&&f.sample_rate==48000,"float WAV metadata");
        require(f.mono[0]==.375F&&f.mono[1]==0&&f.mono[2]==0,"float arithmetic mean preserves cancellation");
        Bytes eight;for(int value:{-32768,32767,0,0,16384,16384,-16384,-16384})u16(eight,static_cast<std::uint16_t>(value));
        require(std::abs(load_audio_wav(scratch.save(wav(eight,8))).mono[0]+1.F/(32768*8))<1e-9,"eight channel mean");
        const auto silent=load_audio_wav(scratch.save(wav(Bytes(8,0),1)));
        require(silent.frames==4&&std::all_of(silent.mono.begin(),silent.mono.end(),[](float x){return x==0;}),"silent PCM is present decoded frames");
        Bytes large(192001*2,0);large[large.size()-2]=255;large.back()=127;
        const auto batch=load_audio_wav(scratch.save(wav(large,1,192000)));
        require(batch.frames==192001&&batch.mono.front()==0&&batch.mono.back()==32767.F/32768,"decoder crosses bounded batch without losing sample");
        const auto extra=load_audio_wav(scratch.save(riff({chunk("JUNK",{42}),chunk("fmt ",format()),chunk("LIST",{1,2,3}),chunk("data",pcm)})));
        require(extra.mono==loaded.mono&&extra.sha256!=loaded.sha256,"odd padded unknown chunks accepted and included in SHA");
        auto fmt18=format();u16(fmt18,0);
        require(load_audio_wav(scratch.save(riff({chunk("fmt ",fmt18),chunk("data",pcm)}))).frames==2,"canonical zero extension accepted");
        for(const auto invalid:{std::numeric_limits<float>::quiet_NaN(),std::numeric_limits<float>::infinity(),-std::numeric_limits<float>::infinity(),8.01F}) {
            Bytes b;u32(b,std::bit_cast<std::uint32_t>(invalid));
            rejects([&]{load_audio_wav(scratch.save(wav(b,1,48000,3,32)));},"nonfinite or excessive float sample rejected");
        }
        for(auto tag_value:{0U,2U,65534U})rejects([&]{load_audio_wav(scratch.save(wav(pcm,2,8000,tag_value)));},"unsupported encoding rejected");
        for(auto channels:{0U,9U})rejects([&]{load_audio_wav(scratch.save(wav(pcm,channels)));},"invalid channels rejected");
        for(auto rate:{7999U,192001U})rejects([&]{load_audio_wav(scratch.save(wav(pcm,2,rate)));},"invalid sample rate rejected");
        rejects([&]{load_audio_wav(scratch.save(wav(pcm,2,8000,1,24)));},"unsupported PCM bits rejected");
        rejects([&]{load_audio_wav(scratch.save(wav(pcm,2,8000,3,16)));},"unsupported float bits rejected");
        auto corrupt=wav(pcm);corrupt[0]='X';rejects([&]{load_audio_wav(scratch.save(corrupt));},"RIFF signature rejected");
        corrupt=wav(pcm);corrupt[8]='X';rejects([&]{load_audio_wav(scratch.save(corrupt));},"WAVE signature rejected");
        corrupt=wav(pcm);patch32(corrupt,4,0);rejects([&]{load_audio_wav(scratch.save(corrupt));},"RIFF length mismatch rejected");
        corrupt=wav(pcm);corrupt.push_back(0);rejects([&]{load_audio_wav(scratch.save(corrupt));},"trailing bytes rejected");
        corrupt=wav(pcm);patch32(corrupt,40,0xfffffff0);rejects([&]{load_audio_wav(scratch.save(corrupt));},"oversize chunk rejected before decode");
        corrupt=wav(pcm);corrupt.pop_back();patch32(corrupt,4,static_cast<std::uint32_t>(corrupt.size()-8));rejects([&]{load_audio_wav(scratch.save(corrupt));},"truncated payload rejected");
        rejects([&]{load_audio_wav(scratch.save(riff({chunk("fmt ",format()),chunk("data",{0,0,0})})));},"partial PCM frame rejected");
        rejects([&]{load_audio_wav(scratch.save(riff({chunk("fmt ",format()),chunk("data",{})})));},"missing PCM differs from silence");
        rejects([&]{load_audio_wav(scratch.save(riff({chunk("data",pcm),chunk("fmt ",format())})));},"data before fmt rejected");
        rejects([&]{load_audio_wav(scratch.save(riff({chunk("fmt ",format()),chunk("fmt ",format()),chunk("data",pcm)})));},"duplicate fmt rejected");
        rejects([&]{load_audio_wav(scratch.save(riff({chunk("fmt ",format()),chunk("data",pcm),chunk("data",pcm)})));},"duplicate data rejected");
        rejects([&]{load_audio_wav(scratch.save(riff({chunk("fmt ",format())})));},"missing data rejected");
        rejects([&]{load_audio_wav(scratch.save(riff({chunk("JUNK",{42},false)})));},"missing odd padding rejected");
        auto alignment=format();alignment[12]=0;rejects([&]{load_audio_wav(scratch.save(riff({chunk("fmt ",alignment),chunk("data",pcm)})));},"wrong block align rejected");
        auto byte_rate=format();patch32(byte_rate,8,1);rejects([&]{load_audio_wav(scratch.save(riff({chunk("fmt ",byte_rate),chunk("data",pcm)})));},"wrong byte rate rejected");
        fmt18.back()=1;rejects([&]{load_audio_wav(scratch.save(riff({chunk("fmt ",fmt18),chunk("data",pcm)})));},"nonempty format extension rejected");
        auto oversized=scratch.save(wav(pcm));std::filesystem::resize_file(oversized,512ULL*1024*1024+1);
        rejects([&]{load_audio_wav(oversized);},"file over 512MiB rejected before hash or allocation");
        auto duration=wav(Bytes(8000*601*2,0),1,8000);const auto long_file=scratch.save(duration);duration.clear();
        rejects([&]{load_audio_wav(long_file);},"duration over 600 seconds rejected before allocation");
        rejects([&]{load_audio_wav(scratch.root/"missing.wav");},"missing file rejected");
        std::cout<<checks<<" offline WAV file/hash checks passed; no endpoints opened\n";return 0;
    } catch(const std::exception& e) {std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
}
