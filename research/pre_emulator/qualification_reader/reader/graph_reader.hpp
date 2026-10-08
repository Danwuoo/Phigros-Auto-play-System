#pragma once
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include "research/windows_qualification/verifier.hpp"
#include <windows.h>
#include "streamed_hash.hpp"

// Development prototype: read-only graph primitives, no admission or device API.
// The larger bound is for the combined graph (three compiler roots, installed
// dependencies and separate raw campaigns). Native-stage/R2 caps are unchanged.
namespace pas::preflight_graph {
namespace w = windows_qualification;
using J = nlohmann::json;
namespace fs = std::filesystem;
inline fs::path final_path(const fs::path& p){
    HANDLE handle=CreateFileW(p.c_str(),FILE_READ_ATTRIBUTES,FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,
        nullptr,OPEN_EXISTING,FILE_FLAG_BACKUP_SEMANTICS|FILE_FLAG_OPEN_REPARSE_POINT,nullptr);
    if(handle==INVALID_HANDLE_VALUE)throw std::runtime_error("final path open");
    std::array<wchar_t,1024> text{};
    const auto n=GetFinalPathNameByHandleW(handle,text.data(),static_cast<DWORD>(text.size()),FILE_NAME_NORMALIZED|VOLUME_NAME_DOS);
    CloseHandle(handle);w::require(n>0&&n<text.size(),"final path bound");
    std::wstring result(text.data(),n);w::require(result.starts_with(L"\\\\?\\"),"final DOS path");
    return fs::path(result.substr(4));
}
inline bool same_path_text(const fs::path& a,const fs::path& b){
    const auto x=a.generic_wstring(),y=b.generic_wstring();return CompareStringOrdinal(x.c_str(),-1,y.c_str(),-1,TRUE)==CSTR_EQUAL;
}
class Reader final {
    fs::path root_;
    std::map<fs::path, J> seen_;
    std::uint64_t bytes_ = 0;
    std::set<fs::path> installed_libraries_;
    static constexpr std::uint64_t graph_cap = 64ull * 1024 * w::MiB;
    static constexpr std::size_t file_cap = 32768;
    const fs::path installed_toolchain_=fs::path(L"C:/Program Files/Microsoft Visual Studio/18/Community/VC/vcpkg/scripts/buildsystems/vcpkg.cmake");
    const fs::path system_command_=fs::path(L"C:/WINDOWS/system32/cmd.exe");
    const std::set<fs::path> installed_build_tools_{
        fs::path(L"C:/Program Files (x86)/Microsoft Visual Studio/18/BuildTools/VC/Auxiliary/Build/vcvars64.bat"),
        fs::path(L"C:/Users/wurre/.cache/codex-runtimes/codex-primary-runtime/dependencies/native/powershell/pwsh.exe"),
        fs::path(L"C:/Users/wurre/AppData/Local/Programs/Python/Python310/Lib/site-packages/cmake/data/bin/cmake.exe"),
        fs::path(L"C:/Users/wurre/AppData/Local/Programs/Python/Python310/Scripts/ninja.exe")};
    fs::path checked_path(const std::string& raw) const {
        w::require(!raw.empty() && raw.size() <= 768, "graph path bound");
        const fs::path p(raw);
        w::require(p.is_absolute() && p.root_name() == root_.root_name(), "graph absolute drive");
        fs::path prefix;
        for (const auto& part : p) {
            const auto s = part.string();
            w::require(part != ".." && part != ".", "graph traversal");
            if (part != p.root_name() && part != p.root_directory())
                w::require(!s.empty() && s.find(':') == std::string::npos &&
                           s.back() != '.' && s.back() != ' ', "graph path component");
            prefix /= part;
            if (prefix == p.root_name()) continue;
            const DWORD attributes = GetFileAttributesW(prefix.c_str());
            w::require(attributes != INVALID_FILE_ATTRIBUTES &&
                       !(attributes & FILE_ATTRIBUTE_REPARSE_POINT), "graph missing path or reparse");
        }
        const auto actual = final_path(p);
        const auto rel = actual.lexically_relative(root_);
        const bool workspace_file=!rel.empty() && !rel.is_absolute() && *rel.begin() != "..";
        // The native dependency closure also pins this one installed toolchain
        // file. No directory-wide exception or configurable external root.
        w::require(workspace_file || same_path_text(actual,installed_toolchain_) || same_path_text(actual,system_command_) || std::any_of(installed_build_tools_.begin(),installed_build_tools_.end(),[&](const auto& tool){return same_path_text(actual,tool);}), ("graph containment: "+actual.string()).c_str());
        w::require(fs::is_regular_file(actual), "graph regular file");
        return actual;
    }
public:
    explicit Reader(const fs::path& root) : root_(final_path(root)) {
        w::require(root_.is_absolute() && fs::is_directory(root_), "graph workspace root");
    }
    fs::path bound(const J& entry, std::uint64_t cap = 1073807360ull) {
        w::require(entry.is_object() && entry.at("bytes").is_number_unsigned(), "graph binding type");
        const auto path = checked_path(entry.at("path").get<std::string>());
        if(installed_libraries_.contains(path))cap=3ull*1024*w::MiB;
        const auto n = entry.at("bytes").get<std::uint64_t>();
        const auto sha = entry.at("sha256").get<std::string>();
        w::require(n <= cap && fs::file_size(path) == n && w::hex(sha, 64), ("graph binding size/SHA: "+path.string()).c_str());
        if (const auto it = seen_.find(path); it != seen_.end()) {
            w::require(it->second.at("bytes") == n && it->second.at("sha256") == sha,
                       "graph conflicting binding");
            return path;
        }
        w::require(seen_.size() < file_cap && n <= graph_cap - bytes_, "graph aggregate bound");
        w::require(streamed_sha256(path, cap) == sha, "graph SHA mismatch");
        bytes_ += n;
        seen_.emplace(path, J{{"path", path.string()}, {"bytes", n}, {"sha256", sha}});
        return path;
    }
    J object(const J& entry, std::uint64_t cap = 4 * w::MiB) {
        return w::read(bound(entry, cap), cap);
    }
    void entries(const J& rows, std::size_t maximum = 8192) {
        w::require(rows.is_array() && !rows.empty() && rows.size() <= maximum, "graph entry array");
        std::set<fs::path> unique;
        for (const auto& e : rows) w::require(unique.insert(bound(e)).second, "graph duplicate entry");
    }
    void installed_dependencies(const J& rows,const fs::path& declared_root){
        const auto expected=root_/"out/vcpkg_installed/x64-windows";
        w::require(final_path(declared_root)==expected&&rows.is_array()&&!rows.empty()&&rows.size()<=8192,"fixed installed dependency scope");
        std::uint64_t total=0;std::set<fs::path> paths;
        for(const auto& entry:rows){
            const auto p=checked_path(entry.at("path").get<std::string>()),relative=p.lexically_relative(expected);
            w::require(!relative.empty()&&!relative.is_absolute()&&*relative.begin()!=".."&&paths.insert(p).second,"installed dependency containment/unique");
            w::require(entry.at("bytes").is_number_unsigned(),"installed dependency byte type");
            const auto n=entry.at("bytes").get<std::uint64_t>();
            w::require(n<=3ull*1024*w::MiB&&n<=12ull*1024*w::MiB-total,"installed dependency aggregate bound");total+=n;
            if(n>256ull*w::MiB){w::require(p.extension()==".lib","large installed dependency library type");installed_libraries_.insert(p);}
            bound(entry);
        }
    }
    void finish() const {
        for (const auto& [path, e] : seen_)
            w::require(checked_path(path.string()) == path && fs::file_size(path) == e.at("bytes") &&
                       streamed_sha256(path,e.at("bytes").get<std::uint64_t>()) == e.at("sha256").get<std::string>(), "graph changed during verification");
    }
    std::size_t count() const { return seen_.size(); }
    std::uint64_t bytes() const { return bytes_; }
    J owned(const J& binding, int native, int runner, bool natural) {
        const auto path = bound(binding, w::MiB);
        const auto v = w::read(path);
        w::require(!v.at("pending").get<bool>() && v.at("producer_complete").get<bool>() &&
                   v.at("verification_exit") == 0 && v.at("native_exit") == native &&
                   v.at("runner_exit") == runner, "graph unverified native stage");
        entries(v.at("entries"), 128);
        const auto stage = v.at("stage").get<std::string>();
        w::require(!stage.empty() && stage.size() <= 90 &&
                   stage.find_first_not_of("abcdefghijklmnopqrstuvwxyz0123456789-") == std::string::npos &&
                   path.filename() == stage + "-verification.json", "graph stage binding");
        const auto result_path = final_path(path.parent_path() / (stage + "-result.json"));
        w::require(seen_.contains(result_path), "graph result not sealed");
        const auto r = w::read(result_path);
        const auto& f = r.at("facts");
        w::require(r.at("stage") == stage && !r.at("pending").get<bool>() &&
                   f.at("created").get<bool>() && f.at("assigned").get<bool>() &&
                   f.at("resumed").get<bool>() && f.at("identity_trusted").get<bool>() &&
                   f.at("held_all_signaled").get<bool>() && f.at("streams_completed").get<bool>() &&
                   f.at("active_final") == 0 && f.at("exit_code") == native &&
                   f.at("runner_exit") == runner && f.at("errors").empty() &&
                   !f.at("errors_overflow").get<bool>() &&
                   f.at("guard_version") == "pas.owned-held-identity.v3", "graph native facts");
        if (natural) w::require(f.at("natural_quiescence").get<bool>() &&
                               f.at("termination_returned").is_null(), "graph natural completion");
        return v;
    }
    std::set<fs::path> closure(const J& binding, const J& process, const std::string& stage) {
        const auto receipt_path = bound(binding, w::MiB);
        const auto c = object(binding, w::MiB);
        const auto state_path = bound(c.at("state"), w::MiB);
        const auto s = w::read(state_path);
        w::require(c.at("stage") == stage && c.at("schema") == "pas.current-input-closure-verification.v1" &&
                   c.at("pass").get<bool>() && c.at("before_verified").get<bool>() &&
                   c.at("after_verified").get<bool>() && c.at("status") == "COMPLETE" &&
                   s.at("status") == "COMPLETE" && s.at("running").is_null() &&
                   s.at("native_stages_consumed") == 1 && s.at("native_launches_known") == 1,
                   "graph complete input closure");
        const auto v = owned(c.at("verification"), 0, 0, true);
        w::require(v.at("stage") == stage && c.at("verification") == process.at("verification") &&
                   c.at("state") == process.at("state"), "graph closure/process join");
        const auto freeze_path = bound(J{{"path", (state_path.parent_path() / "freeze.json").string()},
            {"bytes", fs::file_size(state_path.parent_path() / "freeze.json")}, {"sha256", s.at("freeze_sha")}}, 65536);
        const auto contract_path = bound(J{{"path", (state_path.parent_path() / "contract.json").string()},
            {"bytes", fs::file_size(state_path.parent_path() / "contract.json")}, {"sha256", s.at("contract_sha")}}, 65536);
        w::require(s.at("freeze_sha") == seen_.at(freeze_path).at("sha256") &&
                   s.at("contract_sha") == seen_.at(contract_path).at("sha256"), "graph state freeze/contract SHA");
        bool sealed = false;
        for (const auto& e : s.at("receipts")) sealed |= bound(e) == bound(c.at("verification"));
        w::require(sealed, "graph state must seal native verification");
        const auto freeze = w::read(freeze_path, 65536);
        std::map<fs::path, std::string> frozen;
        for (const auto& e : freeze.at("files"))
            w::require(frozen.emplace(bound(e), e.at("sha256").get<std::string>()).second, "graph repeated frozen input");
        const auto root_path = bound(c.at("root"), 32768);
        w::require(frozen.contains(root_path) && frozen.at(root_path) == c.at("root").at("sha256").get<std::string>(),
                   "graph closure root directly frozen");
        const auto root = object(c.at("root"), 32768);
        w::require(root.at("schema") == "pas.current-input-closure.v1" && root.at("stage") == stage &&
                   final_path(root.at("verification_output").get<std::string>()) == receipt_path &&
                   root.at("leaves").is_array() && !root.at("leaves").empty() &&
                   root.at("leaves").size() <= 128, "graph closure metadata bound");
        std::set<fs::path> leaves, inputs;
        std::size_t index = 0;
        for (const auto& e : root.at("leaves")) {
            const auto leaf_path = bound(e, 32768);
            w::require(leaf_path != root_path && leaf_path != receipt_path && leaves.insert(leaf_path).second,
                       "graph closure duplicate/self leaf");
            const auto leaf = object(e, 32768);
            const auto& files = leaf.at("files");
            w::require(leaf.at("schema") == "pas.current-input-closure-leaf.v1" && leaf.at("stage") == stage &&
                       leaf.at("index") == index++ && files.is_array() && !files.empty() && files.size() <= 64,
                       "graph closure ordered leaf file bound");
            for (const auto& file : files) {
                const auto p = bound(file, 256ull * w::MiB);
                w::require(p != root_path && p != receipt_path && p != leaf_path && inputs.insert(p).second &&
                           inputs.size() <= 8192, "graph closure duplicate/self input");
            }
        }
        for (const auto& p : leaves) w::require(!inputs.contains(p), "graph closure leaf/input overlap");
        w::require(c.at("files_count") == inputs.size() && root.at("files_count") == inputs.size() &&
                   c.at("leaf_count") == leaves.size() && root.at("leaf_count") == leaves.size(),
                   "graph closure complete denominator");
        w::require(!w::read(contract_path).at("device_allowed").get<bool>(), "graph cold contract device scope");
        for(const auto& [p,sha]:frozen){(void)sha;inputs.insert(p);}
        return inputs;
    }
    std::set<fs::path> process(const J& p) {
        w::require(p.is_object()&&p.size()==8,"eight complete process evidence bindings");
        for(const auto* key:{"specification","input_closure","result","verification","freeze","state","contract","command"})
            bound(p.at(key));
        const auto v=owned(p.at("verification"),0,0,true);
        const auto stage=v.at("stage").get<std::string>();
        const auto spec=object(p.at("specification")),command=object(p.at("command")),contract=object(p.at("contract"));
        const auto result=object(p.at("result"));
        w::require(spec.at("stage")==stage&&command.at("stage")==stage&&result.at("stage")==stage&&
            contract.at("spec").at("name")==stage&&spec.at("native_exit")==0&&spec.at("runner_exit")==0,
            "process stage and expected exit join");
        w::require(final_path(spec.at("exe").get<std::string>())==
            final_path(result.at("facts").at("root_image").get<std::string>()),"actual process root image");
        for(const auto* key:{"exe","argv","native_exit","runner_exit","total_s","stream_cap"})
            w::require(spec.at(key)==command.at("spec").at(key)&&spec.at(key)==contract.at("spec").at(key),
                "frozen and launched command join");
        const auto inputs=closure(p.at("input_closure"),p,stage);
        w::require(inputs.contains(bound(p.at("specification"))),"specification frozen before launch");
        return inputs;
    }
    void gate(const J& binding) {
        const auto g = object(binding, w::MiB);
        w::require(g.at("schema") == "pas.windows-offline-process-gate.v3" &&
                   g.at("file_only_cases") == 20 && g.at("file_only_failed") == 0 &&
                   g.at("native_controls_verified") == 4 && g.at("wrapper_positive").get<bool>() &&
                   g.at("device_operations") == 0 && !g.at("historical_stop_resumed").get<bool>(),
                   "graph process gate v3");
        bound(g.at("guard_source")); bound(g.at("process_source"));
        const auto controls = [&](const J& pin, std::size_t expected) {
            const auto d = object(pin, w::MiB);
            w::require(d.at("failed") == 0 && d.at("actual_CreateProcess") == 0 &&
                       d.at("cases").is_array() && d.at("cases").size() == expected, "graph file-only controls");
            std::set<std::string> ids;
            for (const auto& c : d.at("cases"))
                w::require(c.at("pass").get<bool>() && ids.insert(c.at("id").get<std::string>()).second,
                           "graph complete unique control set");
        };
        controls(g.at("diagnostic"), 20);
        controls(g.at("cached_identity_controls"), 9);
        controls(g.at("identity_wait_controls"), 39);
        const std::map<std::string, std::pair<int,int>> expected{
            {"natural",{0,0}}, {"nonzero",{7,7}}, {"owned-child",{0,125}}, {"probe-1",{0,0}}};
        w::require(g.at("native_controls").is_array() && g.at("native_controls").size() == 4,
                   "graph four native controls");
        std::set<std::string> seen;
        for (const auto& c : g.at("native_controls")) {
            const auto id = c.at("stage").get<std::string>();
            w::require(expected.contains(id) && seen.insert(id).second, "graph native control set");
            const auto [native, runner] = expected.at(id);
            w::require(c.at("native_exit") == native && c.at("runner_exit") == runner &&
                       c.at("verification_exit") == 0 && c.at("active_final") == 0 &&
                       c.at("held_all_signaled").get<bool>() && c.at("streams_completed").get<bool>(),
                       "graph native control summary");
            w::require(owned(c.at("receipt"), native, runner, id != "owned-child").at("stage") == id,
                       "graph native control identity");
        }
    }
};
}





