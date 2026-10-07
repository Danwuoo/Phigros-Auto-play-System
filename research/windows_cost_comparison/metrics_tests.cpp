#include "metrics.hpp"
#include <nlohmann/json.hpp>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
int main(int argc,char** argv){try{
 if(argc!=2||std::filesystem::exists(argv[1]))throw std::runtime_error("fresh test output required");
 namespace c=pas::windows_cost_comparison;using J=nlohmann::json;J checks=J::array();
 auto check=[&](const char* name,bool pass){checks.push_back({{"name",name},{"pass",pass}});if(!pass)throw std::runtime_error(name);};
 auto rejects=[&](const char* name,auto call){bool rejected=false;try{call();}catch(const std::exception&){rejected=true;}check(name,rejected);};
 const auto f=c::noise({10,10,10,10},"observer_ms");check("zero_noise_keeps_floor",f.d==0&&f.t==.5&&f.adequate);
 const auto pairs=c::noise({10,10.25,20,20.5},"observer_ms");check("declared_disjoint_pairs_not_range",pairs.d==.5&&pairs.t==1&&pairs.adequate);
 check("noise_relative_boundary_inclusive",c::noise({10,11.5,10,11.5},"observer_ms").adequate);
 check("noise_above_boundary_fails",!c::noise({10,11.500001,10,10},"observer_ms").adequate);
 check("owner_floor",c::noise({0,0,0,0},"owner_ms").t==.25);
 check("capture_floor",c::noise({0,0,0,0},"capture_to_owner_ms").t==3);
 const auto boundary=c::compare({10,11,11,10},1);check("regression_boundary_inclusive",boundary.non_regression&&!boundary.improved&&boundary.b_minus_a==1);
 check("above_regression_boundary_fails",!c::compare({10,11.000001,11.000001,10},1).non_regression);
 check("improvement_at_t_does_not_claim_benefit",!c::compare({10,9,9,10},1).improved);
 check("improvement_beyond_t",c::compare({10,8.9,8.9,10},1).improved);
 const auto drift=c::compare({10,10,12,14},1);check("ABBA_pairs_balance_within_batch",drift.a==12&&drift.b==11&&drift.non_regression&&!drift.improved);
 const auto high=std::numeric_limits<double>::max();check("finite_pair_mean_does_not_overflow",std::isfinite(c::compare({high,high,high,high},1).a));
 rejects("negative_AA",[&]{c::noise({1,-1,1,1},"observer_ms");});
 rejects("NaN_AA",[&]{c::noise({1,std::numeric_limits<double>::quiet_NaN(),1,1},"observer_ms");});
 rejects("infinite_AA",[&]{c::noise({1,1,std::numeric_limits<double>::infinity(),1},"owner_ms");});
 rejects("overflow_noise_tolerance",[&]{c::noise({0,high,0,0},"observer_ms");});
 rejects("unknown_metric",[&]{c::noise({1,1,1,1},"hook_ms");});
 rejects("negative_B",[&]{c::compare({1,-1,1,1},1);});
 rejects("NaN_B",[&]{c::compare({1,1,std::numeric_limits<double>::quiet_NaN(),1},1);});
 rejects("negative_T",[&]{c::compare({1,1,1,1},-1);});
 rejects("NaN_T",[&]{c::compare({1,1,1,1},std::numeric_limits<double>::quiet_NaN());});
 rejects("infinite_T",[&]{c::compare({1,1,1,1},std::numeric_limits<double>::infinity());});
 J result={{"schema","pas.windows-cost-comparison-metric-tests.v1"},{"checks",checks},{"assertions",checks.size()},{"failed",0},{"device_operations",0},{"live_ready",false}};
 std::ofstream out(argv[1],std::ios::binary);out<<result.dump(2)<<'\n';if(!out)throw std::runtime_error("test output write");
 std::cout<<"comparison metric controls="<<checks.size()<<" passed\n";return 0;
 }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 2;}}
