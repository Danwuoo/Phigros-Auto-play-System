#include "hd9_strategy_trace.hpp"
#include <gtest/gtest.h>
#include <filesystem>
#include <fstream>
#include <regex>
// Supplement the unchanged original red test. Its oracle and raw failure stay
// intact. Every historical field and all plans/receipts must still match.
TEST(CurrentSessionSchemaCompatibility, ExactGoldenExceptTwoAddedDiagnosticFields) {
 using J=nlohmann::json;
 const auto original=pas::test::hd9_trace(false),wrapped=pas::test::hd9_trace(true);
 ASSERT_EQ(original,wrapped);ASSERT_EQ(wrapped.size(),108);
 std::ifstream file(std::filesystem::path(PAS_TEST_SOURCE_DIR)/"tests/data/main-strategy-golden.json");ASSERT_TRUE(file);
 const auto golden=J::parse(file);auto legacy=wrapped;
 for(auto& frame:legacy){auto& decision=frame.at("decision");decision["decision_schema"]=2;
  for(auto& target:decision.at("targets")){
   target.erase("line_projection_only");target.erase("last_line_observed_ns");target.erase("projected_line_along_px");target.erase("projected_line_half_length_px");
  }
 }
 const auto diff=J::diff(golden,legacy);ASSERT_EQ(diff.size(),286);
 const std::regex allowed("^/[0-9]+/decision/targets/[0-9]+/(current_contact|tail_terminal_proof)$");
 for(const auto& change:diff){ASSERT_EQ(change.at("op"),"add");ASSERT_TRUE(std::regex_match(change.at("path").get<std::string>(),allowed));}
 for(auto& frame:legacy)for(auto& target:frame.at("decision").at("targets")){
  ASSERT_TRUE(target.contains("current_contact")&&target.at("current_contact").is_string());
  ASSERT_TRUE(target.contains("tail_terminal_proof")&&(target.at("tail_terminal_proof").is_null()||target.at("tail_terminal_proof").is_string()));
  target.erase("current_contact");target.erase("tail_terminal_proof");
 }
 EXPECT_EQ(legacy,golden);
}
