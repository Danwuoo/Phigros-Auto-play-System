#pragma once
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <stdexcept>

namespace rhythm::experimental {

enum class RecoveryRoute {stream,snapshot};
struct RecoveryFrameMeta {
    std::int64_t received_ns=0,source_timestamp_us=0;
    int width=0,height=0,stride=0,rotation=-1;
    bool source_valid=false;
};
struct RecoveryAdmission {
    bool accepted=false;
    std::uint64_t sequence=0;
    const char* reason="not_admitted";
};

// Pure scalar admission policy. The caller serializes every method with the
// same publisher mutex and publishes the original frame/receipt only on true.
// A new unary capture is an observation, not proof of new rendering/gameplay.
class FreshCaptureRecovery {
public:
    static constexpr std::int64_t probe_interval_ns=200'000'000;
    static constexpr std::int64_t frame_max_age_ns=250'000'000;
    static constexpr std::int64_t max_relative_lag_ns=250'000'000;
    static constexpr std::size_t max_probe_attempts=4096;
private:
    static constexpr std::int64_t second=1'000'000'000;
    static constexpr std::int64_t safe=std::numeric_limits<std::int64_t>::max()/4;
    int expected_rotation_;
    std::int64_t anchor_received_ns_=-1,anchor_source_us_=0;
    std::int64_t last_received_ns_=-1,last_source_us_=0,last_probe_ns_=-1,last_decision_ns_=-1;
    std::uint64_t sequence_=0;
    std::size_t probes_=0;
    static bool host_time(std::int64_t value) noexcept {return value>=0&&value<=safe;}
    static RecoveryAdmission reject(const char* why) noexcept {return {false,0,why};}
public:
    explicit FreshCaptureRecovery(int expected_rotation):expected_rotation_(expected_rotation) {
        if(expected_rotation!=0&&expected_rotation!=1)
            throw std::invalid_argument("capture recovery requires the fixed portrait/landscape source rotation");
    }
    RecoveryAdmission admit(const RecoveryFrameMeta& meta,RecoveryRoute route,std::int64_t now,bool armed) noexcept {
        if(route!=RecoveryRoute::stream&&route!=RecoveryRoute::snapshot)return reject("invalid_recovery_route");
        if(!host_time(now)||!host_time(meta.received_ns)||now<meta.received_ns||now<last_decision_ns_)
            return reject("invalid_or_regressed_recovery_host_clock");
        if(!meta.source_valid)return reject("invalid_recovery_source");
        if(meta.width!=1280||meta.height!=720||meta.stride!=3840||meta.rotation!=expected_rotation_)
            return reject("changed_recovery_geometry_or_rotation");
        if(now-meta.received_ns>=frame_max_age_ns)return reject("stale_recovery_frame_receipt");
        if(meta.source_timestamp_us<=0||meta.source_timestamp_us>std::numeric_limits<std::int64_t>::max()/1000)
            return reject("invalid_recovery_source_timestamp");
        if(route==RecoveryRoute::snapshot&&(!armed||anchor_received_ns_<0))
            return reject("snapshot_recovery_requires_armed_stream_anchor");
        if(anchor_received_ns_>=0) {
            if(meta.received_ns<=last_received_ns_)return reject("nonmonotonic_recovery_receipt");
            if(meta.source_timestamp_us<=last_source_us_)return reject("nonmonotonic_recovery_source_timestamp");
            const auto source_delta_us=meta.source_timestamp_us-last_source_us_;
            const auto host_delta=meta.received_ns-last_received_ns_;
            if(source_delta_us>10'000'000||host_delta>10*second)
                return reject("recovery_source_timestamp_discontinuity");
            const auto source_delta=source_delta_us*1000;
            if(source_delta>std::max(second,host_delta+500'000'000))
                return reject("recovery_source_timestamp_discontinuity");
            const auto anchor_delta_us=meta.source_timestamp_us-anchor_source_us_;
            if(anchor_delta_us<0||anchor_delta_us>std::numeric_limits<std::int64_t>::max()/1000)
                return reject("recovery_source_anchor_overflow");
            const auto lag=meta.received_ns-anchor_received_ns_-anchor_delta_us*1000;
            if(lag>max_relative_lag_ns)return reject("recovery_relative_lag_exceeded");
        }
        if(sequence_==std::numeric_limits<std::uint64_t>::max())return reject("recovery_publication_sequence_exhausted");
        if(anchor_received_ns_<0) {
            // Snapshot can never establish/reset this original stream anchor.
            anchor_received_ns_=meta.received_ns;anchor_source_us_=meta.source_timestamp_us;
        }
        last_received_ns_=meta.received_ns;last_source_us_=meta.source_timestamp_us;last_decision_ns_=now;
        return {true,++sequence_,route==RecoveryRoute::snapshot?"fresh_actual_snapshot_accepted":"fresh_stream_frame_accepted"};
    }
    // A true return reserves this one probe attempt. Failures never renew the
    // accepted receipt; there is no permission/lease update in this method.
    bool should_probe(std::int64_t now,bool armed) noexcept {
        if(!armed||anchor_received_ns_<0||!host_time(now)||now<last_decision_ns_||now<last_received_ns_||
           probes_>=max_probe_attempts||now-last_received_ns_<probe_interval_ns||
           (last_probe_ns_>=0&&(now<last_probe_ns_||now-last_probe_ns_<probe_interval_ns)))return false;
        last_probe_ns_=last_decision_ns_=now;++probes_;return true;
    }
    std::int64_t last_received_ns()const noexcept{return last_received_ns_;}
    std::int64_t last_source_timestamp_us()const noexcept{return last_source_us_;}
    std::int64_t last_probe_ns()const noexcept{return last_probe_ns_;}
    std::uint64_t sequence()const noexcept{return sequence_;}
    std::size_t probe_attempts()const noexcept{return probes_;}
    int expected_rotation()const noexcept{return expected_rotation_;}
};
} // namespace rhythm::experimental
