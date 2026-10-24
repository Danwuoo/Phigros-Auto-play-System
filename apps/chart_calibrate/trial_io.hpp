#pragma once
#include "pas/adb.hpp"
#ifdef _MSC_VER
#pragma warning(push)
// gRPC's own deprecated TLS compatibility types instantiate in its headers.
#pragma warning(disable:4996)
#endif
#include "pas/emulator.hpp"
#ifdef _MSC_VER
#pragma warning(pop)
#endif
#include "rhythm/chart.hpp"
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <limits>
#include <regex>
#include <stdexcept>

namespace rhythm::calibration_trial {
using Json=nlohmann::json;
inline std::string read_file(const std::filesystem::path& path,std::uintmax_t limit) {
    if(std::filesystem::file_size(path)>limit)throw std::runtime_error("file byte budget");
    std::ifstream stream(path,std::ios::binary);
    std::string bytes((std::istreambuf_iterator<char>(stream)),{});
    if(!stream||bytes.size()>limit)throw std::runtime_error("file read failed");
    return bytes;
}
inline void write_json(const std::filesystem::path& path,const Json& value) {
    std::ofstream stream(path,std::ios::binary);stream<<value.dump(2)<<'\n';stream.close();
    if(!stream)throw std::runtime_error("JSON write failed");
}
inline std::string adb_text(const std::filesystem::path& adb,const std::string& serial,
                            const std::vector<std::string>& tail,int timeout=2000) {
    std::vector<std::string> args{"-s",serial};args.insert(args.end(),tail.begin(),tail.end());
    const auto raw=pas::adb_call(adb,args,timeout);return {raw.begin(),raw.end()};
}
inline std::string field(const std::string& text,const std::string& key) {
    const auto position=text.find(key);if(position==std::string::npos)return {};
    const auto begin=position+key.size();return text.substr(begin,text.find_first_of(" \t\r\n",begin)-begin);
}
inline void foreground(const std::filesystem::path& adb,const std::string& serial) {
    const auto report=adb_text(adb,serial,{"shell","dumpsys","window"});
    const auto begin=report.find("mCurrentFocus=");
    if(begin==std::string::npos||report.substr(begin,report.find('\n',begin)-begin).find("com.PigeonGames.Phigros/")==std::string::npos)
        throw std::runtime_error("Phigros is not foreground");
}
struct Journal {
    std::ofstream stream;std::string pending;std::size_t rows=0,bytes=0;
    explicit Journal(const std::filesystem::path& path):stream(path,std::ios::binary) {
        if(!stream)throw std::runtime_error("journal open failed");
        pending.reserve(65536);
    }
    void append(const Json& row) {
        const auto text=row.dump()+"\n";
        if(rows>=16000||text.size()>64*1024*1024-bytes)throw std::runtime_error("journal budget");
        pending+=text;
        ++rows;bytes+=text.size();
    }
    void flush_after_cleanup() {
        stream<<pending;stream.flush();
        if(!stream)throw std::runtime_error("journal write failed after cleanup");
        pending.clear();
    }
};
inline Json receipt_json(const pas::TouchReceipt& value) {
    return {{"event","rpc"},{"phase",static_cast<int>(value.command.phase)},
        {"id",value.command.contact_id},{"x",value.command.x},{"y",value.command.y},
        {"scheduled_ns",value.command.scheduled_ns},{"host_start_ns",value.injection_start_ns},
        {"host_return_ns",value.injection_return_ns},{"success",value.success},{"reason",value.reason},
        {"effect_verified_by_rpc",false}};
}
inline Json release_json(const pas::ReleaseReport& value) {
    return {{"requested",value.requested_ids},{"failed",value.failed_ids},{"unknown",value.unknown_ids},
        {"host_start_ns",value.start_ns},{"host_return_ns",value.return_ns},{"effect_verified_by_rpc",false}};
}
// JSON numeric conversion must not turn float/bool/oversized values into IDs.
inline std::int64_t kernel_integer(const Json& value,std::int64_t lower,std::int64_t upper) {
    if(!value.is_number_integer())throw std::runtime_error("kernel field must be an integer");
    if(value.is_number_unsigned()&&value.get<std::uint64_t>()>static_cast<std::uint64_t>(upper))
        throw std::runtime_error("kernel integer out of range");
    const auto number=value.get<std::int64_t>();
    if(number<lower||number>upper)throw std::runtime_error("kernel integer out of range");
    return number;
}
inline void validate_kernel_reply(const Json& result,const Json& profile) {
    if(!result.is_object()||result.at("schema")!="pas.evdev-slot-snapshot.v1")
        throw std::runtime_error("kernel reply schema changed");
    for(const char* key:{"boot_id","path","name"})
        if(!result.at(key).is_string()||result.at(key)!=profile.at(key))throw std::runtime_error("kernel route identity changed");
    const auto slots=kernel_integer(profile.at("slot_count"),1,64);
    if(kernel_integer(result.at("rdev"),1,std::numeric_limits<std::int64_t>::max())!=
           kernel_integer(profile.at("rdev"),1,std::numeric_limits<std::int64_t>::max())||
       kernel_integer(result.at("slot_count"),1,64)!=slots)
        throw std::runtime_error("kernel device identity changed");
    (void)kernel_integer(result.at("pid"),1,std::numeric_limits<std::int32_t>::max());
    (void)kernel_integer(result.at("error_errno"),0,0);
    (void)kernel_integer(result.at("bus"),6,6);
    for(const char* key:{"vendor","product","version"})(void)kernel_integer(result.at(key),0,0);
    for(const char* key:{"input_enabled","grab_enabled","game_callback_verified"})
        if(!result.at(key).is_boolean()||result.at(key).get<bool>())throw std::runtime_error("kernel observer scope changed");
    const auto before=kernel_integer(result.at("android_monotonic_before_ns"),0,std::numeric_limits<std::int64_t>::max());
    (void)kernel_integer(result.at("android_monotonic_after_ns"),before,std::numeric_limits<std::int64_t>::max());
    const auto& ids=result.at("tracking_ids");
    if(!ids.is_array()||ids.size()!=static_cast<std::size_t>(slots))throw std::runtime_error("kernel slot vector changed");
    std::int64_t active=0;
    for(std::size_t i=0;i<ids.size();++i) {
        const auto id=kernel_integer(ids[i],-1,std::numeric_limits<std::int32_t>::max());
        if(id<0)continue;
        for(std::size_t j=0;j<i;++j)if(kernel_integer(ids[j],-1,std::numeric_limits<std::int32_t>::max())==id)
            throw std::runtime_error("duplicate active kernel tracking ID");
        ++active;
    }
    if(kernel_integer(result.at("active_count"),0,slots)!=active)throw std::runtime_error("kernel active count disagrees");
}
// One request launches a read-only helper which queries AFTER request start.
// Its result is causally within the host interval, never a device-clock fit.
// This is an instantaneous kernel state of the verified route, not game delivery.
class KernelSlots {
public:
    KernelSlots(std::filesystem::path adb,std::string serial,Json profile,const pas::Clock& clock)
      :adb_(std::move(adb)),serial_(std::move(serial)),profile_(std::move(profile)),clock_(clock) {
        if(profile_.at("schema")!="pas.first-tap-kernel-profile.v1"||
           profile_.at("path")!="/dev/input/event2"||profile_.at("name")!="virtio_input_multi_touch_1"||
           kernel_integer(profile_.at("rdev"),3394,3394)!=3394||kernel_integer(profile_.at("slot_count"),11,11)!=11||
           !std::regex_match(profile_.at("boot_id").get<std::string>(),std::regex("[a-f0-9]{8}-[a-f0-9]{4}-[a-f0-9]{4}-[a-f0-9]{4}-[a-f0-9]{12}"))||
           profile_.at("helper")!="/data/local/tmp/pas-evdev-slots-1bee0d00ef2e69bb"||
           profile_.at("helper_sha256")!="1bee0d00ef2e69bbad846729ddd73e27b8c61b48cd61e4c03ffc2ba897f77ccb")
            throw std::invalid_argument("unsupported kernel profile");
        const auto sha=adb_text(adb_,serial_,{"shell","sha256sum",profile_.at("helper").get<std::string>()});
        if(sha.substr(0,64)!=profile_.at("helper_sha256").get<std::string>())throw std::runtime_error("kernel helper SHA mismatch");
    }
    Json query() const {
        const auto begin=clock_.now_ns();
        const auto bytes=adb_text(adb_,serial_,{"shell",profile_.at("helper").get<std::string>(),"/dev/input/event2"},1000);
        const auto end=clock_.now_ns();
        if(bytes.size()>8192)throw std::runtime_error("kernel reply byte budget");
        Json result=Json::parse(bytes);validate_kernel_reply(result,profile_);
        result["host_request_begin_ns"]=begin;result["host_reply_end_ns"]=end;
        result["causal_host_interval_valid"]=begin>=0&&end>=begin&&end-begin<250'000'000;
        result["reply_sha256"]=chart_text_sha256(bytes);
        return result;
    }
private:
    std::filesystem::path adb_;std::string serial_;Json profile_;const pas::Clock& clock_;
};
} // namespace rhythm::calibration_trial
