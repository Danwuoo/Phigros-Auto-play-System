#include "rhythm/chart_memory.hpp"
#include <iostream>

int main(int argc, char** argv) {
    try {
        const std::string name = argc == 2 ? argv[1] : "phigros";
        if (argc > 2 || (name != "phigros" && name != "project-sekai")) {
            std::cerr << "Usage: rhythm_dry_run [phigros|project-sekai]\n";
            return 2;
        }
        const auto game = name == "phigros" ? rhythm::Game::phigros : rhythm::Game::project_sekai;
        rhythm::Memory memory{{game, "synthetic-only", "fixture", "v1",
            std::string(64, '0'), std::string(64, '0'), "synthetic-layout"}, {}};
        memory.contacts.push_back({1, {{1'000'000'000, rhythm::Phase::down, {0.25, 0.8}},
                                      {1'025'000'000, rhythm::Phase::up, {0.25, 0.8}}}});
        memory.contacts.push_back({2, {{1'000'000'000, rhythm::Phase::down, {0.75, 0.8}},
                                      {1'100'000'000, rhythm::Phase::move, {0.65, 0.8}},
                                      {1'200'000'000, rhythm::Phase::up, {0.5, 0.8}}}});
        std::cout << "DRY RUN ONLY: " << name << "; synthetic contacts, not a native chart\n";
        const rhythm::ClockMap clock{0, 10'000'000'000, 1};
        for (const auto& e : rhythm::map_for_dry_run(memory, clock, 0, 2))
            std::cout << e.host_ns << ',' << e.contact_id << ',' << static_cast<int>(e.phase)
                      << ',' << e.point.x << ',' << e.point.y << '\n';
        return 0;
    } catch (const std::exception& e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
