#include <nlohmann/json.hpp>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>
namespace {
using J=nlohmann::json;namespace fs=std::filesystem;
void require(bool ok,const char* reason){if(!ok)throw std::runtime_error(reason);}
std::string read(const fs::path& path){require(fs::is_regular_file(path)&&fs::file_size(path)<=4*1024*1024,"bounded complete GTest stdout required");std::ifstream f(path,std::ios::binary);std::string s{std::istreambuf_iterator<char>(f),std::istreambuf_iterator<char>()};require(bool(f)||f.eof(),"stdout read failed");return s;}
std::vector<J> traces(const std::string& s){
 require(s.find("[==========] Running 18 tests from 7 test suites.")!=std::string::npos&&s.find("[  PASSED  ] 16 tests.")!=std::string::npos&&s.find("[  FAILED  ] ManualStrategy.SamePixelsFakeClockTargetsPlansReceiptsMatchPremergeMainGolden")!=std::string::npos&&s.find("[  SKIPPED ] ManualUiPixels.ExistingResultAndMenuEvidenceOfflineOnly")!=std::string::npos,"exact original suite outcome required");
 std::vector<J> out;std::size_t offset=0;while((offset=s.find("    Which is: ",offset))!=std::string::npos){offset+=14;const auto end=s.find('\n',offset);auto line=s.substr(offset,end-offset);if(!line.empty()&&line.back()=='\r')line.pop_back();out.push_back(J::parse(line));}
 require(out.size()==2&&out[0].is_array()&&out[1].is_array()&&out[0].size()==108&&out[1].size()==108,"complete current/golden traces required");return out;
}
}
int main(int argc,char** argv){try{
 require(argc==5,"usage baseline-stdout candidate-stdout original-golden fresh-output");const fs::path output=fs::absolute(argv[4]);require(!fs::exists(output),"fresh comparison output required");
 const auto baseline=traces(read(argv[1])),candidate=traces(read(argv[2]));const auto golden=J::parse(read(argv[3]));
 require(baseline[0]==candidate[0],"serialization candidate changed current pixels/targets/plans/receipts trace");require(baseline[1]==candidate[1]&&candidate[1]==golden,"historical oracle bytes/trace changed");require(candidate[0]!=golden,"expected historical golden failure disappeared");
 const auto diff=J::diff(golden,candidate[0]);J actions=J::array();std::size_t diagnostic=0;for(const auto& change:diff){const auto path=change.at("path").get<std::string>();if(path.find("/decision/")!=std::string::npos)++diagnostic;else actions.push_back(change);}
 const J result={{"schema","pas.current-session-trace-baseline-control.v1"},{"baseline_source_commit","8e953c723efe5ed97af51184739673f15ed8af03"},{"candidate_source_commit","3f23faf8bb91a4a974c33463e7232d3507c1dcc1"},{"current_trace_equal",true},{"frames",candidate[0].size()},{"original_golden_failure_preserved",true},{"original_suite_tests",18},{"original_suite_pass",16},{"original_suite_fail",1},{"original_suite_skip",1},{"historical_diff_operations",diff.size()},{"decision_diff_operations",diagnostic},{"other_historical_differences",actions},{"live_ready",false},{"device_operations",0},{"scope","exact current-source baseline comparison; historical main golden remains FAIL; no live or cost qualification"}};
 std::ofstream f(output,std::ios::binary);f<<result.dump(2)<<'\n';require(bool(f),"fresh comparison write failed");std::cout<<"current baseline trace identical; historical main golden remains FAIL\n";return 0;
 }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 2;}}
