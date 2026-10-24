#pragma once

#include "pointer_ledger.hpp"

// Independent bounded observation layer for the restored PAS NativeActivity.
// API timestamp semantics: https://developer.android.com/ndk/reference/group/input
// AMotionEvent_*Time uses Android System.nanoTime(), not Windows host/QPC time.
#include <android/input.h>
#include <android/log.h>
#include <android/native_activity.h>
#include <sys/system_properties.h>
#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>
#include <time.h>
#include <array>
#include <cerrno>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <locale>
#include <sstream>
#include <string>

namespace pas_fixture_evidence {
class Recorder {
public:
    static constexpr std::uint64_t max_events=8192,max_bytes=16*1024*1024;
    static constexpr std::int64_t max_duration_ns=60'000'000'000LL;
    void start(ANativeActivity* activity) {
        if(started_)return;
        started_=true;
        char enabled[PROP_VALUE_MAX]{};
        __system_property_get("debug.pas.touch_evidence",enabled);
        if(std::strcmp(enabled,"1")!=0)return;
        start_ns_=now();
        if(start_ns_<0||!activity||!activity->internalDataPath) {log("Evidence initialization unavailable");return;}
        char path[4096];
        const int count=std::snprintf(path,sizeof(path),"%s/touch-evidence-%ld-%lld.jsonl",activity->internalDataPath,
            static_cast<long>(getpid()),static_cast<long long>(start_ns_));
        if(count<=0||static_cast<std::size_t>(count)>=sizeof(path)) {log("Evidence path exceeds bound");return;}
        fd_=open(path,O_WRONLY|O_CREAT|O_EXCL|O_CLOEXEC,0600);
        if(fd_<0) {log("Evidence file could not be exclusively created");return;}
        __android_log_print(ANDROID_LOG_INFO,"PASTouchEvidence","path=%s max_events=%llu max_bytes=%llu duration_ns=%lld",path,
            static_cast<unsigned long long>(max_events),static_cast<unsigned long long>(max_bytes),static_cast<long long>(max_duration_ns));
        std::ostringstream header;header.imbue(std::locale::classic());
        header<<"{\"type\":\"header\",\"schema\":\"pas.android.touch-evidence.v1\",\"fixture\":\"native-evidence-v3\","
              <<"\"arrival_clock\":\"Android CLOCK_MONOTONIC nanoseconds\",\"event_clock\":\"Android System.nanoTime nanoseconds (NDK input contract)\","
              <<"\"host_clock_mapping\":\"unknown; Windows host QPC is a different clock domain\",\"history_samples\":\"count only; not expanded\","
              <<"\"start_monotonic_ns\":"<<start_ns_<<",\"max_events\":"<<max_events<<",\"max_bytes\":"<<max_bytes<<",\"max_duration_ns\":"<<max_duration_ns<<"}\n";
        if(!write_all(header.str()))fail();
    }
    void poll() {
        if(fd_<0)return;
        const auto stamp=now();
        if(stamp<0||stamp<start_ns_) {fault_=true;close("clock_fault");}
        else if(stamp-start_ns_>=max_duration_ns) close("duration_limit");
    }
    void event(const AInputEvent* event,int width,int height) {
        if(fd_<0||AInputEvent_getType(event)!=AINPUT_EVENT_TYPE_MOTION)return;
        const auto arrival=now();
        if(arrival<0||arrival<start_ns_) {fault_=true;close("clock_fault");return;}
        if(arrival-start_ns_>=max_duration_ns) {close("duration_limit");return;}
        if(events_>=max_events) {++first_unrecorded_;close("event_limit");return;}
        const auto pointers=AMotionEvent_getPointerCount(event);
        const int raw=AMotionEvent_getAction(event);
        const int action=raw&AMOTION_EVENT_ACTION_MASK;
        const int index=(raw&AMOTION_EVENT_ACTION_POINTER_INDEX_MASK)>>AMOTION_EVENT_ACTION_POINTER_INDEX_SHIFT;
        const auto event_time=AMotionEvent_getEventTime(event),down_time=AMotionEvent_getDownTime(event);
        if(pointers==0||pointers>ledger_.active.size()||index<0||static_cast<std::size_t>(index)>=pointers||
           event_time<0||down_time<0||down_time>event_time) {++first_unrecorded_;fault_=true;close("invalid_event");return;}
        std::array<bool,32> present{};
        for(std::size_t p=0;p<pointers;++p) {
            const int id=AMotionEvent_getPointerId(event,p);
            if(id<0||static_cast<std::size_t>(id)>=ledger_.active.size()||present[static_cast<std::size_t>(id)]||
               !std::isfinite(AMotionEvent_getX(event,p))||!std::isfinite(AMotionEvent_getY(event,p))||
               !std::isfinite(AMotionEvent_getPressure(event,p))) {++first_unrecorded_;fault_=true;close("invalid_pointer");return;}
            present[static_cast<std::size_t>(id)]=true;
        }
        if(last_event_ns_>=0&&event_time<last_event_ns_)++time_errors_;
        last_event_ns_=event_time;
        const auto changed=static_cast<std::size_t>(AMotionEvent_getPointerId(event,static_cast<std::size_t>(index)));
        PointerAction kind=PointerAction::other;
        switch(action) {
        case AMOTION_EVENT_ACTION_DOWN:kind=PointerAction::down;break;
        case AMOTION_EVENT_ACTION_POINTER_DOWN:kind=PointerAction::pointer_down;break;
        case AMOTION_EVENT_ACTION_UP:kind=PointerAction::up;break;
        case AMOTION_EVENT_ACTION_POINTER_UP:kind=PointerAction::pointer_up;break;
        case AMOTION_EVENT_ACTION_MOVE:kind=PointerAction::move;break;
        case AMOTION_EVENT_ACTION_CANCEL:kind=PointerAction::cancel;break;
        }
        ledger_.observe(kind,changed,present);
        std::ostringstream row;row.imbue(std::locale::classic());row.precision(9);
        row<<"{\"type\":\"motion\",\"sequence\":"<<events_+1<<",\"arrival_monotonic_ns\":"<<arrival
           <<",\"android_event_ns\":"<<event_time<<",\"android_down_ns\":"<<down_time
           <<",\"action_raw\":"<<raw<<",\"action\":"<<action<<",\"action_index\":"<<index
           <<",\"source\":"<<AInputEvent_getSource(event)<<",\"device_id\":"<<AInputEvent_getDeviceId(event)
           <<",\"surface_width\":"<<width<<",\"surface_height\":"<<height
           <<",\"history_size\":"<<AMotionEvent_getHistorySize(event)<<",\"pointers\":[";
        for(std::size_t p=0;p<pointers;++p) {
            if(p)row<<',';
            row<<"{\"id\":"<<AMotionEvent_getPointerId(event,p)<<",\"x\":"<<AMotionEvent_getX(event,p)
               <<",\"y\":"<<AMotionEvent_getY(event,p)<<",\"pressure\":"<<AMotionEvent_getPressure(event,p)
               <<",\"tool_type\":"<<AMotionEvent_getToolType(event,p)<<'}';
        }
        row<<"],\"active_after\":[";bool comma=false;
        for(std::size_t p=0;p<ledger_.active.size();++p) if(ledger_.active[p]) {if(comma)row<<',';row<<p;comma=true;}
        row<<"],\"lifecycle_errors\":"<<lifecycle_errors()<<"}\n";
        const auto text=row.str();
        if(text.size()>max_bytes-2048||bytes_>max_bytes-2048-text.size()) {++first_unrecorded_;close("byte_limit");return;}
        if(!write_all(text)) {fail();return;}
        ++events_;
    }
    void close(const char* reason) {
        if(fd_<0)return;
        const auto stop=now();
        const char* release=fault_||first_unrecorded_||lifecycle_errors() ? "unknown_due_incomplete_evidence" :
            (!ledger_.saw_pointer ? "no_pointer_observations" : (ledger_.count()==0 ? "zero_active_in_observed_stream" : "active_at_close"));
        std::ostringstream footer;footer.imbue(std::locale::classic());
        footer<<"{\"type\":\"footer\",\"stop_reason\":\""<<reason<<"\",\"stop_monotonic_ns\":"<<stop
              <<",\"events_written\":"<<events_<<",\"bytes_before_footer\":"<<bytes_
              <<",\"first_unrecorded_events\":"<<first_unrecorded_<<",\"further_events_not_observed\":true"
              <<",\"lifecycle_errors\":"<<lifecycle_errors()<<",\"fault\":"<<(fault_?"true":"false")
              <<",\"active_after_last_observation\":"<<ledger_.count()<<",\"release_observation\":\""<<release<<"\"}\n";
        if(!write_all(footer.str()))log("Evidence footer write failed; file is partial");
        if(::close(fd_)!=0)log("Evidence close failed; persistence unknown");
        fd_=-1;
    }
private:
    static std::int64_t now() {timespec stamp{};return clock_gettime(CLOCK_MONOTONIC,&stamp)==0 ? stamp.tv_sec*1'000'000'000LL+stamp.tv_nsec : -1;}
    static void log(const char* text) {__android_log_write(ANDROID_LOG_ERROR,"PASTouchEvidence",text);}
    std::uint64_t lifecycle_errors() const {return time_errors_+ledger_.errors;}
    bool write_all(const std::string& text) {
        std::size_t offset=0;
        while(offset<text.size()) {const auto written=write(fd_,text.data()+offset,text.size()-offset);if(written<0&&errno==EINTR)continue;if(written<=0)return false;offset+=static_cast<std::size_t>(written);}
        bytes_+=text.size();return true;
    }
    void fail() {fault_=true;log("Evidence I/O failed; file is partial");if(fd_>=0)::close(fd_);fd_=-1;}
    bool started_=false,fault_=false;
    int fd_=-1;
    std::int64_t start_ns_=0,last_event_ns_=-1;
    std::uint64_t events_=0,bytes_=0,first_unrecorded_=0,time_errors_=0;
    PointerLedger ledger_;
};
} // namespace pas_fixture_evidence
