#include "pas/config.hpp"

#include <cmath>
#include <fstream>
#include <set>
#include <stdexcept>

namespace pas {
namespace {
using json = nlohmann::json;

void fields(const json& object, std::set<std::string> allowed,
            std::set<std::string> required, const char* label) {
    if (!object.is_object()) throw std::invalid_argument(std::string(label) + " must be an object");
    for (auto it = object.begin(); it != object.end(); ++it)
        if (!allowed.contains(it.key())) throw std::invalid_argument(std::string("unknown ") + label + " field: " + it.key());
    for (const auto& key : required)
        if (!object.contains(key)) throw std::invalid_argument(std::string("missing ") + label + " field: " + key);
}

int integer(const json& value, int low, int high, const char* label) {
    if (!value.is_number_integer()) throw std::invalid_argument(std::string(label) + " must be integer");
    const auto number = value.get<std::int64_t>();
    if (number < low || number > high) throw std::invalid_argument(std::string(label) + " out of range");
    return static_cast<int>(number);
}

std::string string(const json& value, const char* label) {
    if (!value.is_string() || value.get<std::string>().empty())
        throw std::invalid_argument(std::string(label) + " must be nonempty string");
    return value.get<std::string>();
}

json read(const std::filesystem::path& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file) throw std::runtime_error("cannot open configuration file");
    return json::parse(file, nullptr, true, true);
}
}

RuntimeConfig load_config(const std::filesystem::path& path) {
    auto raw = read(path);
    fields(raw, {"schema", "name", "serial", "capture", "touch", "scheduler", "preview", "log_dir", "game"},
           {"schema", "name", "serial", "capture", "touch", "scheduler", "preview", "log_dir"}, "profile");
    if (integer(raw.at("schema"), 2, 2, "schema") != 2)
        throw std::invalid_argument("use 'pas config migrate' for schema 1");
    const auto& c = raw.at("capture");
    const auto& t = raw.at("touch");
    const auto& s = raw.at("scheduler");
    const auto& p = raw.at("preview");
    fields(c, {"kind", "execution", "transport", "image_format", "row_order", "width", "height",
               "source_rotation", "endpoint", "token_file", "grpc_read_chunk_kib", "max_relative_lag_ms"},
           {"kind", "execution", "transport", "image_format", "row_order", "width", "height", "source_rotation"}, "capture");
    fields(t, {"kind", "timeout_ms", "max_contacts", "width", "height", "rotation_deg"},
           {"kind", "timeout_ms", "max_contacts"}, "touch");
    fields(s, {"max_plans", "max_steps", "horizon_ms", "evidence_max_age_ms"},
           {"max_plans", "max_steps", "horizon_ms", "evidence_max_age_ms"}, "scheduler");
    fields(p, {"hz"}, {"hz"}, "preview");
    RuntimeConfig config;
    config.name = string(raw.at("name"), "name");
    config.serial = string(raw.at("serial"), "serial");
    if (config.serial.rfind("emulator-", 0) != 0) throw std::invalid_argument("explicit emulator serial required");
    config.capture_kind = string(c.at("kind"), "capture kind");
    if (config.capture_kind != "fake" && config.capture_kind != "emulator-grpc")
        throw std::invalid_argument("unsupported capture kind");
    if (string(c.at("execution"), "execution") != "thread")
        throw std::invalid_argument("process execution is retired; run 'pas config migrate'");
    config.capture_transport = string(c.at("transport"), "transport");
    if (config.capture_transport != "payload" || c.at("image_format") != "rgb888" ||
        c.at("row_order") != "top-down")
        throw std::invalid_argument("runtime requires payload RGB888 top-down");
    config.width = integer(c.at("width"), 1, 4096, "capture width");
    config.height = integer(c.at("height"), 1, 4096, "capture height");
    if (static_cast<std::uint64_t>(config.width) * config.height * 3 > 16 * 1024 * 1024)
        throw std::invalid_argument("capture geometry exceeds fixed pool capacity");
    config.source_rotation = integer(c.at("source_rotation"), 0, 3, "source rotation");
    config.grpc_read_chunk_kib = integer(c.value("grpc_read_chunk_kib", json(256)),
                                       8, 256, "gRPC read chunk KiB");
    if (config.grpc_read_chunk_kib != 8 && config.grpc_read_chunk_kib != 64 &&
        config.grpc_read_chunk_kib != 256)
        throw std::invalid_argument("gRPC read chunk must be 8, 64 or 256 KiB");
    config.endpoint = c.contains("endpoint") ? string(c.at("endpoint"), "endpoint") : "";
    config.max_relative_lag_ms = integer(c.value("max_relative_lag_ms", json(250)),
                                         1, 1000, "max relative lag ms");
    config.token_file = c.contains("token_file") ? string(c.at("token_file"), "token_file") : "";
    if (config.endpoint.empty() != config.token_file.empty())
        throw std::invalid_argument("endpoint and token_file must be paired");
    config.touch_kind = string(t.at("kind"), "touch kind");
    if (config.touch_kind != "none" && config.touch_kind != "emulator-grpc")
        throw std::invalid_argument("unsupported touch kind");
    config.touch_timeout_ms = integer(t.at("timeout_ms"), 1, 2000, "touch timeout");
    config.max_contacts = integer(t.at("max_contacts"), 1, 10, "contact limit");
    config.touch_width = t.contains("width") ? integer(t.at("width"), 2, 4096, "touch width") : config.width;
    config.touch_height = t.contains("height") ? integer(t.at("height"), 2, 4096, "touch height") : config.height;
    config.touch_rotation = t.contains("rotation_deg") ? integer(t.at("rotation_deg"), 0, 270, "touch rotation") : 0;
    if (config.touch_rotation % 90) throw std::invalid_argument("touch rotation must be quarter-turn");
    config.max_plans = integer(s.at("max_plans"), 1, 256, "plan limit");
    config.max_steps = integer(s.at("max_steps"), 2, 1024, "step limit");
    config.horizon_ms = integer(s.at("horizon_ms"), 1, 5000, "horizon");
    config.evidence_max_age_ms = integer(s.at("evidence_max_age_ms"), 1, 1000, "evidence age");
    if(raw.contains("game")) {
        const auto& g=raw.at("game");
        fields(g,{"enabled_types","lead_ms","uncertainty_ms"},{"enabled_types","lead_ms","uncertainty_ms"},"game");
        if(!g.at("enabled_types").is_array()||g.at("enabled_types").empty()||g.at("enabled_types").size()>4)
            throw std::invalid_argument("game enabled_types must contain 1..4 types");
        config.game_type_mask=0;
        for(const auto& type:g.at("enabled_types")) {
            const auto value=string(type,"game type");
            const int bit=value=="tap"?1:value=="hold"?2:value=="drag"?4:value=="flick"?8:0;
            if(!bit||(config.game_type_mask&bit)) throw std::invalid_argument("unknown or duplicate game type");
            config.game_type_mask|=bit;
        }
        config.game_lead_ms=integer(g.at("lead_ms"),-60,60,"game lead ms");
        config.game_uncertainty_ms=integer(g.at("uncertainty_ms"),1,60,"game uncertainty ms");
    }
    if (!p.at("hz").is_number() || !std::isfinite(p.at("hz").get<double>()) ||
        p.at("hz").get<double>() < 0 || p.at("hz").get<double>() > 10)
        throw std::invalid_argument("preview Hz out of range");
    config.preview_hz = p.at("hz").get<double>();
    config.log_dir = string(raw.at("log_dir"), "log directory");
    config.public_json = raw;
    config.public_json["capture"]["grpc_read_chunk_kib"] = config.grpc_read_chunk_kib;
    config.public_json["capture"]["max_relative_lag_ms"] = config.max_relative_lag_ms;
    if (config.public_json.at("capture").contains("token_file"))
        config.public_json.at("capture")["token_file"] = "<redacted>";
    return config;
}

void migrate_config(const std::filesystem::path& source, const std::filesystem::path& target) {
    if (std::filesystem::exists(target)) throw std::runtime_error("migration target already exists");
    auto raw = read(source);
    if (!raw.is_object() || !raw.contains("schema") || raw.at("schema") != 1 ||
        !raw.contains("capture") || !raw.at("capture").is_object() ||
        raw.at("capture").value("execution", "") != "process")
        throw std::invalid_argument("expected legacy schema 1 process profile");
    raw["schema"] = 2;
    raw["capture"]["execution"] = "thread";
    // Validate before writing; use a temporary sibling and rename atomically.
    const auto temporary = target.string() + ".tmp";
    {
        std::ofstream output(temporary, std::ios::binary | std::ios::trunc);
        if (!output) throw std::runtime_error("cannot create migrated configuration");
        output << raw.dump(2) << '\n';
    }
    try { (void)load_config(temporary); std::filesystem::rename(temporary, target); }
    catch (...) { std::filesystem::remove(temporary); throw; }
}

} // namespace pas
