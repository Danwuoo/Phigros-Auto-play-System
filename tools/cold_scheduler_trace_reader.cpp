// Independent, offline Windows C++20 diagnostic. No runtime/CMake integration.
// Usage (ticks are absolute QueryPerformanceCounter values, not xperf offsets):
//   cold_scheduler_trace_reader --etl stopped.etl --target-tid 1234
//     --begin-qpc 123456 --end-qpc 234567 --qpc-frequency 10000000
//     --output fresh-summary.json
// Pure bounded state controls: --self-test --output fresh-controls.json
// Output is CreateNew; exit 0 = usable observations (possibly partial UNKNOWN),
// 2 = UNKNOWN trace contract, 1 = CLI/input/output error. None grants qualification.
// Link only Windows SDK Advapi32.lib and Tdh.lib. Do not start/stop an ETW session.
// Official contracts used, accessed 2026-10-11:
// https://learn.microsoft.com/en-us/windows/win32/api/evntrace/nf-evntrace-opentracew
// https://learn.microsoft.com/en-us/windows/win32/api/evntrace/nf-evntrace-processtrace
// https://learn.microsoft.com/en-us/windows/win32/etw/wnode-header
// https://learn.microsoft.com/en-us/windows/win32/api/evntrace/ns-evntrace-trace_logfile_header
// https://learn.microsoft.com/en-us/windows/win32/etw/thread-v2
// https://learn.microsoft.com/en-us/windows/win32/etw/cswitch
// https://learn.microsoft.com/en-us/windows/win32/etw/readythread
// https://learn.microsoft.com/en-us/windows/win32/etw/lost-event
// https://learn.microsoft.com/en-us/windows/win32/etw/rt-lostevent
// https://learn.microsoft.com/en-us/windows/win32/api/tdh/nf-tdh-tdhgeteventinformation
// https://learn.microsoft.com/en-us/windows/win32/api/tdh/ns-tdh-event_property_info
// https://learn.microsoft.com/en-us/windows/win32/api/tdh/nf-tdh-tdhgetproperty
// https://learn.microsoft.com/en-us/windows/win32/etw/logging-mode-constants

#ifndef _WIN32
#error cold_scheduler_trace_reader requires the Windows SDK
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <evntrace.h>
#include <evntcons.h>
#include <tdh.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <initializer_list>
#include <limits>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>

#pragma comment(lib, "advapi32.lib")
#pragma comment(lib, "tdh.lib")

#ifndef PAS_COLD_SCHEDULER_TOOL_SOURCE_COMMIT
#define PAS_COLD_SCHEDULER_TOOL_SOURCE_COMMIT "UNPINNED"
#endif
#ifndef PAS_COLD_SCHEDULER_TOOL_SOURCE_DIRTY
#define PAS_COLD_SCHEDULER_TOOL_SOURCE_DIRTY 1
#endif

namespace {
using Tick = std::int64_t;
constexpr std::size_t kSummaryBytes = 1024 * 1024;
constexpr std::size_t kTargetRecords = 262144;
constexpr std::size_t kSchemaBytes = 64 * 1024;
constexpr std::uint64_t kFileBytes = 128ULL * 1024 * 1024;
constexpr SIZE_T kJobMemoryBytes = 512ULL * 1024 * 1024;
constexpr std::uint64_t kAllRecords = 32000000;
constexpr std::uint64_t kBuffers = 65536;
constexpr ULONG kBufferBytes = 1024 * 1024;
constexpr std::size_t kPathChars = 32767;
constexpr std::size_t kTopIntervals = 32;
constexpr std::uint64_t kWindowSeconds = 75;
constexpr GUID kThreadGuid{0x3d6fa8d1, 0xfe05, 0x11d0,
                          {0x9d, 0xda, 0x00, 0xc0, 0x4f, 0xd7, 0xba, 0x7c}};
constexpr GUID kLostGuid{0x6a399ae0, 0x4bc6, 0x4de9,
                        {0x87, 0x0b, 0x36, 0x57, 0xf8, 0x94, 0x7e, 0x7e}};

enum class Issue : unsigned {
    OpenTrace, ClockNotQpc, FrequencyMismatch, NotFinalized, CircularCoverage,
    AppendOrMultipleFiles, MissingHeader, EventLoss, BufferLoss, LostRecord,
    BufferCap, RecordCap, TargetCap, BufferSize, UnsupportedVersion,
    TdhSchema, TdhProperty, InvalidTimestamp, MissingCoverage,
    MissingEventClasses, ProcessTrace, CallbackException, InputChanged,
    SchemaChanged, InvalidProcessor, CloseTrace, Count
};
constexpr std::array<const char*, static_cast<std::size_t>(Issue::Count)> kIssues{
    "open_trace_failed", "clock_is_not_raw_qpc", "perf_frequency_mismatch",
    "etl_not_finalized", "circular_or_buffering_coverage_unproven",
    "append_or_multifile_trace_unsupported", "missing_or_invalid_log_header",
    "events_lost", "buffers_lost", "explicit_lost_event_or_buffer_or_file",
    "buffer_count_cap", "record_count_cap", "target_record_cap",
    "trace_buffer_size_cap", "scheduler_version_not_documented_v2",
    "tdh_schema_unavailable_or_invalid", "tdh_scalar_property_invalid",
    "invalid_raw_qpc_timestamp", "scheduler_events_do_not_bracket_window",
    "missing_cswitch_or_readythread_schema", "process_trace_did_not_reach_eof",
    "callback_exception", "etl_identity_changed", "scheduler_schema_changed",
    "scheduler_processor_index_invalid", "close_trace_failed"
};

struct Options {
    std::wstring etl;
    std::wstring output;
    ULONG target{};
    Tick begin{};
    Tick end{};
    Tick frequency{};
};

class Handle {
public:
    explicit Handle(HANDLE value = INVALID_HANDLE_VALUE) : value_(value) {}
    ~Handle() { if (value_ != INVALID_HANDLE_VALUE && value_ != nullptr) CloseHandle(value_); }
    Handle(const Handle&) = delete;
    Handle& operator=(const Handle&) = delete;
    HANDLE get() const { return value_; }
private:
    HANDLE value_;
};

[[noreturn]] void fail(const char* message) { throw std::runtime_error(message); }

std::uint64_t decimal(std::wstring_view text) {
    if (text.empty() || text.size() > 20) fail("invalid unsigned decimal argument");
    std::uint64_t value = 0;
    for (const auto c : text) {
        if (c < L'0' || c > L'9') fail("decimal arguments must contain digits only");
        const auto digit = static_cast<std::uint64_t>(c - L'0');
        if (value > (std::numeric_limits<std::uint64_t>::max() - digit) / 10)
            fail("decimal argument overflow");
        value = value * 10 + digit;
    }
    return value;
}

Options arguments(int argc, wchar_t** argv) {
    if (argc != 13) fail("expected --etl --target-tid --begin-qpc --end-qpc --qpc-frequency --output pairs");
    Options result;
    unsigned seen = 0;
    for (int i = 1; i < argc; i += 2) {
        const std::wstring_view key(argv[i]), value(argv[i + 1]);
        unsigned bit = 0;
        if (key == L"--etl") { bit = 1; result.etl = value; }
        else if (key == L"--output") { bit = 2; result.output = value; }
        else if (key == L"--target-tid") {
            bit = 4; const auto n = decimal(value);
            if (n == 0 || n > std::numeric_limits<ULONG>::max()) fail("target TID out of range");
            result.target = static_cast<ULONG>(n);
        } else {
            if (key == L"--begin-qpc") bit = 8;
            else if (key == L"--end-qpc") bit = 16;
            else if (key == L"--qpc-frequency") bit = 32;
            else fail("unknown argument");
            const auto n = decimal(value);
            if (n == 0 || n > static_cast<std::uint64_t>(std::numeric_limits<Tick>::max()))
                fail("QPC argument out of range");
            if (bit == 8) result.begin = static_cast<Tick>(n);
            else if (bit == 16) result.end = static_cast<Tick>(n);
            else result.frequency = static_cast<Tick>(n);
        }
        if (seen & bit) fail("duplicate argument");
        seen |= bit;
    }
    if (seen != 63 || result.etl.empty() || result.output.empty() ||
        result.etl.size() > kPathChars || result.output.size() > kPathChars)
        fail("missing or oversized path argument");
    if (result.begin >= result.end) fail("QPC interval must be nonempty and increasing");
    const auto duration = static_cast<std::uint64_t>(result.end - result.begin);
    const auto frequency = static_cast<std::uint64_t>(result.frequency);
    if (duration / frequency > kWindowSeconds ||
        (duration / frequency == kWindowSeconds && duration % frequency != 0))
        fail("reader window exceeds 75 second diagnostic input cap");
    return result;
}

std::uint64_t file_size(const BY_HANDLE_FILE_INFORMATION& info) {
    return (static_cast<std::uint64_t>(info.nFileSizeHigh) << 32) | info.nFileSizeLow;
}
bool same_file(const BY_HANDLE_FILE_INFORMATION& a, const BY_HANDLE_FILE_INFORMATION& b) {
    return a.dwVolumeSerialNumber == b.dwVolumeSerialNumber &&
           a.nFileIndexHigh == b.nFileIndexHigh && a.nFileIndexLow == b.nFileIndexLow &&
           file_size(a) == file_size(b) &&
           a.ftLastWriteTime.dwHighDateTime == b.ftLastWriteTime.dwHighDateTime &&
           a.ftLastWriteTime.dwLowDateTime == b.ftLastWriteTime.dwLowDateTime;
}

enum class Kind : std::uint8_t { In, Out, Ready, Contradiction };
enum class State : std::uint8_t { Running, Ready, Waiting, Unknown };
constexpr std::array<const char*, 4> kStates{"scheduled_running", "ready", "waiting", "unknown"};
struct Record {
    Tick tick{};
    ULONG cpu{};
    Kind kind{};
    std::int8_t old_state{};
    std::uint8_t ready_flags{};
};
static_assert(sizeof(Record) <= 24);

struct Schema {
    bool checked{};
    EVENT_DESCRIPTOR descriptor{};
    USHORT bitness{};
    ULONG property_count{};
    ULONG schema_bytes{};
};

struct Context {
    const Options& options;
    std::unique_ptr<Record[]> records{std::make_unique<Record[]>(kTargetRecords)};
    // Only two documented classes. No unbounded schema cache or event payloads.
    std::array<Schema, 2> schemas{};
    alignas(TRACE_EVENT_INFO) std::array<std::byte, kSchemaBytes> schema_buffer{};
    std::size_t count{};
    std::uint64_t issues{};
    std::uint64_t callbacks{};
    std::uint64_t buffer_callbacks{};
    std::uint64_t scheduler_records{};
    std::uint64_t window_scheduler_records{};
    std::uint64_t cswitch_records{};
    std::uint64_t ready_records{};
    std::uint64_t lost_records{};
    std::uint64_t input_order_regressions{};
    Tick previous_scheduler_tick{};
    Tick first_scheduler_tick{std::numeric_limits<Tick>::max()};
    Tick last_scheduler_tick{};
    ULONG max_events_lost{};
    ULONG max_buffers_lost{};
    ULONG max_consumer_events_lost{};
    ULONG max_buffers_read{};
    ULONG first_tdh_error{};
    ULONG process_status{ERROR_INVALID_FUNCTION};
    ULONG open_error{};
    ULONG close_status{ERROR_INVALID_FUNCTION};
    TRACE_LOGFILE_HEADER header{};
    bool opened{};
    bool processed{};
    bool input_stable{};
    bool stop{};
    bool own_job_verified{};
    bool already_in_outer_job{};
    std::wstring resolved_etl;

    explicit Context(const Options& value) : options(value) {}
    void mark(Issue issue) noexcept { issues |= 1ULL << static_cast<unsigned>(issue); }
    void abort(Issue issue) noexcept { mark(issue); stop = true; }
    void tdh_error(ULONG error, Issue issue) noexcept {
        if (first_tdh_error == 0) first_tdh_error = error;
        abort(issue);
    }
    void observe_header(const EVENT_TRACE_LOGFILEW& log) noexcept {
        max_events_lost = std::max(max_events_lost, log.LogfileHeader.EventsLost);
        max_buffers_lost = std::max(max_buffers_lost, log.LogfileHeader.BuffersLost);
        max_consumer_events_lost = std::max(max_consumer_events_lost, log.EventsLost);
        max_buffers_read = std::max(max_buffers_read, log.BuffersRead);
        if (max_events_lost != 0 || max_consumer_events_lost != 0) mark(Issue::EventLoss);
        if (max_buffers_lost != 0) mark(Issue::BufferLoss);
        if (log.BufferSize > kBufferBytes || log.LogfileHeader.BufferSize > kBufferBytes)
            abort(Issue::BufferSize);
        if (log.LogfileHeader.ReservedFlags != header.ReservedFlags ||
            log.LogfileHeader.PerfFreq.QuadPart != header.PerfFreq.QuadPart)
            abort(Issue::FrequencyMismatch);
    }
    void push(Record record) noexcept {
        if (count == kTargetRecords) { abort(Issue::TargetCap); return; }
        records[count++] = record;
    }
};

// Bound the complete offline reader, including TDH/ETW SDK allocations. A
// restrictive outer guard job is retained; failed nested assignment is an input
// STOP, never a request for breakaway or a relaxed limit. No kill-on-close.
void own_job(Handle& job, Context& context) {
    BOOL outer = FALSE;
    if (!IsProcessInJob(GetCurrentProcess(), nullptr, &outer)) fail("cannot query existing job membership");
    context.already_in_outer_job = outer != FALSE;
    JOBOBJECT_EXTENDED_LIMIT_INFORMATION limit{};
    limit.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_PROCESS_MEMORY | JOB_OBJECT_LIMIT_JOB_MEMORY;
    limit.ProcessMemoryLimit = kJobMemoryBytes;
    limit.JobMemoryLimit = kJobMemoryBytes;
    if (job.get() == nullptr || job.get() == INVALID_HANDLE_VALUE ||
        !SetInformationJobObject(job.get(), JobObjectExtendedLimitInformation, &limit, sizeof(limit)) ||
        !AssignProcessToJobObject(job.get(), GetCurrentProcess())) fail("cannot establish bounded reader job; no trace consumed");
    JOBOBJECT_EXTENDED_LIMIT_INFORMATION actual{};
    if (!QueryInformationJobObject(job.get(), JobObjectExtendedLimitInformation, &actual, sizeof(actual), nullptr) ||
        actual.BasicLimitInformation.LimitFlags != limit.BasicLimitInformation.LimitFlags ||
        actual.ProcessMemoryLimit != kJobMemoryBytes || actual.JobMemoryLimit != kJobMemoryBytes)
        fail("reader job limit readback mismatch; no trace consumed");
    context.own_job_verified = true;
}

struct Field { const wchar_t* name; USHORT type; ULONG bytes; };
constexpr std::array<Field, 3> kSwitchFields{{
    {L"NewThreadId", TDH_INTYPE_UINT32, 4},
    {L"OldThreadId", TDH_INTYPE_UINT32, 4},
    {L"OldThreadState", TDH_INTYPE_INT8, 1}
}};
constexpr std::array<Field, 2> kReadyFields{{
    {L"TThreadId", TDH_INTYPE_UINT32, 4}, {L"Flag", TDH_INTYPE_INT8, 1}
}};

bool property_name(const std::byte* base, ULONG bytes, ULONG offset,
                   std::wstring_view expected) noexcept {
    if (offset == 0 || offset % alignof(wchar_t) != 0 || offset >= bytes) return false;
    const auto available = (bytes - offset) / sizeof(wchar_t);
    if (expected.size() >= available) return false;
    const auto* name = reinterpret_cast<const wchar_t*>(base + offset);
    return std::equal(expected.begin(), expected.end(), name) && name[expected.size()] == L'\0';
}

template<std::size_t N>
bool schema(Context& context, EVENT_RECORD& event, Schema& cache,
            const std::array<Field, N>& fields) {
    const auto& descriptor = event.EventHeader.EventDescriptor;
    const auto bitness = static_cast<USHORT>(event.EventHeader.Flags &
        (EVENT_HEADER_FLAG_32_BIT_HEADER | EVENT_HEADER_FLAG_64_BIT_HEADER));
    if (descriptor.Version != 2) { context.abort(Issue::UnsupportedVersion); return false; }
    if ((event.EventHeader.Flags & EVENT_HEADER_FLAG_CLASSIC_HEADER) == 0 ||
        (event.EventHeader.Flags & (EVENT_HEADER_FLAG_TRACE_MESSAGE | EVENT_HEADER_FLAG_STRING_ONLY)) != 0) {
        context.abort(Issue::TdhSchema); return false;
    }
    if (cache.checked) {
        if (std::memcmp(&cache.descriptor, &descriptor, sizeof(descriptor)) != 0 || cache.bitness != bitness) {
            context.abort(Issue::SchemaChanged); return false;
        }
        return true;
    }
    ULONG bytes = 0;
    ULONG error = TdhGetEventInformation(&event, 0, nullptr, nullptr, &bytes);
    constexpr auto minimum = offsetof(TRACE_EVENT_INFO, EventPropertyInfoArray);
    if (error != ERROR_INSUFFICIENT_BUFFER || bytes < minimum || bytes > kSchemaBytes) {
        context.tdh_error(error, Issue::TdhSchema); return false;
    }
    auto* info = reinterpret_cast<TRACE_EVENT_INFO*>(context.schema_buffer.data());
    error = TdhGetEventInformation(&event, 0, nullptr, info, &bytes);
    if (error != ERROR_SUCCESS || bytes < minimum || bytes > kSchemaBytes) {
        context.tdh_error(error, Issue::TdhSchema); return false;
    }
    if (info->DecodingSource != DecodingSourceWbem || !IsEqualGUID(info->ProviderGuid, kThreadGuid) ||
        info->EventDescriptor.Version != descriptor.Version ||
        info->EventDescriptor.Opcode != descriptor.Opcode ||
        info->TopLevelPropertyCount > info->PropertyCount || info->PropertyCount > 64 ||
        info->PropertyCount > (bytes - minimum) / sizeof(EVENT_PROPERTY_INFO)) {
        context.abort(Issue::TdhSchema); return false;
    }
    for (const auto& required : fields) {
        unsigned matches = 0;
        for (ULONG i = 0; i < info->TopLevelPropertyCount; ++i) {
            const auto& property = info->EventPropertyInfoArray[i];
            if (!property_name(context.schema_buffer.data(), bytes, property.NameOffset, required.name)) continue;
            ++matches;
            constexpr unsigned forbidden = PropertyStruct | PropertyParamLength | PropertyParamCount |
                PropertyParamFixedCount | PropertyHasCustomSchema;
            if ((static_cast<unsigned>(property.Flags) & forbidden) != 0 || property.count != 1 ||
                property.nonStructType.InType != required.type ||
                (property.length != 0 && property.length != required.bytes)) {
                context.abort(Issue::TdhSchema); return false;
            }
        }
        if (matches != 1) { context.abort(Issue::TdhSchema); return false; }
    }
    cache = {true, descriptor, bitness, info->PropertyCount, bytes};
    return true;
}

template<typename T>
bool scalar(Context& context, EVENT_RECORD& event, const wchar_t* name, T& value) {
    PROPERTY_DATA_DESCRIPTOR descriptor{};
    descriptor.PropertyName = static_cast<ULONGLONG>(reinterpret_cast<ULONG_PTR>(name));
    descriptor.ArrayIndex = std::numeric_limits<ULONG>::max();
    ULONG bytes = 0;
    ULONG error = TdhGetPropertySize(&event, 0, nullptr, 1, &descriptor, &bytes);
    if (error != ERROR_SUCCESS || bytes != sizeof(T)) {
        context.tdh_error(error, Issue::TdhProperty); return false;
    }
    error = TdhGetProperty(&event, 0, nullptr, 1, &descriptor, bytes,
                           reinterpret_cast<PBYTE>(&value));
    if (error != ERROR_SUCCESS) { context.tdh_error(error, Issue::TdhProperty); return false; }
    return true;
}

void consume(Context& context, EVENT_RECORD& event) {
    if (context.stop) return;
    if (++context.callbacks > kAllRecords) { context.abort(Issue::RecordCap); return; }
    if (IsEqualGUID(event.EventHeader.ProviderId, kLostGuid)) {
        const auto opcode = event.EventHeader.EventDescriptor.Opcode;
        if (opcode >= 32 && opcode <= 34) { ++context.lost_records; context.mark(Issue::LostRecord); }
        return;
    }
    if (!IsEqualGUID(event.EventHeader.ProviderId, kThreadGuid)) return;
    const auto opcode = event.EventHeader.EventDescriptor.Opcode;
    if (opcode != 36 && opcode != 50) return;
    ++context.scheduler_records;
    const auto tick = event.EventHeader.TimeStamp.QuadPart;
    if (tick <= 0) { context.abort(Issue::InvalidTimestamp); return; }
    if (context.previous_scheduler_tick != 0 && tick < context.previous_scheduler_tick)
        ++context.input_order_regressions;
    context.previous_scheduler_tick = tick;
    context.first_scheduler_tick = std::min(context.first_scheduler_tick, tick);
    context.last_scheduler_tick = std::max(context.last_scheduler_tick, tick);
    // Only global raw coverage extents are observed outside the requested window.
    // No target state is imported/backfilled from events outside [begin,end].
    if (tick < context.options.begin || tick > context.options.end) return;
    ++context.window_scheduler_records;
    if (opcode == 36) {
        ++context.cswitch_records;
        if (!schema(context, event, context.schemas[0], kSwitchFields)) return;
        ULONG incoming = 0, outgoing = 0;
        if (!scalar(context, event, L"NewThreadId", incoming) ||
            !scalar(context, event, L"OldThreadId", outgoing)) return;
        if (incoming != context.options.target && outgoing != context.options.target) return;
        const auto cpu = GetEventProcessorIndex(&event);
        if (cpu >= context.header.NumberOfProcessors) { context.abort(Issue::InvalidProcessor); return; }
        if (incoming == outgoing) {
            context.push({tick, cpu, Kind::Contradiction, 0, 0}); return;
        }
        if (incoming == context.options.target) context.push({tick, cpu, Kind::In, 0, 0});
        else {
            std::int8_t state = 0;
            if (scalar(context, event, L"OldThreadState", state))
                context.push({tick, cpu, Kind::Out, state, 0});
        }
    } else {
        ++context.ready_records;
        if (!schema(context, event, context.schemas[1], kReadyFields)) return;
        ULONG target = 0;
        if (!scalar(context, event, L"TThreadId", target) || target != context.options.target) return;
        std::int8_t flags = 0;
        if (scalar(context, event, L"Flag", flags))
            context.push({tick, GetEventProcessorIndex(&event), Kind::Ready, 0,
                          static_cast<std::uint8_t>(flags)});
    }
}

void WINAPI event_callback(EVENT_RECORD* event) noexcept {
    if (event == nullptr || event->UserContext == nullptr) return;
    auto& context = *static_cast<Context*>(event->UserContext);
    try { consume(context, *event); }
    catch (...) { context.abort(Issue::CallbackException); }
}

ULONG WINAPI buffer_callback(EVENT_TRACE_LOGFILEW* log) noexcept {
    if (log == nullptr || log->Context == nullptr) return FALSE;
    auto& context = *static_cast<Context*>(log->Context);
    context.observe_header(*log);
    if (++context.buffer_callbacks > kBuffers) context.abort(Issue::BufferCap);
    return context.stop ? FALSE : TRUE;
}

void validate_header(Context& context, const EVENT_TRACE_LOGFILEW& log) {
    context.header = log.LogfileHeader;
    const auto& header = context.header;
    if (header.ReservedFlags != 1) context.mark(Issue::ClockNotQpc);
    if (header.PerfFreq.QuadPart <= 0 || header.PerfFreq.QuadPart != context.options.frequency)
        context.mark(Issue::FrequencyMismatch);
    if (header.EndTime.QuadPart <= 0) context.mark(Issue::NotFinalized);
    if (header.StartTime.QuadPart <= 0 || header.EndTime.QuadPart < header.StartTime.QuadPart ||
        header.NumberOfProcessors == 0 || header.BufferSize == 0 || header.BuffersWritten == 0 ||
        (header.PointerSize != 4 && header.PointerSize != 8)) context.mark(Issue::MissingHeader);
    if (header.LogFileMode & (EVENT_TRACE_FILE_MODE_CIRCULAR | EVENT_TRACE_BUFFERING_MODE))
        context.mark(Issue::CircularCoverage);
    if (header.LogFileMode & (EVENT_TRACE_FILE_MODE_APPEND | EVENT_TRACE_FILE_MODE_NEWFILE))
        context.mark(Issue::AppendOrMultipleFiles);
    context.observe_header(log);
}

void read_etl(Context& context, HANDLE input) {
    EVENT_TRACE_LOGFILEW log{};
    // Consume the final path of the already locked file, not a redirectable
    // reparse alias. No path/origin is inferred from ETL header strings.
    std::array<wchar_t, kPathChars + 1> filename{};
    const auto chars = GetFinalPathNameByHandleW(input, filename.data(),
        static_cast<DWORD>(filename.size()), FILE_NAME_NORMALIZED | VOLUME_NAME_DOS);
    if (chars == 0 || chars >= filename.size()) fail("locked ETL final path unavailable or oversized");
    context.resolved_etl.assign(filename.data(), chars);
    log.LogFileName = context.resolved_etl.data();
    log.ProcessTraceMode = PROCESS_TRACE_MODE_EVENT_RECORD | PROCESS_TRACE_MODE_RAW_TIMESTAMP;
    log.EventRecordCallback = event_callback;
    log.BufferCallback = buffer_callback;
    log.Context = &context;
    auto trace = OpenTraceW(&log);
    if (trace == INVALID_PROCESSTRACE_HANDLE) {
        context.open_error = GetLastError(); context.mark(Issue::OpenTrace); return;
    }
    context.opened = true;
    validate_header(context, log);
    if (context.issues == 0) {
        context.processed = true;
        // These API parameters are FILETIME, so raw QPC endpoints are filtered
        // in the event callback, never supplied as ProcessTrace time parameters.
        context.process_status = ProcessTrace(&trace, 1, nullptr, nullptr);
        context.observe_header(log);
        if (context.process_status != ERROR_SUCCESS || context.stop) context.mark(Issue::ProcessTrace);
    }
    context.close_status = CloseTrace(trace);
    if (context.close_status != ERROR_SUCCESS) context.mark(Issue::CloseTrace);
    if (context.processed) {
        if (context.first_scheduler_tick > context.options.begin ||
            context.last_scheduler_tick < context.options.end) context.mark(Issue::MissingCoverage);
        if (!context.schemas[0].checked || !context.schemas[1].checked)
            context.mark(Issue::MissingEventClasses);
    }
}

struct Interval { Tick begin{}; Tick end{}; State state{State::Unknown}; };
struct Total { std::uint64_t ticks{}; std::uint64_t intervals{}; };
struct Analysis {
    std::array<Total, 4> totals{};
    std::array<Interval, kTopIntervals> top{};
    std::size_t top_count{};
    std::uint64_t ambiguous_timestamp_groups{};
    std::uint64_t contradictions{};
    std::uint64_t unclassified_pairs{};
    std::uint64_t ready_swapped_or_unknown_flags{};
    std::uint64_t unique_target_timestamps{};
    bool partition_exact{};
    void add(Tick begin, Tick end, State state) {
        if (end <= begin) return;
        auto& total = totals[static_cast<std::size_t>(state)];
        total.ticks += static_cast<std::uint64_t>(end - begin); ++total.intervals;
        const Interval value{begin, end, state};
        std::size_t position = 0;
        while (position < top_count && top[position].end - top[position].begin >= end - begin) ++position;
        if (position == kTopIntervals) return;
        const auto limit = std::min(top_count, kTopIntervals - 1);
        for (auto i = limit; i > position; --i) top[i] = top[i - 1];
        top[position] = value; top_count = std::min(top_count + 1, kTopIntervals);
    }
};

State after(const Record& record) {
    if (record.kind == Kind::In) return State::Running;
    if (record.kind == Kind::Out) {
        if (record.old_state == 1) return State::Ready;
        if (record.old_state == 5) return State::Waiting;
        // Initialized/Running/Standby/Terminated/Transition/DeferredReady are
        // not silently promoted to ready or waiting.
        return State::Unknown;
    }
    if (record.kind == Kind::Ready && (record.ready_flags & ~1U) == 0) return State::Ready;
    // Flag 2/4 denotes swapped-out stack/address space. ReadyThread may repeat
    // after swap-in; this reader conservatively retains UNKNOWN for that span.
    return State::Unknown;
}

State pair_state(const Record& left, const Record& right, Analysis& analysis) {
    const auto state = after(left);
    if (left.kind == Kind::Contradiction || right.kind == Kind::Contradiction) {
        ++analysis.contradictions; return State::Unknown;
    }
    if (right.kind == Kind::Out) {
        if (state == State::Running && left.kind == Kind::In && left.cpu == right.cpu &&
            right.old_state >= 0 && right.old_state <= 7 && right.old_state != 2)
            return State::Running;
        ++analysis.contradictions; return State::Unknown;
    }
    if (right.kind == Kind::In) {
        if (state == State::Ready) return State::Ready;
        if (state == State::Running) ++analysis.contradictions;
        else ++analysis.unclassified_pairs;
        return State::Unknown;
    }
    if (right.kind == Kind::Ready) {
        if (state == State::Waiting) return State::Waiting;
        // Repeat ReadyThread can be documented swap-in, not proof of a second
        // waiting period. Unknown and duplicate states never fabricate time.
        if (state == State::Running) ++analysis.contradictions;
        else ++analysis.unclassified_pairs;
        return State::Unknown;
    }
    ++analysis.unclassified_pairs; return State::Unknown;
}

Analysis analyze(Context& context) {
    Analysis result;
    if (context.issues != 0 || context.count == 0) {
        result.add(context.options.begin, context.options.end, State::Unknown);
    } else {
        std::sort(context.records.get(), context.records.get() + context.count,
                  [](const Record& a, const Record& b) { return a.tick < b.tick; });
        Tick cursor = context.options.begin;
        const Record* previous = nullptr;
        bool previous_ambiguous = false;
        for (std::size_t i = 0; i < context.count;) {
            std::size_t end = i + 1;
            while (end < context.count && context.records[end].tick == context.records[i].tick) ++end;
            const auto& current = context.records[i];
            const bool ambiguous = end - i != 1;
            ++result.unique_target_timestamps;
            if (ambiguous) ++result.ambiguous_timestamp_groups;
            for (std::size_t j = i; j < end; ++j) {
                if (context.records[j].kind == Kind::Ready && (context.records[j].ready_flags & ~1U) != 0)
                    ++result.ready_swapped_or_unknown_flags;
            }
            State state = State::Unknown;
            if (previous != nullptr && !previous_ambiguous && !ambiguous)
                state = pair_state(*previous, current, result);
            result.add(cursor, current.tick, state);
            cursor = current.tick; previous = &current; previous_ambiguous = ambiguous; i = end;
        }
        // No target observation outside the requested window is used to guess
        // the leading/trailing state; a closing transition is required.
        result.add(cursor, context.options.end, State::Unknown);
    }
    std::uint64_t sum = 0;
    for (const auto& total : result.totals) sum += total.ticks;
    result.partition_exact = sum == static_cast<std::uint64_t>(context.options.end - context.options.begin);
    if (!result.partition_exact) fail("internal non-overlapping interval partition failed");
    return result;
}

class Json {
public:
    Json() { bytes_.reserve(kSummaryBytes); }
    void raw(std::string_view value) {
        if (value.size() > kSummaryBytes - bytes_.size()) fail("summary exceeds 1 MiB");
        bytes_.append(value);
    }
    void number(std::uint64_t value) { raw(std::to_string(value)); }
    void boolean(bool value) { raw(value ? "true" : "false"); }
    void quote(std::string_view value) {
        raw("\"");
        constexpr char hex[] = "0123456789abcdef";
        for (const auto byte : value) {
            const auto c = static_cast<unsigned char>(byte);
            if (c == '"') raw("\\\"");
            else if (c == '\\') raw("\\\\");
            else if (c < 0x20) {
                const std::array<char, 6> escape{'\\', 'u', '0', '0', hex[c >> 4], hex[c & 15]};
                raw(std::string_view(escape.data(), escape.size()));
            } else { const char one = byte; raw(std::string_view(&one, 1)); }
        }
        raw("\"");
    }
    void wide(std::wstring_view value) {
        const int size = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, value.data(),
            static_cast<int>(value.size()), nullptr, 0, nullptr, nullptr);
        if (size <= 0 || size > static_cast<int>(4 * kPathChars)) fail("invalid UTF-16 path");
        std::string utf8(static_cast<std::size_t>(size), '\0');
        if (WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, value.data(),
            static_cast<int>(value.size()), utf8.data(), size, nullptr, nullptr) != size)
            fail("path UTF-8 conversion failed");
        quote(utf8);
    }
    const std::string& bytes() const { return bytes_; }
private:
    std::string bytes_;
};

void summary(Json& out, const Context& context, const Analysis& analysis,
             const BY_HANDLE_FILE_INFORMATION& identity) {
    const auto number = [&out](const char* key, std::uint64_t value) {
        out.raw(",\n  "); out.quote(key); out.raw(": "); out.number(value);
    };
    const auto boolean = [&out](const char* key, bool value) {
        out.raw(",\n  "); out.quote(key); out.raw(": "); out.boolean(value);
    };
    out.raw("{\n  \"schema\": \"pas.cold-scheduler-trace-summary.v1\",\n  \"status\": ");
    out.quote(context.issues != 0 ? "UNKNOWN" :
        (analysis.totals[3].ticks == 0 ? "OBSERVED" : "PARTIAL_UNKNOWN"));
    boolean("diagnostic_only", true); boolean("qualification", false);
    boolean("qualification_issued", false); boolean("live_ready", false); number("device_operations", 0);
    out.raw(",\n  \"source_commit\": "); out.quote(PAS_COLD_SCHEDULER_TOOL_SOURCE_COMMIT);
    boolean("source_dirty", PAS_COLD_SCHEDULER_TOOL_SOURCE_DIRTY != 0);
    out.raw(",\n  \"identity\": {\"tool\": \"cold_scheduler_trace_reader\", \"source_commit\": ");
    out.quote(PAS_COLD_SCHEDULER_TOOL_SOURCE_COMMIT);
    out.raw(", \"source_dirty\": "); out.boolean(PAS_COLD_SCHEDULER_TOOL_SOURCE_DIRTY != 0); out.raw("}");
    boolean("causal_attribution_available", false); boolean("tid_lifetime_or_pid_join_verified", false);
    out.raw(",\n  \"interpretation\": \"Scheduled-running is scheduler residency, not proof of instructions executing, no interrupts, or stall cause. Numeric TID identity must be joined externally to the component receipt.\"");
    out.raw(",\n  \"etl\": "); out.wide(context.options.etl);
    out.raw(",\n  \"locked_etl_final_path\": "); out.wide(context.resolved_etl);
    number("etl_bytes", file_size(identity)); number("etl_volume_serial", identity.dwVolumeSerialNumber);
    number("etl_file_index_high", identity.nFileIndexHigh); number("etl_file_index_low", identity.nFileIndexLow);
    number("etl_last_write_filetime", (static_cast<std::uint64_t>(identity.ftLastWriteTime.dwHighDateTime) << 32) |
           identity.ftLastWriteTime.dwLowDateTime);
    boolean("input_opened_read_only_share_read_deny_write_delete", true);
    boolean("input_identity_before_after_equal", context.input_stable);
    boolean("etl_sha256_computed", false);
    boolean("own_job_readback_verified", context.own_job_verified);
    boolean("outer_job_present", context.already_in_outer_job);
    boolean("own_job_kill_on_close", false);
    out.raw(",\n  \"clock_domain\": \"absolute_raw_qpc_ticks\"");
    boolean("relative_origin_inferred", false); number("target_tid", context.options.target);
    number("begin_qpc", static_cast<std::uint64_t>(context.options.begin));
    number("end_qpc", static_cast<std::uint64_t>(context.options.end));
    number("expected_qpc_frequency", static_cast<std::uint64_t>(context.options.frequency));
    number("header_clock_type", context.header.ReservedFlags);
    out.raw(",\n  \"header_perf_frequency\": ");
    if (context.header.PerfFreq.QuadPart > 0) out.number(static_cast<std::uint64_t>(context.header.PerfFreq.QuadPart));
    else out.raw("null");
    number("header_log_mode", context.header.LogFileMode); number("header_buffer_bytes", context.header.BufferSize);
    number("header_processors", context.header.NumberOfProcessors);
    number("header_buffers_written", context.header.BuffersWritten);
    number("header_provider_version", context.header.ProviderVersion);
    number("header_events_lost_max", context.max_events_lost);
    number("header_buffers_lost_max", context.max_buffers_lost);
    number("consumer_events_lost_max", context.max_consumer_events_lost);
    number("explicit_loss_records", context.lost_records);
    boolean("open_trace_succeeded", context.opened); boolean("process_trace_invoked", context.processed);
    number("open_trace_error", context.open_error);
    out.raw(",\n  \"process_trace_status\": ");
    if (context.processed) out.number(context.process_status); else out.raw("null");
    out.raw(",\n  \"close_trace_status\": ");
    if (context.opened) out.number(context.close_status); else out.raw("null");
    number("first_tdh_error", context.first_tdh_error);
    number("all_event_callbacks", context.callbacks); number("buffer_callbacks", context.buffer_callbacks);
    number("buffers_read_max", context.max_buffers_read);
    number("scheduler_records_total", context.scheduler_records);
    number("scheduler_records_in_window", context.window_scheduler_records);
    number("cswitch_records_in_window", context.cswitch_records);
    number("readythread_records_in_window", context.ready_records);
    number("target_records_in_window", context.count);
    number("scheduler_input_order_regressions", context.input_order_regressions);
    out.raw(",\n  \"scheduler_coverage_first_qpc\": ");
    if (context.first_scheduler_tick != std::numeric_limits<Tick>::max())
        out.number(static_cast<std::uint64_t>(context.first_scheduler_tick)); else out.raw("null");
    out.raw(",\n  \"scheduler_coverage_last_qpc\": ");
    if (context.last_scheduler_tick > 0) out.number(static_cast<std::uint64_t>(context.last_scheduler_tick)); else out.raw("null");
    boolean("coverage_verified", context.issues == 0);
    out.raw(",\n  \"coverage_rule\": \"Finalized single sequential file; zero reported loss; ProcessTrace EOF; documented scheduler events bracket raw QPC interval. Circular overwrite continuity is unproven and rejected as UNKNOWN.\"");
    out.raw(",\n  \"issues\": [");
    bool comma = false;
    for (std::size_t i = 0; i < kIssues.size(); ++i) {
        if ((context.issues & (1ULL << i)) == 0) continue;
        if (comma) out.raw(", "); out.quote(kIssues[i]); comma = true;
    }
    out.raw("],\n  \"decoded_schemas\": [");
    for (std::size_t i = 0; i < context.schemas.size(); ++i) {
        if (i) out.raw(", "); const auto& schema = context.schemas[i];
        out.raw("{\"opcode\": "); out.number(i == 0 ? 36 : 50);
        out.raw(", \"validated\": "); out.boolean(schema.checked);
        out.raw(", \"version\": "); if (schema.checked) out.number(schema.descriptor.Version); else out.raw("null");
        out.raw(", \"tdh_bytes\": "); out.number(schema.schema_bytes);
        out.raw(", \"property_count\": "); out.number(schema.property_count);
        out.raw("}");
    }
    out.raw("],\n  \"limits\": {\"etl_bytes\": "); out.number(kFileBytes);
    out.raw(", \"all_records\": "); out.number(kAllRecords);
    out.raw(", \"buffers\": "); out.number(kBuffers);
    out.raw(", \"trace_buffer_bytes\": "); out.number(kBufferBytes);
    out.raw(", \"target_records\": "); out.number(kTargetRecords);
    out.raw(", \"owned_record_storage_bytes\": "); out.number(kTargetRecords * sizeof(Record));
    out.raw(", \"tdh_scratch_bytes\": "); out.number(kSchemaBytes);
    out.raw(", \"summary_bytes\": "); out.number(kSummaryBytes);
    out.raw(", \"own_job_process_and_job_memory_bytes\": "); out.number(kJobMemoryBytes);
    out.raw(", \"max_window_seconds\": "); out.number(kWindowSeconds);
    out.raw(", \"top_intervals\": "); out.number(kTopIntervals);
    out.raw(", \"owned_dynamic_storage_budget_bytes\": 9437184, \"sdk_internal_memory_measured\": false}");
    number("ambiguous_timestamp_groups", analysis.ambiguous_timestamp_groups);
    number("transition_contradictions", analysis.contradictions);
    number("unclassified_transition_pairs", analysis.unclassified_pairs);
    number("ready_swapped_or_unknown_flag_records", analysis.ready_swapped_or_unknown_flags);
    number("unique_target_timestamps", analysis.unique_target_timestamps);
    boolean("nonoverlapping_partition_exact", analysis.partition_exact);
    out.raw(",\n  \"duration_ticks\": {");
    for (std::size_t i = 0; i < kStates.size(); ++i) {
        if (i) out.raw(", "); out.quote(kStates[i]); out.raw(": "); out.number(analysis.totals[i].ticks);
    }
    out.raw("},\n  \"interval_counts\": {");
    for (std::size_t i = 0; i < kStates.size(); ++i) {
        if (i) out.raw(", "); out.quote(kStates[i]); out.raw(": "); out.number(analysis.totals[i].intervals);
    }
    out.raw("},\n  \"top_intervals\": [");
    for (std::size_t i = 0; i < analysis.top_count; ++i) {
        if (i) out.raw(", "); const auto& interval = analysis.top[i];
        out.raw("{\"begin_qpc\": "); out.number(static_cast<std::uint64_t>(interval.begin));
        out.raw(", \"end_qpc\": "); out.number(static_cast<std::uint64_t>(interval.end));
        out.raw(", \"ticks\": "); out.number(static_cast<std::uint64_t>(interval.end - interval.begin));
        out.raw(", \"state\": "); out.quote(kStates[static_cast<std::size_t>(interval.state)]); out.raw("}");
    }
    out.raw("]\n}\n");
}

void write_new(const std::wstring& path, const std::string& bytes) {
    if (bytes.size() > kSummaryBytes) fail("summary size cap");
    Handle output(CreateFileW(path.c_str(), GENERIC_READ | GENERIC_WRITE, 0, nullptr,
                              CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr));
    if (output.get() == INVALID_HANDLE_VALUE) fail("CreateNew summary failed (existing file is never overwritten)");
    DWORD written = 0;
    if (!WriteFile(output.get(), bytes.data(), static_cast<DWORD>(bytes.size()), &written, nullptr) ||
        written != bytes.size() || !FlushFileBuffers(output.get())) fail("summary write/flush failed; partial file retained");
    LARGE_INTEGER zero{};
    if (!SetFilePointerEx(output.get(), zero, nullptr, FILE_BEGIN)) fail("summary readback seek failed");
    // Small fixed readback chunks, not another 1 MiB copy.
    std::array<char, 4096> chunk{};
    for (std::size_t offset = 0; offset < bytes.size();) {
        const auto requested = static_cast<DWORD>(std::min(chunk.size(), bytes.size() - offset));
        DWORD read = 0;
        if (!ReadFile(output.get(), chunk.data(), requested, &read, nullptr) || read != requested ||
            std::memcmp(chunk.data(), bytes.data() + offset, read) != 0) fail("summary readback mismatch; file retained");
        offset += read;
    }
}

int self_test(const std::wstring& output_path) {
    if (output_path.empty() || output_path.size() > kPathChars) fail("self-test output path cap");
    const Options options{L"", output_path, 1234, 100, 200, 10000000};
    Context context(options);
    unsigned assertions = 0;
    std::array<const char*, 20> names{};
    const auto check = [&assertions, &names](bool value, const char* name) {
        if (!value) throw std::runtime_error(std::string("self-test failed: ") + name);
        if (assertions == names.size()) fail("self-test count exceeds fixed cap");
        names[assertions] = name;
        ++assertions;
    };
    const auto evaluate = [&context](std::initializer_list<Record> records) {
        context.count = 0; context.issues = 0; context.stop = false;
        for (const auto& record : records) context.push(record);
        return analyze(context);
    };
    const auto durations = [](const Analysis& value, std::uint64_t running,
                              std::uint64_t ready, std::uint64_t waiting, std::uint64_t unknown) {
        return value.partition_exact && value.totals[0].ticks == running && value.totals[1].ticks == ready &&
               value.totals[2].ticks == waiting && value.totals[3].ticks == unknown;
    };
    check(durations(evaluate({}), 0, 0, 0, 100), "empty trace is wholly unknown");
    check(durations(evaluate({{110, 0, Kind::In}, {150, 0, Kind::Out, 5}}), 40, 0, 0, 60), "running with unknown edges");
    check(durations(evaluate({{110, 0, Kind::Out, 1}, {130, 0, Kind::In}, {170, 0, Kind::Out, 5}}),
                    40, 20, 0, 40), "ready to running");
    check(durations(evaluate({{110, 0, Kind::Out, 5}, {140, 0, Kind::Ready},
                              {160, 0, Kind::In}, {180, 0, Kind::Out, 5}}), 20, 20, 30, 30), "waiting ready running partition");
    check(durations(evaluate({{110, 0, Kind::Out, 5}, {140, 0, Kind::In}}), 0, 0, 0, 100), "missing ReadyThread is unknown");
    const auto double_in = evaluate({{110, 0, Kind::In}, {140, 1, Kind::In}, {170, 1, Kind::Out, 5}});
    check(durations(double_in, 30, 0, 0, 70) && double_in.contradictions == 1, "overlapping switch-in rejected");
    const auto cpu = evaluate({{110, 0, Kind::In}, {160, 1, Kind::Out, 5}});
    check(durations(cpu, 0, 0, 0, 100) && cpu.contradictions == 1, "CPU mismatch cannot double count");
    check(durations(evaluate({{110, 0, Kind::Ready}, {140, 0, Kind::Ready},
                              {160, 0, Kind::In}, {180, 0, Kind::Out, 5}}), 20, 20, 0, 60), "duplicate ready interval unknown");
    const auto ties = evaluate({{110, 0, Kind::In}, {110, 0, Kind::Out, 5}, {140, 0, Kind::Ready},
                                {160, 0, Kind::In}, {180, 0, Kind::Out, 5}});
    check(durations(ties, 20, 20, 0, 60) && ties.ambiguous_timestamp_groups == 1, "timestamp tie has no fabricated order");
    check(durations(evaluate({{170, 0, Kind::Out, 5}, {130, 0, Kind::In}}), 40, 0, 0, 60), "cross-buffer callback order sorted");
    check(durations(evaluate({{110, 0, Kind::Ready, 0, 2}, {140, 0, Kind::In}, {180, 0, Kind::Out, 5}}),
                    40, 0, 0, 60), "stack-swapped ready interval unknown");
    check(durations(evaluate({{110, 0, Kind::Ready, 0, 4}, {140, 0, Kind::In}, {180, 0, Kind::Out, 5}}),
                    40, 0, 0, 60), "address-space-swapped ready interval unknown");
    check(durations(evaluate({{110, 0, Kind::Out, 7}, {140, 0, Kind::In}}), 0, 0, 0, 100), "deferred ready not silently promoted");
    const auto invalid_state = evaluate({{110, 0, Kind::In}, {150, 0, Kind::Out, 2}});
    check(durations(invalid_state, 0, 0, 0, 100) && invalid_state.contradictions == 1, "switch-out still-running contradiction");
    check(durations(evaluate({{100, 0, Kind::In}, {200, 0, Kind::Out, 5}}), 100, 0, 0, 0), "exact running endpoints");
    check(durations(evaluate({{100, 0, Kind::Out, 1}, {200, 0, Kind::In}}), 0, 100, 0, 0), "exact ready endpoints");
    check(durations(evaluate({{100, 0, Kind::Out, 5}, {200, 0, Kind::Ready}}), 0, 0, 100, 0), "exact waiting endpoints");
    evaluate({{100, 0, Kind::In}, {200, 0, Kind::Out, 5}});
    context.mark(Issue::EventLoss);
    check(durations(analyze(context), 0, 0, 0, 100), "loss invalidates attributed durations");
    context.count = kTargetRecords; context.issues = 0; context.stop = false;
    context.push({150, 0, Kind::In});
    check(context.count == kTargetRecords && context.stop &&
          (context.issues & (1ULL << static_cast<unsigned>(Issue::TargetCap))) != 0, "fixed target record cap");
    evaluate({{100, 0, Kind::In}, {200, 0, Kind::Out, 5}});
    context.mark(Issue::ClockNotQpc);
    check(durations(analyze(context), 0, 0, 0, 100), "non-QPC trace has no attributed durations");
    if (assertions != 20) fail("self-test control denominator mismatch");
    Json report;
    report.raw("{\"schema\":\"pas.cold-scheduler-trace-reader-controls.v1\",\"status\":\"PASS\",\"assertions\":");
    report.number(assertions);
    report.raw(",\"pass\":true,\"failed\":0,\"diagnostic_only\":true,\"qualification\":false,\"qualification_issued\":false,\"live_ready\":false,\"device_operations\":0,\"trace_files_read\":0,\"trace_sessions_started\":0,\"source_commit\":");
    report.quote(PAS_COLD_SCHEDULER_TOOL_SOURCE_COMMIT);
    report.raw(",\"source_dirty\":"); report.boolean(PAS_COLD_SCHEDULER_TOOL_SOURCE_DIRTY != 0);
    report.raw(",\"identity\":{\"tool\":\"cold_scheduler_trace_reader\",\"source_commit\":");
    report.quote(PAS_COLD_SCHEDULER_TOOL_SOURCE_COMMIT);
    report.raw(",\"source_dirty\":"); report.boolean(PAS_COLD_SCHEDULER_TOOL_SOURCE_DIRTY != 0);
    report.raw("},\"checks\":[");
    for (std::size_t i = 0; i < names.size(); ++i) {
        if (i) report.raw(","); report.raw("{\"id\":"); report.quote(names[i]); report.raw(",\"pass\":true}");
    }
    report.raw("]}\n");
    write_new(output_path, report.bytes());
    std::cout << "reader state controls assertions=" << assertions << " qualification=false\n";
    return 0;
}
} // namespace

int wmain(int argc, wchar_t** argv) {
    try {
        if (argc == 4 && std::wstring_view(argv[1]) == L"--self-test" && std::wstring_view(argv[2]) == L"--output")
            return self_test(argv[3]);
        const auto options = arguments(argc, argv);
        // Deny concurrent modification/removal while OpenTrace reads the same
        // path. This is a file identity/stability check, not an invented SHA pin.
        Handle input(CreateFileW(options.etl.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr,
                                 OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr));
        if (input.get() == INVALID_HANDLE_VALUE) fail("cannot open stopped ETL read-only with write/delete denied");
        BY_HANDLE_FILE_INFORMATION before{}, after_identity{};
        if (!GetFileInformationByHandle(input.get(), &before) ||
            (before.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) ||
            file_size(before) == 0 || file_size(before) > kFileBytes) fail("ETL size/identity input cap");
        Context context(options);
        Handle job(CreateJobObjectW(nullptr, nullptr));
        own_job(job, context);
        read_etl(context, input.get());
        context.input_stable = GetFileInformationByHandle(input.get(), &after_identity) && same_file(before, after_identity);
        if (!context.input_stable) context.mark(Issue::InputChanged);
        const auto analysis = analyze(context);
        Json output;
        summary(output, context, analysis, before);
        write_new(options.output, output.bytes());
        std::cout << "diagnostic summary bytes=" << output.bytes().size()
                  << " target_records=" << context.count << " qualification=false\n";
        return context.issues == 0 ? 0 : 2;
    } catch (const std::exception& error) {
        std::cerr << "cold scheduler reader: " << error.what() << '\n';
        return 1;
    }
}
