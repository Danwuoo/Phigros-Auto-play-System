#include "rhythm/audio_capture_win.hpp"
#include <array>
#include <cmath>
#include <cstring>
#include <iostream>
#include <limits>
#include <stdexcept>

namespace {
void require(bool value,const char* label) {if(!value) throw std::runtime_error(label);}
template<class F> void rejects(F fn) {
    bool caught=false;try{fn();}catch(const std::exception&){caught=true;}
    require(caught,"expected rejection");
}
}
int main() {
    using namespace rhythm;
    try {
        const std::array<std::int16_t,4> stereo{32767,-32768,16384,16384};
        const auto bytes=std::as_bytes(std::span(stereo));
        const auto mono=decode_capture_mono(bytes,2,2,CapturePcmEncoding::signed16,false);
        require(mono.size()==2 && std::abs(mono[0]+1.0F/65536)<1e-7F && mono[1]==.5F,"signed16 mean");
        const std::array<float,4> floats{.5F,.25F,-1.0F,1.0F};
        const auto decoded=decode_capture_mono(std::as_bytes(std::span(floats)),2,2,CapturePcmEncoding::float32,false);
        require(decoded[0]==.375F && decoded[1]==0,"float mean and phase cancellation");
        const auto silent=decode_capture_mono({},100,2,CapturePcmEncoding::float32,true);
        require(silent.size()==100 && silent.back()==0,"silent flag ignores buffer");
        rejects([&]{decode_capture_mono(bytes,3,2,CapturePcmEncoding::signed16,false);});
        rejects([&]{decode_capture_mono({},192001,2,CapturePcmEncoding::float32,true);});
        rejects([&]{decode_capture_mono({},1,9,CapturePcmEncoding::float32,true);});
        const std::array<float,1> invalid{std::numeric_limits<float>::quiet_NaN()};
        rejects([&]{decode_capture_mono(std::as_bytes(std::span(invalid)),1,1,CapturePcmEncoding::float32,false);});
        AudioCaptureConfig config; config.max_duration_ns=181'000'000'000;
        rejects([&]{WasapiLoopbackCapture capture(config);});
        WasapiLoopbackCapture never_started;
        require(!never_started.status().started && never_started.drain().empty(),"construction opens no audio endpoint");
        std::cout<<"9 offline capture conversion/lifecycle checks passed; no endpoint opened\n";return 0;
    } catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}
}
