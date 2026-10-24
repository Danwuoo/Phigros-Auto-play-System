#include "rhythm/chart.hpp"
#include <algorithm>
#include <array>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <optional>
#include <stdexcept>
#include <string>

int main(int argc, char** argv) {
    try {
        if (argc < 2 || argc > 4) throw std::invalid_argument("usage: chart_inspect chart.json [expected-sha256] [--notes-jsonl]");
        rhythm::ChartImportLimits limits;
        std::ifstream input(argv[1], std::ios::binary | std::ios::ate);
        if (!input) throw std::runtime_error("cannot open chart");
        const auto size = input.tellg();
        if (size < 0 || static_cast<unsigned long long>(size) > limits.max_json_bytes) throw std::invalid_argument("chart byte limit exceeded");
        std::string text(static_cast<std::size_t>(size), '\0');
        input.seekg(0);
        if (!input.read(text.data(), static_cast<std::streamsize>(text.size()))) throw std::runtime_error("chart read failed");
        rhythm::ChartSource source;
        source.label = argv[1];
        bool notes_jsonl = false;
        for (int i = 2; i < argc; ++i) {
            const std::string argument = argv[i];
            if (argument == "--notes-jsonl" && !notes_jsonl) notes_jsonl = true;
            else if (!argument.starts_with("--") && source.chart_sha256.empty()) source.chart_sha256 = argument;
            else throw std::invalid_argument("unknown or duplicate chart_inspect argument");
        }
        const auto chart = rhythm::parse_phigros(text, source, limits);
        std::cout << std::setprecision(17);
        if (notes_jsonl) {
            for (const auto& line : chart.lines) for (const auto& note : line.notes) {
                // IDs/pointers contain only the verified hex hash and fixed schema paths.
                std::cout << "{\"kind\":\"note\",\"chart_sha256\":\"" << chart.source.chart_sha256
                          << "\",\"source_id\":\"" << note.source.id << "\",\"source_pointer\":\"" << note.source.json_pointer
                          << "\",\"line\":" << line.source_index << ",\"bpm\":" << line.bpm
                          << ",\"side\":\"" << (note.side == rhythm::NoteSide::above ? "above" : "below")
                          << "\",\"type\":" << static_cast<int>(note.kind) << ",\"source_index\":" << note.source_index
                          << ",\"raw_tick\":" << note.time_tick << ",\"raw_hold_ticks\":" << note.hold_ticks
                          << ",\"start_ns\":" << note.start_ns << ",\"end_ns\":" << note.end_ns
                          << ",\"position_x\":" << note.position_x << ",\"speed\":" << note.speed
                          << ",\"floor_position\":" << note.floor_position << "}\n";
            }
            return 0;
        }
        std::optional<rhythm::SongNs> first, last;
        std::array<std::size_t, 4> counts{};
        for (const auto& line : chart.lines) for (const auto& note : line.notes) {
            ++counts[static_cast<std::size_t>(note.kind) - 1];
            first = first ? std::min(*first, note.start_ns) : note.start_ns;
            last = last ? std::max(*last, note.end_ns) : note.end_ns;
        }
        std::cout << "{\"format_version\":3,\"chart_sha256\":\"" << chart.source.chart_sha256
                  << "\",\"offset_seconds\":" << chart.offset_seconds << ",\"lines\":" << chart.lines.size()
                  << ",\"notes\":" << chart.note_count << ",\"events\":" << chart.event_count
                  << ",\"types\":{\"tap\":" << counts[0] << ",\"drag\":" << counts[1]
                  << ",\"hold\":" << counts[2] << ",\"flick\":" << counts[3] << "},\"first_note_ns\":";
        if (first) std::cout << *first; else std::cout << "null";
        std::cout << ",\"last_note_end_ns\":";
        if (last) std::cout << *last; else std::cout << "null";
        std::cout << ",\"time_rule\":\"round((tick*1.875/BPM+offset_seconds)*1e9); offset applied once\"}\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "chart-inspect: " << error.what() << '\n';
        return 1;
    }
}
