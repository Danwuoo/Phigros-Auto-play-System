// C++20 evidence comparison. Does not change inputs, expectations or verdicts.
#include <nlohmann/json.hpp>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <stdexcept>
#include <string>
using J = nlohmann::json;
using Checks = std::map<std::string, J>;
J read(const char* path) { std::ifstream f(path); if (!f) throw std::runtime_error("input missing"); return J::parse(f); }
void add(Checks& checks, const J& values, const std::string& prefix) {
  for (std::size_t i = 0; i < values.size(); ++i) {
    const auto& a = values.at(i);
    if (!a.contains("pass") || !a.contains("expected") || !a.contains("actual") || !a.contains("field"))
      throw std::runtime_error("assertion schema");
    checks.emplace(prefix + "/" + std::to_string(i), a);
  }
}
Checks collect(const J& r) {
  Checks checks;
  for (auto it = r.at("layers").begin(); it != r.at("layers").end(); ++it) {
    const auto& rows = it.value().at("rows");
    for (std::size_t i = 0; i < rows.size(); ++i)
      add(checks, rows[i].at("assertions"), "/layers/" + it.key() + "/rows/" + std::to_string(i) + "/assertions");
  }
  for (const std::string group : {"schema_negative_controls", "renderer_controls", "supplemental"}) {
    const auto& rows = r.at(group);
    for (std::size_t i = 0; i < rows.size(); ++i)
      add(checks, rows[i].at("assertions"), "/" + group + "/" + std::to_string(i) + "/assertions");
  }
  add(checks, r.at("contact_adapter_controls"), "/contact_adapter_controls");
  if (checks.size() != r.at("assertions").get<std::size_t>()) throw std::runtime_error("assertion denominator mismatch");
  return checks;
}
int main(int argc, char** argv) {
  try {
    if (argc != 4) throw std::runtime_error("usage: compare_legacy_results BASELINE AFTER NEW_OUTPUT");
    if (std::filesystem::exists(argv[3])) throw std::runtime_error("refusing to overwrite evidence");
    const auto before = read(argv[1]), after = read(argv[2]);
    auto a = collect(before), b = collect(after);
    if (a.size() != b.size() || before.at("assertions") != 3938) throw std::runtime_error("not the full frozen 3938 denominator");
    J oldFailures = J::array(), regressions = J::array(), referenceMaterializations = J::array();
    int fixed = 0, remain = 0, stablePass = 0, afterFailed = 0;
    for (const auto& [pointer, old] : a) {
      const auto it = b.find(pointer);
      if (it == b.end()) throw std::runtime_error("assertion disappeared");
      const auto& now = it->second;
      if (old.at("field") != now.at("field")) throw std::runtime_error("assertion identity changed");
      if (old.at("expected") != now.at("expected")) {
        // The frozen oracle names a reference case for these metamorphic rules.
        // driver.inc materializes that case's CURRENT output as report.expected;
        // a behavior fix can change both sides without changing the oracle rule.
        if (old.at("field") != "equal_to" && old.at("field") != "equivalent_to")
          throw std::runtime_error("literal expected value changed");
        referenceMaterializations.push_back({{"pointer", pointer}, {"field", old.at("field")},
          {"before_expected", old.at("expected")}, {"after_expected", now.at("expected")},
          {"before_pass", old.at("pass")}, {"after_pass", now.at("pass")}});
      }
      const bool wasPass = old.at("pass"), isPass = now.at("pass");
      if (!isPass) ++afterFailed;
      if (!wasPass) {
        const std::string state = isPass ? "fixed_against_unchanged_legacy_expected" : "still_failed_against_unchanged_legacy_expected";
        oldFailures.push_back({{"pointer", pointer}, {"field", old.at("field")}, {"expected", old.at("expected")},
          {"before_actual", old.at("actual")}, {"after_actual", now.at("actual")}, {"status", state}});
        if (isPass) ++fixed; else ++remain;
      } else if (!isPass) {
        regressions.push_back({{"pointer", pointer}, {"field", old.at("field")}, {"expected", old.at("expected")},
          {"before_actual", old.at("actual")}, {"after_actual", now.at("actual")}});
      } else ++stablePass;
    }
    if (oldFailures.size() != before.at("failed_assertions").get<std::size_t>() || afterFailed != after.at("failed_assertions").get<int>())
      throw std::runtime_error("failure denominator mismatch");
    J out = {{"schema", "bvi.cold-v3.legacy-delta.v1"}, {"literal_expectations_unchanged", true},
      {"oracle_definition_integrity", "Six immutable input hashes are independently locked by run_regression.sh; reference-case output materialization is not a literal oracle change."}, {"assertions_before", a.size()},
      {"assertions_after", b.size()}, {"original_failed_assertions", before.at("failed_assertions")}, {"fixed_original_assertions", fixed},
      {"remaining_original_failed_assertions", remain}, {"new_failed_assertions", regressions.size()},
      {"unchanged_passed_assertions", stablePass}, {"after_failed_assertions", afterFailed},
      {"original_failure_map", oldFailures}, {"regressions", regressions},
      {"metamorphic_expected_materializations_changed", referenceMaterializations}, {"candidate_adopted", false},
      {"real_game_miss_effect", nullptr}};
    std::ofstream file(argv[3]); if (!file) throw std::runtime_error("output open"); file << out.dump(2) << '\n';
    if (!file) throw std::runtime_error("output write");
    std::cout << "legacy3938 fixed=" << fixed << " remaining=" << remain << " new_failures=" << regressions.size() << '\n';
    return regressions.empty() ? 0 : 1;
  } catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 2; }
}
