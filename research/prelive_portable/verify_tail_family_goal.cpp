// Additional declared software goal, separate from the unchanged family
// integrity verifier: eight normal completions for each B-positive offset,
// zero normal-tail claims for baseline and negative controls. Not physical gold.
#include <nlohmann/json.hpp>
#include <filesystem>
#include <fstream>
#include <iostream>
using J=nlohmann::json;
int main(int argc,char**argv){try{if(argc!=3||std::filesystem::exists(argv[2])||std::filesystem::file_size(argv[1])>32*1024*1024)throw std::runtime_error("usage FROZEN_V2_REPORT FRESH_OUTPUT");std::ifstream in(argv[1]);const auto data=J::parse(in);J checks=J::array(),summary=J::array();int failed=0;auto check=[&](std::string id,bool pass){checks.push_back({{"id",id},{"pass",pass}});failed+=!pass;};check("complete_12_cell_family",data.at("runs").size()==12);
 for(const auto&run:data.at("runs")){const auto mode=run.at("mode").get<std::string>(),control=run.at("control").get<std::string>();const int offset=run.at("offset");const auto label=mode+"_"+control+"_"+std::to_string(offset);int normal=0,confirmed=0;for(const auto&e:run.at("owner_events")){const auto type=e.at("event").get<std::string>();normal+=type=="game_contact_up"&&e.value("reason","")=="visible_tail_completed"&&e.value("success",false);confirmed+=type=="game_hold_tail_confirmed"&&e.value("terminal_only",false);}
  const int expected=mode=="B"&&control=="positive"?8:0;check(label+"_normal_completion_count",normal==expected);check(label+"_typed_confirmation_count",confirmed==expected);summary.push_back({{"mode",mode},{"control",control},{"offset",offset},{"normal_completions",normal},{"typed_confirmations",confirmed},{"expected",expected}});
 }J out={{"schema","pas.typed-tail-frozen-family-goal.v1"},{"assertions",checks.size()},{"failed",failed},{"checks",checks},{"results",summary},{"software_goal_met",failed==0},{"physical_gold",0},{"windows_cost_qualified",false}};std::ofstream f(argv[2]);f<<out.dump(2)<<'\n';if(!f)throw std::runtime_error("write");std::cout<<"tail family goal="<<checks.size()<<" failed="<<failed<<'\n';return failed?1:0;}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 2;}}
