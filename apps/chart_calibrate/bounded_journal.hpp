#pragma once
#include <nlohmann/json.hpp>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>

namespace rhythm::calibration_trial {
// Receipts are serialized only after release. A library profile's full event
// count may exceed the older UI journal limit, but every buffer remains finite.
struct Journal {
private:
    static std::size_t checked_budget(std::size_t rows) {
        if(rows<1||rows>65536)throw std::invalid_argument("journal row budget must be 1..65536");
        return rows;
    }
public:
    const std::size_t row_limit;
    std::ofstream stream;
    std::string pending;
    std::size_t rows=0,bytes=0;
    explicit Journal(const std::filesystem::path& path,std::size_t row_budget=16000)
      :row_limit(checked_budget(row_budget)),stream(path,std::ios::binary) {
        if(!stream)throw std::runtime_error("journal open failed");
        pending.reserve(65536);
    }
    void append(const nlohmann::json& row) {
        const auto text=row.dump()+"\n";
        if(rows>=row_limit||text.size()>64*1024*1024-bytes)throw std::runtime_error("journal budget");
        pending+=text;++rows;bytes+=text.size();
    }
    void flush_after_cleanup() {
        stream<<pending;stream.flush();
        if(!stream)throw std::runtime_error("journal write failed after cleanup");
        pending.clear();
    }
};
}
