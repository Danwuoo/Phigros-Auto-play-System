#include "rhythm/audio_capture_win.hpp"
#define NOMINMAX
#include <windows.h>
#include <mmdeviceapi.h>
#include <audioclient.h>
#include <mmreg.h>
#include <ksmedia.h>
#include <wrl/client.h>
#include <atomic>
#include <cmath>
#include <cstring>
#include <deque>
#include <limits>
#include <mutex>
#include <stdexcept>
#include <thread>

namespace rhythm {
using Microsoft::WRL::ComPtr;
namespace {
void check(HRESULT value,const char* operation) {
    if(FAILED(value)) throw std::runtime_error(std::string(operation)+" HRESULT "+std::to_string(static_cast<unsigned long>(value)));
}
std::string utf8(const wchar_t* value) {
    if(!value) return {};
    const int size=WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,value,-1,nullptr,0,nullptr,nullptr);
    if(size<=0) throw std::runtime_error("invalid endpoint identity");
    std::string out(static_cast<std::size_t>(size),'\0');
    if(!WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,value,-1,out.data(),size,nullptr,nullptr))
        throw std::runtime_error("endpoint identity conversion failed");
    out.pop_back(); return out;
}
}
Ns audio_host_qpc_ns() {
    LARGE_INTEGER counter{},frequency{};
    if(!QueryPerformanceCounter(&counter) || !QueryPerformanceFrequency(&frequency) ||
       counter.QuadPart<0 || frequency.QuadPart<=0) throw std::runtime_error("QPC unavailable");
    const auto seconds=counter.QuadPart/frequency.QuadPart, remainder=counter.QuadPart%frequency.QuadPart;
    if(seconds>std::numeric_limits<Ns>::max()/1'000'000'000)
        throw std::overflow_error("QPC overflow");
    // Same absolute QPC origin and units as pas::HostClock.
    return seconds*1'000'000'000+static_cast<Ns>(static_cast<long double>(remainder)*1'000'000'000/frequency.QuadPart);
}
std::vector<float> decode_capture_mono(std::span<const std::byte> bytes,
    std::uint32_t frames,unsigned channels,CapturePcmEncoding encoding,bool silent) {
    if(!channels || channels>8 || frames>192000 ||
       (encoding!=CapturePcmEncoding::signed16 && encoding!=CapturePcmEncoding::float32))
        throw std::invalid_argument("unsupported capture packet dimensions/encoding");
    const std::size_t sample_bytes=encoding==CapturePcmEncoding::signed16 ? 2 : 4;
    const auto expected=static_cast<std::size_t>(frames)*channels*sample_bytes;
    if(!silent && bytes.size()!=expected) throw std::invalid_argument("capture packet byte count");
    std::vector<float> mono(frames,0);
    if(silent) return mono;
    for(std::size_t frame=0;frame<frames;++frame) {
        double sum=0;
        for(unsigned c=0;c<channels;++c) {
            const auto* source=bytes.data()+(frame*channels+c)*sample_bytes;
            double value=0;
            if(encoding==CapturePcmEncoding::signed16) {
                std::int16_t scalar; std::memcpy(&scalar,source,2); value=static_cast<double>(scalar)/32768;
            } else {
                float scalar; std::memcpy(&scalar,source,4); value=scalar;
            }
            if(!std::isfinite(value) || std::abs(value)>8)
                throw std::invalid_argument("nonfinite/out-of-budget audio PCM");
            sum+=value;
        }
        mono[frame]=static_cast<float>(sum/channels);
    }
    return mono;
}
struct WasapiLoopbackCapture::Impl {
    AudioCaptureConfig config;
    mutable std::mutex mutex;
    AudioCaptureStatus state;
    std::deque<CapturedAudioPacket> packets;
    std::atomic<bool> stop_requested{false};
    std::thread worker;
    explicit Impl(AudioCaptureConfig value):config(value) {
        if(config.max_duration_ns<=0 || config.max_duration_ns>180'000'000'000 ||
           !config.max_queued_packets || config.max_queued_packets>64)
            throw std::invalid_argument("capture duration/queue limit");
    }
    void run() {
        const auto com=CoInitializeEx(nullptr,COINIT_MULTITHREADED);
        ComPtr<IMMDeviceEnumerator> enumerator;
        ComPtr<IMMDevice> endpoint;
        ComPtr<IAudioClient> client;
        ComPtr<IAudioCaptureClient> capture;
        WAVEFORMATEX* format=nullptr;
        bool stream_started=false;
        try {
            check(com,"COM");
            check(CoCreateInstance(__uuidof(MMDeviceEnumerator),nullptr,CLSCTX_ALL,IID_PPV_ARGS(&enumerator)),"enumerator");
            check(enumerator->GetDefaultAudioEndpoint(eRender,eMultimedia,&endpoint),"default render endpoint");
            LPWSTR endpoint_id=nullptr;
            check(endpoint->GetId(&endpoint_id),"endpoint identity");
            std::string identity;
            try {identity=utf8(endpoint_id);} catch(...) {CoTaskMemFree(endpoint_id);throw;}
            CoTaskMemFree(endpoint_id);
            check(endpoint->Activate(__uuidof(IAudioClient),CLSCTX_ALL,nullptr,reinterpret_cast<void**>(client.GetAddressOf())),"activate");
            check(client->GetMixFormat(&format),"mix format");
            if(!format->nChannels || format->nChannels>8 || format->nSamplesPerSec<8000 ||
               format->nSamplesPerSec>192000 || format->cbSize>4096)
                throw std::runtime_error("unsupported render mix dimensions");
            WORD tag=format->wFormatTag;
            if(tag==WAVE_FORMAT_EXTENSIBLE) {
                if(format->cbSize<22) throw std::runtime_error("truncated extensible format");
                const auto* ext=reinterpret_cast<const WAVEFORMATEXTENSIBLE*>(format);
                if(IsEqualGUID(ext->SubFormat,KSDATAFORMAT_SUBTYPE_IEEE_FLOAT)) tag=WAVE_FORMAT_IEEE_FLOAT;
                else if(IsEqualGUID(ext->SubFormat,KSDATAFORMAT_SUBTYPE_PCM)) tag=WAVE_FORMAT_PCM;
                else throw std::runtime_error("unsupported extensible encoding");
                if(ext->Samples.wValidBitsPerSample!=format->wBitsPerSample)
                    throw std::runtime_error("unsupported padded sample encoding");
            }
            CapturePcmEncoding encoding;
            if(tag==WAVE_FORMAT_IEEE_FLOAT && format->wBitsPerSample==32) encoding=CapturePcmEncoding::float32;
            else if(tag==WAVE_FORMAT_PCM && format->wBitsPerSample==16) encoding=CapturePcmEncoding::signed16;
            else throw std::runtime_error("only float32 and signed16 capture supported");
            if(format->nBlockAlign!=format->nChannels*(format->wBitsPerSample/8))
                throw std::runtime_error("invalid mix block alignment");
            check(client->Initialize(AUDCLNT_SHAREMODE_SHARED,AUDCLNT_STREAMFLAGS_LOOPBACK,1'000'000,0,format,nullptr),"initialize loopback");
            check(client->GetService(IID_PPV_ARGS(&capture)),"capture service");
            {
                std::lock_guard lock(mutex);
                state.endpoint_id=identity; state.sample_rate=format->nSamplesPerSec;
                state.channels=format->nChannels; state.bits_per_sample=format->wBitsPerSample;
                state.running=true;
            }
            const auto start=audio_host_qpc_ns();
            check(client->Start(),"start"); stream_started=true;
            std::uint64_t sequence=0;
            while(!stop_requested.load() && audio_host_qpc_ns()-start<config.max_duration_ns) {
                UINT32 available=0;
                check(capture->GetNextPacketSize(&available),"packet size");
                if(!available) {Sleep(2);continue;}
                BYTE* data=nullptr; UINT32 frames=0; DWORD flags=0; UINT64 position=0,qpc=0;
                check(capture->GetBuffer(&data,&frames,&flags,&position,&qpc),"get buffer");
                if(!frames) continue;
                CapturedAudioPacket packet;
                try {
                    packet.sequence=++sequence; packet.flags=flags;
                    packet.sample_rate=format->nSamplesPerSec; packet.device_position_frames=position;
                    packet.qpc_position_100ns=qpc; packet.received_host_ns=audio_host_qpc_ns();
                    packet.silent=(flags&AUDCLNT_BUFFERFLAGS_SILENT)!=0;
                    packet.discontinuity=(flags&AUDCLNT_BUFFERFLAGS_DATA_DISCONTINUITY)!=0;
                    packet.timestamp_error=(flags&AUDCLNT_BUFFERFLAGS_TIMESTAMP_ERROR)!=0;
                    if(qpc>static_cast<std::uint64_t>(std::numeric_limits<Ns>::max()/100))
                        packet.timestamp_error=true;
                    packet.first_host_ns=packet.timestamp_error ? 0 : static_cast<Ns>(qpc)*100;
                    if(frames>192000) throw std::runtime_error("capture packet frame budget");
                    const auto bytes=static_cast<std::size_t>(frames)*format->nBlockAlign;
                    if(!data && !packet.silent) throw std::runtime_error("missing non-silent buffer");
                    const auto view=data ? std::span<const std::byte>(reinterpret_cast<const std::byte*>(data),bytes) : std::span<const std::byte>{};
                    packet.mono=decode_capture_mono(view,frames,format->nChannels,encoding,packet.silent);
                } catch(...) {capture->ReleaseBuffer(frames);throw;}
                check(capture->ReleaseBuffer(frames),"release buffer");
                {
                    std::lock_guard lock(mutex);
                    ++state.packets; state.frames+=frames; state.silent_packets+=packet.silent;
                    state.discontinuities+=packet.discontinuity; state.timestamp_errors+=packet.timestamp_error;
                    if(packets.size()>=config.max_queued_packets) {
                        state.dropped_packets+=packets.size(); packets.clear();
                        ++state.generation; packet.discontinuity=true;
                    }
                    packet.generation=state.generation;
                    packets.push_back(std::move(packet));
                }
            }
            check(client->Stop(),"stop"); stream_started=false;
            std::lock_guard lock(mutex); state.finished=true;
        } catch(const std::exception& error) {
            std::lock_guard lock(mutex); state.failed=true; state.error=error.what();
        }
        if(stream_started) client->Stop();
        if(format) CoTaskMemFree(format);
        capture.Reset(); client.Reset(); endpoint.Reset(); enumerator.Reset();
        if(SUCCEEDED(com)) CoUninitialize();
        std::lock_guard lock(mutex); state.running=false;
    }
};
WasapiLoopbackCapture::WasapiLoopbackCapture(AudioCaptureConfig config):impl_(std::make_unique<Impl>(config)) {}
WasapiLoopbackCapture::~WasapiLoopbackCapture() {stop();}
void WasapiLoopbackCapture::start() {
    std::lock_guard lock(impl_->mutex);
    if(impl_->state.started) throw std::logic_error("capture cannot auto-restart");
    impl_->state.started=true;
    try {impl_->worker=std::thread([this] {impl_->run();});}
    catch(...) {impl_->state.failed=true;impl_->state.error="capture thread creation failed";throw;}
}
void WasapiLoopbackCapture::stop() {
    impl_->stop_requested.store(true);
    if(impl_->worker.joinable()) impl_->worker.join();
}
std::vector<CapturedAudioPacket> WasapiLoopbackCapture::drain() {
    std::lock_guard lock(impl_->mutex);
    std::vector<CapturedAudioPacket> result; result.reserve(impl_->packets.size());
    while(!impl_->packets.empty()) {result.push_back(std::move(impl_->packets.front()));impl_->packets.pop_front();}
    return result;
}
AudioCaptureStatus WasapiLoopbackCapture::status() const {
    std::lock_guard lock(impl_->mutex); return impl_->state;
}
} // namespace rhythm
