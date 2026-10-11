// Independent diagnostic controller. No gameplay, producer launch, or qualification.
// Build as C++20 with nlohmann/json; Windows libraries: advapi32, bcrypt.
// Tests include this TU with PAS_COLD_SCHEDULER_TRACE_NO_MAIN and inject the ETW API.
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <evntrace.h>
#include <bcrypt.h>
#include <nlohmann/json.hpp>
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <limits>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>
#pragma comment(lib, "advapi32.lib")
#pragma comment(lib, "bcrypt.lib")
#ifndef PAS_COLD_SCHEDULER_TOOL_SOURCE_COMMIT
#define PAS_COLD_SCHEDULER_TOOL_SOURCE_COMMIT "UNBOUND"
#endif
#ifndef PAS_COLD_SCHEDULER_TOOL_SOURCE_DIRTY
#define PAS_COLD_SCHEDULER_TOOL_SOURCE_DIRTY 1
#endif

namespace pas::cold_scheduler {
using Json = nlohmann::json;
namespace fs = std::filesystem;
constexpr ULONG enable_flags = 0x10000810u;
// Explicit Win8+ hybrid-shutdown behavior keeps normalized mode readback exact.
constexpr ULONG log_mode = EVENT_TRACE_SYSTEM_LOGGER_MODE | EVENT_TRACE_FILE_MODE_SEQUENTIAL |
    EVENT_TRACE_STOP_ON_HYBRID_SHUTDOWN;
constexpr std::uint64_t buffer_cap = 64ull * 1024 * 1024;
constexpr std::uint64_t file_cap = 128ull * 1024 * 1024;
constexpr std::size_t json_cap = 65536;
constexpr std::string_view base55 = "12ec0550e882e3b06751d67fa67c43dda3e9312b";
constexpr std::string_view compiled57 = "5dce75d12c63ed51de5d996cedcd44e29d729841";

void require(bool ok, const char* reason) { if (!ok) throw std::runtime_error(reason); }
bool hex_text(std::string_view s, std::size_t n) {
    return s.size() == n && std::all_of(s.begin(), s.end(), [](char c) {
        return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f');
    });
}
std::uint64_t integer(const Json& j, const char* key) {
    const auto& v = j.at(key);
    require(v.is_number_integer() && (v.is_number_unsigned() || v.get<std::int64_t>() >= 0), "typed_nonnegative_integer_required");
    return v.get<std::uint64_t>();
}
void flag(const Json& j, const char* key, bool wanted) {
    require(j.at(key).is_boolean() && j.at(key).get<bool>() == wanted, "typed_flag_mismatch");
}
std::string text_value(const Json& j, const char* key, std::size_t cap = 1024) {
    require(j.at(key).is_string(), "typed_string_required");
    auto s = j.at(key).get<std::string>();
    require(!s.empty() && s.size() <= cap && s.find('\0') == std::string::npos, "string_bound");
    return s;
}
std::wstring wide(std::string_view s) {
    const auto n = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, s.data(), static_cast<int>(s.size()), nullptr, 0);
    require(n > 0 && n <= 1024, "invalid_utf8");
    std::wstring r(n, L'\0');
    require(MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, s.data(), static_cast<int>(s.size()), r.data(), n) == n, "utf8_conversion");
    return r;
}
std::string utf8(std::wstring_view s) {
    if (s.empty()) return {};
    const auto n = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, s.data(), static_cast<int>(s.size()), nullptr, 0, nullptr, nullptr);
    require(n > 0 && n <= 4096, "invalid_utf16");
    std::string r(n, '\0');
    require(WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, s.data(), static_cast<int>(s.size()), r.data(), n, nullptr, nullptr) == n, "utf16_conversion");
    return r;
}
GUID parse_guid(std::string_view s) {
    require(s.size() == 36 && s[8] == '-' && s[13] == '-' && s[18] == '-' && s[23] == '-', "guid_format");
    auto part = [&](std::size_t first, std::size_t n) {
        std::uint32_t r = 0;
        for (std::size_t i = first; i < first + n; ++i) {
            const auto c = s[i];
            require((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'), "guid_hex");
            r = (r << 4) | (c <= '9' ? c - '0' : c - 'a' + 10);
        }
        return r;
    };
    GUID g{}; g.Data1 = part(0, 8); g.Data2 = static_cast<USHORT>(part(9, 4)); g.Data3 = static_cast<USHORT>(part(14, 4));
    g.Data4[0] = static_cast<BYTE>(part(19, 2)); g.Data4[1] = static_cast<BYTE>(part(21, 2));
    for (std::size_t i = 0; i < 6; ++i) g.Data4[i + 2] = static_cast<BYTE>(part(24 + i * 2, 2));
    const GUID zero{}, system{0x9e814aad,0x3204,0x11d2,{0x9a,0x82,0x00,0x60,0x08,0xa8,0x69,0x39}}, global{0xe8908abc,0xaa84,0x11d2,{0x9a,0x93,0x00,0x80,0x5f,0x85,0xd7,0xc6}};
    require(!IsEqualGUID(g, zero) && !IsEqualGUID(g, system) && !IsEqualGUID(g, global), "reserved_guid");
    return g;
}
std::string guid_text(const GUID& g) {
    char s[37]{};
    std::snprintf(s, sizeof(s), "%08lx-%04x-%04x-%02x%02x-%02x%02x%02x%02x%02x%02x",
        static_cast<unsigned long>(g.Data1), static_cast<unsigned>(g.Data2), static_cast<unsigned>(g.Data3),
        g.Data4[0],g.Data4[1],g.Data4[2],g.Data4[3],g.Data4[4],g.Data4[5],g.Data4[6],g.Data4[7]);
    return s;
}
struct Handle {
    HANDLE value = nullptr;
    explicit Handle(HANDLE h = nullptr) : value(h) {}
    ~Handle() { if (value && value != INVALID_HANDLE_VALUE) CloseHandle(value); }
    Handle(const Handle&) = delete; Handle& operator=(const Handle&) = delete;
};
std::string read_small(const fs::path& p, std::size_t cap = json_cap) {
    Handle h(CreateFileW(p.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr));
    require(h.value != INVALID_HANDLE_VALUE, "input_open");
    LARGE_INTEGER n{}; require(GetFileSizeEx(h.value, &n) && n.QuadPart > 0 && static_cast<std::uint64_t>(n.QuadPart) <= cap, "input_size");
    std::string s(static_cast<std::size_t>(n.QuadPart), '\0'); DWORD done = 0;
    require(ReadFile(h.value, s.data(), static_cast<DWORD>(s.size()), &done, nullptr) && done == s.size(), "input_read");
    return s;
}
Json parse_json(const std::string& s) {
    require(s.size() <= json_cap, "json_size");
    auto callback = [](int depth, Json::parse_event_t, Json&) { require(depth <= 12, "json_depth"); return true; };
    auto j = Json::parse(s, callback); require(j.is_object(), "json_object"); return j;
}
std::string sha256(const std::string& s) {
    BCRYPT_ALG_HANDLE alg{}; BCRYPT_HASH_HANDLE hash{};
    require(BCryptOpenAlgorithmProvider(&alg, BCRYPT_SHA256_ALGORITHM, nullptr, 0) >= 0, "sha_open");
    struct AlgClose { BCRYPT_ALG_HANDLE h; ~AlgClose(){ BCryptCloseAlgorithmProvider(h, 0); } } ac{alg};
    require(BCryptCreateHash(alg, &hash, nullptr, 0, nullptr, 0, 0) >= 0, "sha_create");
    struct HashClose { BCRYPT_HASH_HANDLE h; ~HashClose(){ BCryptDestroyHash(h); } } hc{hash};
    std::array<BYTE,32> bytes{};
    require(BCryptHashData(hash, reinterpret_cast<PUCHAR>(const_cast<char*>(s.data())), static_cast<ULONG>(s.size()), 0) >= 0 && BCryptFinishHash(hash, bytes.data(), static_cast<ULONG>(bytes.size()), 0) >= 0, "sha_finish");
    constexpr char hex[] = "0123456789abcdef"; std::string r; r.reserve(64);
    for (auto b : bytes) { r.push_back(hex[b >> 4]); r.push_back(hex[b & 15]); } return r;
}
void create_json(const fs::path& p, const Json& j) {
    auto s = j.dump(2); require(s.size() <= json_cap, "output_bound");
    require(!fs::exists(p), "output_not_fresh");
    auto temporary=p; temporary+=L".publish-tmp-"+std::to_wstring(GetCurrentProcessId());
    {
        Handle h(CreateFileW(temporary.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_NEW, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_WRITE_THROUGH, nullptr));
        require(h.value != INVALID_HANDLE_VALUE, "output_temp_not_fresh"); DWORD done = 0;
        require(WriteFile(h.value, s.data(), static_cast<DWORD>(s.size()), &done, nullptr) && done == s.size() && FlushFileBuffers(h.value), "output_write");
    }
    // No MOVEFILE_REPLACE_EXISTING. Readers never observe an incomplete final file.
    require(MoveFileExW(temporary.c_str(),p.c_str(),MOVEFILE_WRITE_THROUGH), "output_atomic_publish");
}
std::uint64_t qpc() { LARGE_INTEGER t{}; require(QueryPerformanceCounter(&t) && t.QuadPart >= 0, "qpc"); return static_cast<std::uint64_t>(t.QuadPart); }
std::uint64_t frequency() { LARGE_INTEGER f{}; require(QueryPerformanceFrequency(&f) && f.QuadPart > 0, "frequency"); return static_cast<std::uint64_t>(f.QuadPart); }
std::uint64_t creation_time(HANDLE p) {
    FILETIME c{},e{},k{},u{}; require(GetProcessTimes(p,&c,&e,&k,&u), "process_times");
    return (static_cast<std::uint64_t>(c.dwHighDateTime) << 32) | c.dwLowDateTime;
}
struct Config {
    std::wstring name, file;
    GUID guid{};
    std::uint64_t start_qpc = 0, deadline_qpc = 0, qpc_frequency = 0;
};
bool valid_deadline(std::uint64_t start,std::uint64_t deadline,std::uint64_t f,std::uint64_t now) {
    return f>0 && f<=std::numeric_limits<std::uint64_t>::max()/75 && start<=now && deadline>now && deadline>start && deadline-start<=f*75;
}
struct Snapshot {
    TRACEHANDLE handle = 0; GUID guid{}; std::wstring name, file;
    ULONG clock = 0, mode = 0, flags = 0, buffer_kb = 0, minimum = 0, maximum = 0, allocated = 0, maximum_file_mb = 0;
    std::uint64_t logger_thread = 0; ULONG lost = 0, log_lost = 0, realtime_lost = 0, buffers_written = 0;
};
bool identity_matches(const Snapshot& s, const Config& c, TRACEHANDLE handle, std::uint64_t logger_thread = 0) {
    return handle != 0 && s.handle == handle && IsEqualGUID(s.guid,c.guid) && s.name == c.name && s.file == c.file &&
        s.logger_thread != 0 && (logger_thread == 0 || s.logger_thread == logger_thread);
}
bool bounded_snapshot(const Snapshot& s) {
    const auto n = static_cast<std::uint64_t>(s.buffer_kb) * 1024;
    return s.clock == 1 && s.mode == log_mode && s.flags == enable_flags && s.maximum_file_mb == 128 &&
        n > 0 && s.minimum > 0 && s.minimum <= s.maximum && s.allocated > 0 && s.allocated <= s.maximum &&
        n * s.maximum <= buffer_cap && n * s.allocated <= buffer_cap;
}
bool loss_free(const Snapshot& s) { return s.lost == 0 && s.log_lost == 0 && s.realtime_lost == 0; }
Json snapshot_json(const Snapshot& s) {
    return {{"handle",s.handle},{"guid",guid_text(s.guid)},{"session_name",utf8(s.name)},{"file",utf8(s.file)},
        {"logger_thread_id",s.logger_thread},{"clock",s.clock},{"mode",s.mode},{"enable_flags",s.flags},
        {"buffer_kb",s.buffer_kb},{"minimum_buffers",s.minimum},{"maximum_buffers",s.maximum},{"allocated_buffers",s.allocated},
        {"maximum_file_mb",s.maximum_file_mb},{"events_lost",s.lost},{"log_buffers_lost",s.log_lost},{"realtime_buffers_lost",s.realtime_lost},{"buffers_written",s.buffers_written}};
}
struct Api {
    virtual ~Api() = default;
    virtual ULONG all(std::vector<Snapshot>&) = 0;
    virtual ULONG by_name(const std::wstring&, Snapshot&) = 0;
    virtual ULONG by_handle(TRACEHANDLE, Snapshot&) = 0;
    virtual ULONG start(const Config&, TRACEHANDLE&) = 0;
    virtual ULONG stop(TRACEHANDLE, Snapshot&) = 0;
};
// All stop calls are handle-only. A name query is observation, never authority to stop.
class Session {
    Api& api_; Config config_; TRACEHANDLE handle_ = 0; std::uint64_t thread_ = 0;
public:
    bool started = false, cleanup_confirmed = false, foreign_untouched = true, final_statistics_verified = false;
    ULONG start_status = ERROR_INVALID_FUNCTION, query_status = ERROR_INVALID_FUNCTION, stop_status = ERROR_INVALID_FUNCTION;
    Snapshot latest{}; std::string reason = "not_started", cleanup = "not_owned";
    Session(Api& a, Config c) : api_(a), config_(std::move(c)) {}
    TRACEHANDLE handle() const { return handle_; }
    const Config& config() const { return config_; }
    bool begin() {
        std::vector<Snapshot> active;
        if (api_.all(active) != ERROR_SUCCESS) { reason="collision_query_incomplete"; return false; }
        for (const auto& s : active) if (_wcsicmp(s.name.c_str(),config_.name.c_str()) == 0 || IsEqualGUID(s.guid,config_.guid)) {
            reason="existing_session_collision"; return false;
        }
        Snapshot s{}; query_status=api_.by_name(config_.name,s);
        if (query_status != ERROR_WMI_INSTANCE_NOT_FOUND) { reason=query_status==ERROR_SUCCESS?"existing_name":"name_query_failed"; return false; }
        start_status=api_.start(config_,handle_);
        if (start_status != ERROR_SUCCESS || handle_ == 0) { handle_=0; reason="start_failed"; return false; }
        started=true; cleanup="unknown";
        if (!inspect()) return false;
        reason="recording"; return true;
    }
    bool adopt(const Snapshot& owner) {
        if (!identity_matches(owner,config_,owner.handle)) { reason="owner_record_identity"; return false; }
        handle_=owner.handle; thread_=owner.logger_thread; started=true; cleanup="unknown";
        return inspect();
    }
    bool inspect() {
        Snapshot s{}; query_status=api_.by_handle(handle_,s);
        if (query_status != ERROR_SUCCESS) { reason="session_query_failed"; return false; }
        if (!identity_matches(s,config_,handle_,thread_)) { reason="ownership_mismatch"; return false; }
        latest=s; thread_=s.logger_thread;
        if (!bounded_snapshot(s)) { reason="normalized_bounds_or_policy"; return false; }
        if (!loss_free(s)) { reason="trace_events_lost"; return false; }
        return true;
    }
    bool finish() {
        if (!started || !handle_) return false;
        Snapshot observed{}; query_status=api_.by_handle(handle_,observed);
        if (query_status == ERROR_WMI_INSTANCE_NOT_FOUND) { cleanup_confirmed=true; cleanup="own_handle_absent"; return true; }
        if (query_status != ERROR_SUCCESS || !identity_matches(observed,config_,handle_,thread_)) {
            cleanup="ownership_unknown_no_stop"; return false;
        }
        latest=observed;
        Snapshot stopped=observed; stop_status=api_.stop(handle_,stopped);
        if (stop_status==ERROR_SUCCESS && identity_matches(stopped,config_,handle_,thread_) && bounded_snapshot(stopped)) {
            latest=stopped; final_statistics_verified=true;
        }
        // ERROR_MORE_DATA can mean STOP occurred. Always prove handle absence.
        Snapshot after{}; const auto absent=api_.by_handle(handle_,after);
        if (absent == ERROR_WMI_INSTANCE_NOT_FOUND) { cleanup_confirmed=true; cleanup="own_handle_absent_after_stop"; return true; }
        cleanup="stop_or_absence_unconfirmed"; return false;
    }
};
struct PropertyBlock {
    EVENT_TRACE_PROPERTIES p{}; std::array<wchar_t,1024> name{},file{};
    PropertyBlock() { p.Wnode.BufferSize=sizeof(*this); p.Wnode.Flags=WNODE_FLAG_TRACED_GUID;
        p.LoggerNameOffset=offsetof(PropertyBlock,name); p.LogFileNameOffset=offsetof(PropertyBlock,file); }
};
Snapshot extract(const PropertyBlock& b) {
    require(b.p.LoggerNameOffset == offsetof(PropertyBlock,name) && b.p.LogFileNameOffset == offsetof(PropertyBlock,file), "property_offsets");
    require(std::find(b.name.begin(),b.name.end(),L'\0') != b.name.end() && std::find(b.file.begin(),b.file.end(),L'\0') != b.file.end(), "property_string_bound");
    return {b.p.Wnode.HistoricalContext,b.p.Wnode.Guid,b.name.data(),b.file.data(),b.p.Wnode.ClientContext,b.p.LogFileMode,b.p.EnableFlags,
        b.p.BufferSize,b.p.MinimumBuffers,b.p.MaximumBuffers,b.p.NumberOfBuffers,b.p.MaximumFileSize,
        static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(b.p.LoggerThreadId)),b.p.EventsLost,b.p.LogBuffersLost,b.p.RealTimeBuffersLost,b.p.BuffersWritten};
}
struct WindowsApi final : Api {
    ULONG all(std::vector<Snapshot>& out) override {
        constexpr ULONG cap=64;
        auto blocks=std::make_unique<PropertyBlock[]>(cap); std::array<EVENT_TRACE_PROPERTIES*,cap> ptrs{};
        for(ULONG i=0;i<cap;++i) ptrs[i]=&blocks[i].p;
        ULONG count=0; const auto status=QueryAllTracesW(ptrs.data(),cap,&count);
        if(status!=ERROR_SUCCESS || count>cap) return status==ERROR_SUCCESS?ERROR_MORE_DATA:status;
        out.clear(); out.reserve(count); for(ULONG i=0;i<count;++i) out.push_back(extract(blocks[i])); return status;
    }
    ULONG by_name(const std::wstring& name, Snapshot& out) override {
        PropertyBlock b; const auto status=ControlTraceW(0,name.c_str(),&b.p,EVENT_TRACE_CONTROL_QUERY);
        if(status==ERROR_SUCCESS) out=extract(b); return status;
    }
    ULONG by_handle(TRACEHANDLE h, Snapshot& out) override {
        PropertyBlock b; const auto status=ControlTraceW(h,nullptr,&b.p,EVENT_TRACE_CONTROL_QUERY);
        if(status==ERROR_SUCCESS) out=extract(b); return status;
    }
    ULONG start(const Config& c, TRACEHANDLE& h) override {
        PropertyBlock b; b.p.Wnode.Guid=c.guid; b.p.Wnode.ClientContext=1;
        b.p.LogFileMode=log_mode; b.p.EnableFlags=enable_flags; b.p.BufferSize=64; b.p.MinimumBuffers=64; b.p.MaximumBuffers=1024; b.p.MaximumFileSize=128;
        require(c.name.size()<b.name.size() && c.file.size()<b.file.size(), "property_input_bound");
        std::copy(c.name.begin(),c.name.end(),b.name.begin()); std::copy(c.file.begin(),c.file.end(),b.file.begin());
        return StartTraceW(&h,c.name.c_str(),&b.p);
    }
    ULONG stop(TRACEHANDLE h, Snapshot& out) override {
        PropertyBlock b; b.p.Wnode.Guid=out.guid;
        const auto status=ControlTraceW(h,nullptr,&b.p,EVENT_TRACE_CONTROL_STOP);
        if(status==ERROR_SUCCESS) out=extract(b); return status;
    }
};
struct Request {
    Config config; fs::path root; std::string sha, execution;
    Json provenance;
};
bool leaf_ascii(std::wstring_view s) {
    return s.size()>8 && s.size()<=96 && s.substr(0,8)==L"prelive-" && std::all_of(s.begin(),s.end(),[](wchar_t c){return (c>=L'a'&&c<=L'z')||(c>=L'0'&&c<=L'9')||c==L'-';});
}
void reject_reparse_chain(fs::path p) {
    for (; !p.empty(); p=p.parent_path()) {
        const auto attr=GetFileAttributesW(p.c_str());
        require(attr!=INVALID_FILE_ATTRIBUTES && !(attr&FILE_ATTRIBUTE_REPARSE_POINT), "reparse_or_missing_parent");
        if(p==p.root_path()) break;
    }
}
Request load_request(const fs::path& path) {
    const auto bytes=read_small(path); const auto j=parse_json(bytes);
    require(text_value(j,"schema")=="pas.cold-scheduler-trace-request.v1", "request_schema");
    flag(j,"diagnostic_only",true); flag(j,"cost_qualification",false); flag(j,"live_ready",false);
    require(integer(j,"device_operations")==0 && integer(j,"duration_limit_s")==75 && integer(j,"buffer_limit_bytes")==buffer_cap && integer(j,"raw_limit_bytes")==file_cap, "fixed_scope");
    Request r; r.sha=sha256(bytes); r.execution=text_value(j,"execution_source_commit",40); require(hex_text(r.execution,40),"execution_identity");
    require(PAS_COLD_SCHEDULER_TOOL_SOURCE_DIRTY==0 && r.execution==PAS_COLD_SCHEDULER_TOOL_SOURCE_COMMIT &&
        text_value(j,"tool_source_commit",40)==PAS_COLD_SCHEDULER_TOOL_SOURCE_COMMIT,"controller_compiled_source_join");
    require(text_value(j,"compiled_source_commit",40)==compiled57 && text_value(j,"base_source_commit",40)==base55, "base_delta_identity");
    auto workspace=fs::canonical(fs::path(wide(text_value(j,"workspace"))));
    auto out=fs::canonical(workspace/L"out"); reject_reparse_chain(out);
    auto root=fs::path(wide(text_value(j,"artifact_root")));
    require(root.is_absolute() && root.parent_path()==out && leaf_ascii(root.filename().native()) && root.native().size()<=220, "artifact_containment"); r.root=root;
    r.config.name=wide(text_value(j,"session_name",96));
    require(r.config.name.rfind(L"PAS-ColdScheduler-",0)==0 && std::all_of(r.config.name.begin(),r.config.name.end(),[](wchar_t c){return (c>=L'a'&&c<=L'z')||(c>=L'A'&&c<=L'Z')||(c>=L'0'&&c<=L'9')||c==L'-';}), "session_name_scope");
    r.config.guid=parse_guid(text_value(j,"session_guid",36)); r.config.file=(root/L"scheduler.etl").native();
    r.config.start_qpc=integer(j,"start_qpc"); r.config.deadline_qpc=integer(j,"deadline_qpc"); r.config.qpc_frequency=integer(j,"qpc_frequency");
    const auto f=frequency(); const auto now=qpc();
    require(f==r.config.qpc_frequency && valid_deadline(r.config.start_qpc,r.config.deadline_qpc,f,now), "shared_deadline");
    const auto& pin=j.at("component_build"); auto binding_bytes=read_small(fs::path(wide(text_value(pin,"path"))));
    require(integer(pin,"bytes")==binding_bytes.size() && text_value(pin,"sha256",64)==sha256(binding_bytes), "component_binding_pin");
    auto binding=parse_json(binding_bytes);
    require(text_value(binding,"schema")=="pas.production-wait-diagnostic-component-build.v1" && text_value(binding,"delta_source_commit",40)==compiled57 && text_value(binding,"base_source_commit",40)==base55, "component_source_join");
    flag(binding,"diagnostic_only",true); flag(binding,"cost_qualification",false); flag(binding,"live_ready",false); flag(binding,"qualification_issued",false);
    const auto& producer=j.at("producer"); const auto producer_path=text_value(producer,"path");
    require(integer(producer,"bytes")==1838080 && text_value(producer,"sha256",64)=="252421f1bade66b8ca5b197bc30fcf74efc645e197351bd8b91441d4987fbaba", "sealed_producer_pin");
    std::size_t matches=0; for(const auto& p:binding.at("files")) if(text_value(p,"path")==producer_path) { require(p==producer,"producer_binding_join"); ++matches; }
    require(matches==1,"producer_unique_join");
    r.provenance={{"base_source_commit",std::string(base55)},{"compiled_source_commit",std::string(compiled57)},{"execution_source_commit",r.execution},
        {"component_build",pin},{"producer",producer},{"producer_pins_are_from_binding",true},{"producer_not_launched",true}};
    return r;
}
bool in_job() { BOOL b=FALSE; require(IsProcessInJob(GetCurrentProcess(),nullptr,&b),"job_query"); return b!=FALSE; }
Json rights() {
    Handle token; require(OpenProcessToken(GetCurrentProcess(),TOKEN_QUERY,&token.value),"token_open");
    TOKEN_ELEVATION e{}; DWORD n=0; require(GetTokenInformation(token.value,TokenElevation,&e,sizeof(e),&n),"token_elevation");
    alignas(TOKEN_PRIVILEGES) std::array<BYTE,8192> buffer{}; require(GetTokenInformation(token.value,TokenPrivileges,buffer.data(),static_cast<DWORD>(buffer.size()),&n),"token_privileges_bound");
    LUID id{}; require(LookupPrivilegeValueW(nullptr,SE_SYSTEM_PROFILE_NAME,&id),"privilege_lookup");
    const auto* p=reinterpret_cast<const TOKEN_PRIVILEGES*>(buffer.data()); bool present=false,enabled=false;
    require(p->PrivilegeCount<128,"privilege_count");
    for(DWORD i=0;i<p->PrivilegeCount;++i) if(p->Privileges[i].Luid.LowPart==id.LowPart && p->Privileges[i].Luid.HighPart==id.HighPart) { present=true; enabled=(p->Privileges[i].Attributes&SE_PRIVILEGE_ENABLED)!=0; }
    return {{"token_elevated",e.TokenIsElevated!=0},{"system_profile_privilege_present",present},{"system_profile_privilege_enabled",enabled},{"privileges_modified",false},{"named_logger_permission_not_inferred",true}};
}
void bound_own_job(Handle& job) {
    // Windows 8+ may nest this empty job beneath an existing host job. Actual
    // Assign/readback decides compatibility; do not infer failure from in_job().
    // Parent limits and parent-job termination still apply to this process.
    job.value=CreateJobObjectW(nullptr,nullptr); require(job.value!=nullptr,"own_job_create");
    JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits{};
    limits.BasicLimitInformation.LimitFlags=JOB_OBJECT_LIMIT_PROCESS_MEMORY|JOB_OBJECT_LIMIT_JOB_MEMORY;
    limits.ProcessMemoryLimit=256ull*1024*1024; limits.JobMemoryLimit=256ull*1024*1024;
    require(SetInformationJobObject(job.value,JobObjectExtendedLimitInformation,&limits,sizeof(limits)) && AssignProcessToJobObject(job.value,GetCurrentProcess()),"own_job_limit_or_assign");
    JOBOBJECT_EXTENDED_LIMIT_INFORMATION actual{}; require(QueryInformationJobObject(job.value,JobObjectExtendedLimitInformation,&actual,sizeof(actual),nullptr),"own_job_readback");
    require(actual.BasicLimitInformation.LimitFlags==limits.BasicLimitInformation.LimitFlags && actual.ProcessMemoryLimit==limits.ProcessMemoryLimit && actual.JobMemoryLimit==limits.JobMemoryLimit,"own_job_readback_mismatch");
    BOOL member=FALSE; require(IsProcessInJob(GetCurrentProcess(),job.value,&member) && member,"own_job_membership_readback");
}
Json common_record(const Request& r, const char* schema) {
    return {{"schema",schema},{"request_sha256",r.sha},{"session_name",utf8(r.config.name)},{"session_guid",guid_text(r.config.guid)},
        {"start_qpc",r.config.start_qpc},{"deadline_qpc",r.config.deadline_qpc},{"qpc_frequency",r.config.qpc_frequency},
        {"process_id",GetCurrentProcessId()},{"creation_filetime",creation_time(GetCurrentProcess())},{"provenance",r.provenance},
        {"diagnostic_only",true},{"cost_qualification",false},{"live_ready",false},{"qualification_issued",false},{"device_operations",0},
        {"tool_source_commit",PAS_COLD_SCHEDULER_TOOL_SOURCE_COMMIT},{"tool_source_dirty",PAS_COLD_SCHEDULER_TOOL_SOURCE_DIRTY!=0},
        {"logging_mode","sequential"}};
}
void record_matches(const Json& j, const Request& r, const char* schema) {
    require(text_value(j,"schema")==schema && text_value(j,"request_sha256",64)==r.sha && text_value(j,"session_name")==utf8(r.config.name) &&
        text_value(j,"session_guid",36)==guid_text(r.config.guid) && integer(j,"start_qpc")==r.config.start_qpc && integer(j,"deadline_qpc")==r.config.deadline_qpc &&
        integer(j,"qpc_frequency")==r.config.qpc_frequency && j.at("provenance")==r.provenance,"record_request_join");
    flag(j,"diagnostic_only",true); flag(j,"cost_qualification",false); flag(j,"live_ready",false); flag(j,"qualification_issued",false); require(integer(j,"device_operations")==0,"record_device");
    flag(j,"tool_source_dirty",false); require(text_value(j,"tool_source_commit",40)==PAS_COLD_SCHEDULER_TOOL_SOURCE_COMMIT && text_value(j,"logging_mode")=="sequential","record_tool_join");
}
Snapshot owner_snapshot(const Json& j, const Request& r) {
    record_matches(j,r,"pas.cold-scheduler-trace-owner.v1"); flag(j,"successful_start",true);
    const auto& s=j.at("instance"); Snapshot v{}; v.handle=integer(s,"handle"); v.guid=parse_guid(text_value(s,"guid",36));
    v.name=wide(text_value(s,"session_name")); v.file=wide(text_value(s,"file")); v.logger_thread=integer(s,"logger_thread_id");
    require(identity_matches(v,r.config,v.handle),"owner_instance_join"); return v;
}
Handle* pin_live_process(const Json& record) {
    const auto pid=integer(record,"process_id"); require(pid>0 && pid<=MAXDWORD && pid!=GetCurrentProcessId(),"sibling_pid");
    auto h=std::make_unique<Handle>(OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION|SYNCHRONIZE,FALSE,static_cast<DWORD>(pid)));
    require(h->value && creation_time(h->value)==integer(record,"creation_filetime") && WaitForSingleObject(h->value,0)==WAIT_TIMEOUT,"sibling_identity_or_liveness");
    std::array<wchar_t,1024> other{},own{}; DWORD count=static_cast<DWORD>(other.size());
    const auto own_count=GetModuleFileNameW(nullptr,own.data(),static_cast<DWORD>(own.size()));
    require(own_count>0 && own_count<own.size() && QueryFullProcessImageNameW(h->value,0,other.data(),&count) &&
        _wcsicmp(other.data(),own.data())==0,"sibling_executable_identity");
    return h.release();
}
void ensure_root(const Request& r) {
    if(!fs::exists(r.root)) require(CreateDirectoryW(r.root.c_str(),nullptr),"artifact_root_create");
    reject_reparse_chain(r.root);
}
bool etl_bound(const Request& r) {
    WIN32_FILE_ATTRIBUTE_DATA a{};
    if(!GetFileAttributesExW(r.config.file.c_str(),GetFileExInfoStandard,&a)) return GetLastError()==ERROR_FILE_NOT_FOUND;
    return !(a.dwFileAttributes&(FILE_ATTRIBUTE_DIRECTORY|FILE_ATTRIBUTE_REPARSE_POINT)) && ((static_cast<std::uint64_t>(a.nFileSizeHigh)<<32)|a.nFileSizeLow)<=file_cap;
}
void wait_slice(const Config& c) {
    const auto now=qpc(); if(now>=c.deadline_qpc) return;
    const auto left=c.deadline_qpc-now; const auto ms=std::min<std::uint64_t>(100,std::max<std::uint64_t>(1,left/(c.qpc_frequency/1000+1)));
    Sleep(static_cast<DWORD>(ms));
}
Json session_record(const Request& r, const Session& s, const char* schema) {
    auto j=common_record(r,schema); j["successful_start"]=s.started; j["instance"]=snapshot_json(s.latest);
    j["reason"]=s.reason; j["start_status"]=s.start_status; j["query_status"]=s.query_status; j["stop_status"]=s.stop_status;
    j["cleanup"]=s.cleanup; j["cleanup_confirmed"]=s.cleanup_confirmed; j["foreign_sessions_untouched"]=s.foreign_untouched;
    j["final_statistics_verified"]=s.final_statistics_verified;
    j["observed_qpc"]=qpc(); j["ttl_overrun"]=qpc()>r.config.deadline_qpc;
    j["no_realtime_api_completion_guarantee"]=true; j["start_to_owner_publication_crash_window"]=true; return j;
}
int preflight(const Request& r) {
    const bool outer_job=in_job(); Handle job; bool memory_job_verified=false; std::string job_failure="none";
    try { bound_own_job(job); memory_job_verified=true; }
    catch(const std::exception& e) { job_failure=std::string(e.what()).substr(0,128); }
    WindowsApi api; std::vector<Snapshot> active;
    const auto all_status=memory_job_verified?api.all(active):ERROR_INVALID_FUNCTION; bool collision=false;
    for(const auto& s:active) collision|=IsEqualGUID(s.guid,r.config.guid)||_wcsicmp(s.name.c_str(),r.config.name.c_str())==0;
    Snapshot named{}; const auto name_status=memory_job_verified?api.by_name(r.config.name,named):ERROR_INVALID_FUNCTION;
    auto j=common_record(r,"pas.cold-scheduler-trace-preflight.v1"); j["rights"]=rights(); j["in_outer_job"]=outer_job;
    j["outer_job"]=outer_job; j["nested_memory_job_verified"]=memory_job_verified; j["own_memory_job_verified"]=memory_job_verified;
    j["memory_limit_bytes"]=256ull*1024*1024; j["memory_job_failure"]=job_failure;
    j["outer_job_termination_isolated"]=false; j["session_query_attempted"]=memory_job_verified;
    j["all_query_status"]=all_status; j["name_query_status"]=name_status; j["collision"]=collision||name_status==ERROR_SUCCESS;
    j["start_operations"]=0; j["stop_operations"]=0; j["run_ready"]=false;
    j["query_clear"]=memory_job_verified && all_status==ERROR_SUCCESS && name_status==ERROR_WMI_INSTANCE_NOT_FOUND && !collision;
    ensure_root(r); create_json(r.root/L"preflight.json",j); return j.at("query_clear").get<bool>()?0:2;
}
int controller(const Request& r) {
    const bool outer_job=in_job(); Handle job; bound_own_job(job); ensure_root(r);
    require(!fs::exists(r.config.file) && !fs::exists(r.root/L"owner.json") && !fs::exists(r.root/L"controller-report.json") && !fs::exists(r.root/L"stop.json"),"recording_paths_not_fresh");
    auto watcher=parse_json(read_small(r.root/L"watchdog-ready.json")); record_matches(watcher,r,"pas.cold-scheduler-trace-watchdog-ready.v1");
    flag(watcher,"own_memory_job_verified",true); flag(watcher,"nested_memory_job_verified",true);
    require(integer(watcher,"memory_limit_bytes")==256ull*1024*1024,"watchdog_memory_bound");
    std::unique_ptr<Handle> watcher_handle(pin_live_process(watcher));
    WindowsApi api; Session s(api,r.config);
    try {
        require(qpc()<r.config.deadline_qpc,"deadline_before_start");
        if(s.begin()) {
            auto owner=session_record(r,s,"pas.cold-scheduler-trace-owner.v1"); owner["controller_memory_limit_bytes"]=256ull*1024*1024;
            owner["outer_job"]=outer_job; owner["nested_memory_job_verified"]=true; owner["outer_job_termination_isolated"]=false;
            owner["watchdog_process_id"]=integer(watcher,"process_id"); owner["watchdog_creation_filetime"]=integer(watcher,"creation_filetime");
            create_json(r.root/L"owner.json",owner);
            require(qpc()<r.config.deadline_qpc,"startup_deadline");
            auto ready=owner; ready["schema"]="pas.cold-scheduler-trace-ready.v1"; create_json(r.root/L"ready.json",ready);
            while(qpc()<r.config.deadline_qpc) {
                if(WaitForSingleObject(watcher_handle->value,0)!=WAIT_TIMEOUT) { s.reason="watchdog_exited"; break; }
                if(!s.inspect() || !etl_bound(r)) { if(!etl_bound(r)) s.reason="raw_file_bound"; break; }
                if(fs::exists(r.root/L"stop.json")) {
                    auto stop=parse_json(read_small(r.root/L"stop.json",4096));
                    require(text_value(stop,"schema")=="pas.cold-scheduler-trace-stop.v1" && text_value(stop,"request_sha256",64)==r.sha &&
                        text_value(stop,"session_guid",36)==guid_text(r.config.guid) && integer(stop,"handle")==s.handle(),"stop_ticket_mismatch");
                    s.reason="requested_stop"; break;
                }
                wait_slice(r.config);
            }
            if(qpc()>=r.config.deadline_qpc) s.reason="shared_deadline";
        }
    } catch(const std::exception& e) { s.reason=std::string(e.what()).substr(0,128); }
    s.finish(); auto j=session_record(r,s,"pas.cold-scheduler-trace-controller-report.v1"); j["raw_file_within_cap"]=etl_bound(r);
    j["outer_job"]=outer_job; j["nested_memory_job_verified"]=true; j["outer_job_termination_isolated"]=false;
    create_json(r.root/L"controller-report.json",j);
    return s.cleanup_confirmed && s.final_statistics_verified && loss_free(s.latest) && etl_bound(r) && s.reason=="requested_stop"?0:2;
}
int watchdog(const Request& r) {
    const bool outer_job=in_job(); Handle job; bound_own_job(job); ensure_root(r);
    require(!fs::exists(r.root/L"watchdog-ready.json") && !fs::exists(r.root/L"owner.json") && !fs::exists(r.root/L"watchdog-report.json") && !fs::exists(r.config.file),"watchdog_paths_not_fresh");
    auto ready=common_record(r,"pas.cold-scheduler-trace-watchdog-ready.v1"); ready["own_memory_job_verified"]=true; ready["memory_limit_bytes"]=256ull*1024*1024;
    ready["outer_job"]=outer_job; ready["nested_memory_job_verified"]=true; ready["outer_job_termination_isolated"]=false;
    create_json(r.root/L"watchdog-ready.json",ready);
    WindowsApi api; Session s(api,r.config); std::unique_ptr<Handle> owner_process; bool owner_seen=false;
    try {
        while(qpc()<r.config.deadline_qpc) {
            if(!owner_seen && fs::exists(r.root/L"owner.json")) {
                const auto j=parse_json(read_small(r.root/L"owner.json")); const auto owner=owner_snapshot(j,r);
                require(integer(j,"watchdog_process_id")==GetCurrentProcessId() && integer(j,"watchdog_creation_filetime")==creation_time(GetCurrentProcess()),"watchdog_owner_join");
                owner_seen=true;
                if(!s.adopt(owner)) break;
                owner_process.reset(pin_live_process(j));
            }
            if(owner_seen) {
                if(WaitForSingleObject(owner_process->value,0)!=WAIT_TIMEOUT) { s.reason="controller_exited"; break; }
                if(!s.inspect() || !etl_bound(r)) { if(!etl_bound(r)) s.reason="raw_file_bound"; break; }
                if(fs::exists(r.root/L"controller-report.json")) { s.reason="controller_completed"; break; }
            }
            wait_slice(r.config);
        }
        if(qpc()>=r.config.deadline_qpc) s.reason=owner_seen?"shared_deadline":"no_published_owner_before_deadline";
    } catch(const std::exception& e) { s.reason=std::string(e.what()).substr(0,128); }
    if(owner_seen) s.finish();
    auto j=session_record(r,s,"pas.cold-scheduler-trace-watchdog-report.v1"); j["owner_seen"]=owner_seen; j["memory_limit_bytes"]=256ull*1024*1024;
    j["outer_job"]=outer_job; j["nested_memory_job_verified"]=true; j["outer_job_termination_isolated"]=false;
    j["no_owner_no_stop"]=!owner_seen; j["run_ready"]=false; create_json(r.root/L"watchdog-report.json",j);
    return owner_seen && s.cleanup_confirmed?0:2;
}
} // namespace pas::cold_scheduler

#if !defined(PAS_COLD_SCHEDULER_TRACE_NO_MAIN) && !defined(COLD_SCHEDULER_TRACE_NO_MAIN)
int wmain(int argc,wchar_t** argv) {
    try {
        pas::cold_scheduler::require(argc==3,"usage: --preflight|--controller|--watchdog request.json");
        const auto r=pas::cold_scheduler::load_request(argv[2]); const std::wstring mode=argv[1];
        if(mode==L"--preflight") return pas::cold_scheduler::preflight(r);
        if(mode==L"--controller") return pas::cold_scheduler::controller(r);
        if(mode==L"--watchdog") return pas::cold_scheduler::watchdog(r);
        throw std::runtime_error("unknown_mode");
    } catch(const std::exception& e) {
        // Fixed-size error text. A request which cannot be trusted chooses no output path.
        std::fprintf(stderr,"cold_scheduler_trace: %.128s\n",e.what()); return 2;
    }
}
#endif
