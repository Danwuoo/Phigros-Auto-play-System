#include "pas/session_recording.hpp"
#include "pas/adb.hpp"
#include "pas/analysis.hpp"
#include <fstream>
#include <iostream>
#include <chrono>
#include <thread>
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <mfapi.h>
#include <mfidl.h>
#include <mfreadwrite.h>
#include <wrl/client.h>

std::vector<std::filesystem::path> prepare_recording_selection(const std::filesystem::path&,
    const std::filesystem::path&,const std::filesystem::path&);

namespace {
using namespace pas;
using json=nlohmann::json;
using Microsoft::WRL::ComPtr;
void check(HRESULT result,const char* operation){if(FAILED(result))throw std::runtime_error(std::string(operation)+": HRESULT "+std::to_string(result));}
std::filesystem::path safe_relative(const std::string& value) {
    const std::filesystem::path path(value);
    if(path.is_absolute()||path.has_root_name()||path.empty())throw std::invalid_argument("absolute/empty image path");
    for(const auto& part:path)if(part=="..")throw std::invalid_argument("escaping image path");
    return path;
}
json read_json(const std::filesystem::path& path) {
    if(std::filesystem::file_size(path)>2*1024*1024)throw std::invalid_argument("JSON capacity");
    std::ifstream file(path);return json::parse(file);
}
int bench(const std::filesystem::path& clips,const std::filesystem::path& output) {
    if(std::filesystem::exists(output))throw std::invalid_argument("bench output exists");
    std::ifstream index(clips/"index.jsonl");std::string row;std::vector<json> entries;
    while(std::getline(index,row)) {
        if(row.size()>64*1024||entries.size()>=64)throw std::invalid_argument("bench index capacity");
        const auto entry=json::parse(row);const auto path=clips/safe_relative(entry.at("path"));
        if(std::filesystem::file_size(path)!=2764800||sha256_file(path)!=entry.at("sha256").get<std::string>())
            throw std::invalid_argument("bench RGB SHA/bytes mismatch");
        entries.push_back(entry);
    }
    if(entries.empty())throw std::invalid_argument("empty benchmark");
    HostClock clock;FullRecordingOptions options;options.pre_roll_frames=0;
    SessionRecording recording(output,clock,1280,720,options);recording.start(1);
    auto due=std::chrono::steady_clock::now();std::uint64_t frames=0;
    for(int repeat=0;repeat<10;++repeat)for(const auto& entry:entries) {
        Frame frame;frame.width=1280;frame.height=720;frame.stride=3840;frame.source_rotation=1;
        frame.rgb.resize(2764800);std::ifstream file(clips/safe_relative(entry.at("path")),std::ios::binary);
        file.read(reinterpret_cast<char*>(frame.rgb.data()),frame.rgb.size());
        if(file.gcount()!=static_cast<std::streamsize>(frame.rgb.size()))throw std::runtime_error("bench RGB read");
        std::this_thread::sleep_until(due);due+=std::chrono::nanoseconds(16'666'667);
        frame.sequence=++frames;frame.capture_complete_ns=clock.now_ns();frame.pixels_ready_ns=frame.capture_complete_ns;
        recording.observe(frame);if(recording.faulted())throw std::runtime_error(recording.error());
    }
    recording.finish(1);recording.close();
    // Independent WIC decode, all outputs including repeated clips, exact bytes.
    std::ifstream result(output/"index.jsonl");std::uint64_t checked=0;
    while(std::getline(result,row)) {
        const auto entry=json::parse(row);const auto& original=entries.at(checked%entries.size());
        std::vector<std::uint8_t> rgb(2764800);std::ifstream file(clips/safe_relative(original.at("path")),std::ios::binary);
        file.read(reinterpret_cast<char*>(rgb.data()),rgb.size());
        if(load_diagnostic_png(output/safe_relative(entry.at("path"))).rgb!=rgb)
            throw std::runtime_error("lossless PNG round trip failed");
        ++checked;
    }
    auto report=recording.summary();report["exact_rgb_round_trips"]=checked;
    report["input_index_sha256"]=sha256_file(clips/"index.jsonl");report["real_input_backend_constructed"]=false;
    report["scope"]="same recorder; 60Hz producer; 27 recorded RGB scenarios repeated10; emulator idle concurrently; not capture+owner performance";
    std::ofstream file(output/"benchmark.json");file<<report.dump(2)<<'\n';
    std::cout<<report.dump(2)<<'\n';
    return recording.faulted()||checked!=frames?1:0;
}
class MfScope {
public:
    MfScope(){check(CoInitializeEx(nullptr,COINIT_MULTITHREADED),"COM startup");com_=true;check(MFStartup(MF_VERSION),"MF startup");mf_=true;}
    ~MfScope(){if(mf_)MFShutdown();if(com_)CoUninitialize();}
private:bool com_=false,mf_=false;
};
int video(const std::filesystem::path& root,const std::filesystem::path& output) {
    if(std::filesystem::exists(output)||std::filesystem::exists(output.string()+".json"))
        throw std::invalid_argument("video output exists");
    const auto summary=read_json(root/"summary.json");
    if(sha256_file(root/"index.jsonl")!=summary.at("index_sha256").get<std::string>()||
       std::filesystem::file_size(root/"index.jsonl")>64*1024*1024)throw std::invalid_argument("video index SHA/capacity");
    std::vector<json> entries;std::ifstream index(root/"index.jsonl");std::string row;Nanoseconds previous=0;
    while(std::getline(index,row)) {
        if(entries.size()>=36000||row.size()>64*1024)throw std::invalid_argument("video index capacity");
        auto entry=json::parse(row);const auto path=root/safe_relative(entry.at("path"));
        const auto time=entry.at("capture_complete_ns").get<Nanoseconds>();
        if(entry.at("ordinal")!=entries.size()||entry.at("width")!=1280||entry.at("height")!=720||
           (previous&&time<=previous)||std::filesystem::file_size(path)>4*1024*1024||
           sha256_file(path)!=entry.at("png_sha256").get<std::string>())throw std::invalid_argument("video PNG/index provenance");
        previous=time;entries.push_back(std::move(entry));
    }
    if(entries.empty())throw std::invalid_argument("empty recording video");
    MfScope scope;ComPtr<IMFSinkWriter> sink;ComPtr<IMFAttributes> attributes;
    check(MFCreateAttributes(&attributes,2),"attributes");
    check(attributes->SetUINT32(MF_READWRITE_ENABLE_HARDWARE_TRANSFORMS,TRUE),"hardware transform attribute");
    check(MFCreateSinkWriterFromURL(output.c_str(),nullptr,attributes.Get(),&sink),"MP4 sink");
    ComPtr<IMFMediaType> encoded,input;check(MFCreateMediaType(&encoded),"output type");
    check(encoded->SetGUID(MF_MT_MAJOR_TYPE,MFMediaType_Video),"major video");
    check(encoded->SetGUID(MF_MT_SUBTYPE,MFVideoFormat_H264),"H264 output");
    check(encoded->SetUINT32(MF_MT_AVG_BITRATE,6'000'000),"bitrate");
    check(encoded->SetUINT32(MF_MT_INTERLACE_MODE,MFVideoInterlace_Progressive),"progressive");
    check(MFSetAttributeSize(encoded.Get(),MF_MT_FRAME_SIZE,1280,720),"size");
    check(MFSetAttributeRatio(encoded.Get(),MF_MT_FRAME_RATE,60,1),"rate");
    check(MFSetAttributeRatio(encoded.Get(),MF_MT_PIXEL_ASPECT_RATIO,1,1),"aspect");
    DWORD stream=0;check(sink->AddStream(encoded.Get(),&stream),"add stream");
    check(MFCreateMediaType(&input),"input type");check(input->SetGUID(MF_MT_MAJOR_TYPE,MFMediaType_Video),"input video");
    check(input->SetGUID(MF_MT_SUBTYPE,MFVideoFormat_NV12),"NV12 input");
    check(input->SetUINT32(MF_MT_INTERLACE_MODE,MFVideoInterlace_Progressive),"input progressive");
    check(MFSetAttributeSize(input.Get(),MF_MT_FRAME_SIZE,1280,720),"input size");
    check(MFSetAttributeRatio(input.Get(),MF_MT_FRAME_RATE,60,1),"input rate");
    check(MFSetAttributeRatio(input.Get(),MF_MT_PIXEL_ASPECT_RATIO,1,1),"input aspect");
    check(sink->SetInputMediaType(stream,input.Get(),nullptr),"set input");check(sink->BeginWriting(),"begin MP4");
    const auto start=entries.front().at("capture_complete_ns").get<Nanoseconds>();
    std::ofstream mapping(output.string()+".jsonl");
    for(std::size_t i=0;i<entries.size();++i) {
        const auto& entry=entries[i];const auto frame=load_diagnostic_png(root/safe_relative(entry.at("path")));
        ComPtr<IMFMediaBuffer> buffer;check(MFCreateMemoryBuffer(1280*720*3/2,&buffer),"NV12 buffer");
        BYTE* data=nullptr;check(buffer->Lock(&data,nullptr,nullptr),"buffer lock");
        const auto clamp=[](int value){return static_cast<BYTE>(std::clamp(value,0,255));};
        for(int y=0;y<720;y+=2)for(int x=0;x<1280;x+=2) {
            int u=0,v=0;
            for(int dy=0;dy<2;++dy)for(int dx=0;dx<2;++dx) {
                const auto* pixel=frame.rgb.data()+(y+dy)*frame.stride+(x+dx)*3;
                const int r=pixel[0],g=pixel[1],b=pixel[2];
                data[(y+dy)*1280+x+dx]=clamp(((66*r+129*g+25*b+128)>>8)+16);
                u+=((-38*r-74*g+112*b+128)>>8)+128;v+=((112*r-94*g-18*b+128)>>8)+128;
            }
            const int uv=1280*720+(y/2)*1280+x;data[uv]=clamp(u/4);data[uv+1]=clamp(v/4);
        }
        check(buffer->Unlock(),"buffer unlock");check(buffer->SetCurrentLength(1280*720*3/2),"buffer length");
        ComPtr<IMFSample> sample;check(MFCreateSample(&sample),"sample");check(sample->AddBuffer(buffer.Get()),"sample buffer");
        const auto time=entry.at("capture_complete_ns").get<Nanoseconds>();
        const auto duration=i+1<entries.size()?entries[i+1].at("capture_complete_ns").get<Nanoseconds>()-time:16'666'667;
        check(sample->SetSampleTime((time-start)/100),"sample QPC relative time");
        check(sample->SetSampleDuration(std::max<Nanoseconds>(1,duration/100)),"sample duration");
        check(sink->WriteSample(stream,sample.Get()),"write H264 sample");
        mapping<<json{{"video_time_ns",time-start},{"source_frame",entry.at("source_frame")},
            {"ordinal",entry.at("ordinal")},{"capture_complete_ns",time},{"duration_ns",duration}}.dump()<<'\n';
        if(i%60==0&&std::filesystem::exists(output)&&std::filesystem::file_size(output)>512ULL*1024*1024)
            throw std::runtime_error("preview MP4 capacity exceeded");
    }
    check(sink->Finalize(),"finalize MP4");mapping.close();
    const json report={{"frames",entries.size()},{"source_index_sha256",sha256_file(root/"index.jsonl")},
        {"mp4_sha256",sha256_file(output)},{"mapping_sha256",sha256_file(output.string()+".jsonl")},
        {"encoding","H264/NV12 lossy review preview only; PNG originals are annotation evidence"},
        {"timing","capture QPC differences / 100ns Media Foundation time base; no synthesized intermediate images"},
        {"recording_complete",summary.value("continuous_received_pixels_complete",false)},
        {"source_absolute_age",nullptr},{"real_input_backend_constructed",false}};
    std::ofstream file(output.string()+".json");file<<report.dump(2)<<'\n';std::cout<<report.dump(2)<<'\n';
    return 0;
}
}
int main(int argc,char** argv) {
    try {
        if(argc==4&&std::string(argv[1])=="bench")return bench(argv[2],argv[3]);
        if(argc==4&&std::string(argv[1])=="video")return video(argv[2],argv[3]);
        if(argc==5&&std::string(argv[1])=="select") {
            const auto clips=prepare_recording_selection(argv[2],argv[3],argv[4]);
            for(const auto& clip:clips) {
                if(video(clip,clip/"review.mp4")!=0)throw std::runtime_error("selected preview failed");
                std::cout<<"Completed "<<clip.filename().string()<<'\n'<<std::flush;
            }
            const auto path=std::filesystem::path(argv[4])/"package-summary.json";
            auto report=read_json(path);report["state"]="ready";report["previews_exported"]=clips.size();
            std::ofstream file(path,std::ios::binary);file<<report.dump(2)<<'\n';
            if(!file)throw std::runtime_error("selection completion report failed");
            return 0;
        }
        throw std::invalid_argument("usage: pas_recording_check bench clip-root new-output-dir | video recording-root new-output.mp4");
    } catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
}
