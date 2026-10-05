// Research-only input/protocol audit. Does not alter frozen inputs or expected values.
#include <nlohmann/json.hpp>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <set>
#include <stdexcept>
#include <string>
using J=nlohmann::json;
J read(const std::filesystem::path& p){std::ifstream f(p);if(!f)throw std::runtime_error("missing "+p.string());return J::parse(f);}
void require(bool b,const char* message){if(!b)throw std::runtime_error(message);}
int code(const std::filesystem::path& p){std::ifstream f(p);int n=-999;f>>n;require(bool(f),"missing native exit");return n;}
int main(int argc,char** argv){try{
 if(argc==3&&std::string(argv[1])=="--check-input"){
  const auto j=read(argv[2]);J positions=J::array();std::set<std::string> names;int arrays=0,frames=0;
  require(j.at("cases").size()==20,"new case denominator");
  for(std::size_t ci=0;ci<j["cases"].size();++ci){const auto& c=j["cases"][ci];require(c.at("id")=="R1","unexpected case family");require(c["frames"].size()==c["typed_frames"].size(),"frame mapping");
   for(std::size_t fi=0;fi<c["typed_frames"].size();++fi){++frames;const auto& line=c["typed_frames"][fi].at("lines");const auto& rgb=c["frames"][fi].at("lines");
    if(line.is_object()){
     require(line.size()==4&&line.contains("center")&&line.contains("id")&&line.contains("angle")&&line.contains("length"),"noncanonical singleton fields");
     require(line["center"].is_array()&&line["center"].size()==2&&line["center"][0].is_number()&&line["center"][1].is_number()&&line["id"].is_number_integer()&&line["angle"].is_number()&&line["length"].is_number(),"singleton value types");
     require(rgb.is_array()&&rgb.size()==1&&rgb[0]==line,"singleton not equal to corresponding RGB line");
     names.insert(c["name"].get<std::string>());
     positions.push_back({{"case",c["name"]},{"json_pointer","/cases/"+std::to_string(ci)+"/typed_frames/"+std::to_string(fi)+"/lines"},{"rgb_pointer","/cases/"+std::to_string(ci)+"/frames/"+std::to_string(fi)+"/lines/0"},{"exact_equal_to_single_rgb_line",true},{"line",line}});
    }else {require(line.is_array()&&line==rgb,"non-object line shape or RGB mismatch");++arrays;}
   }
  }
  require(frames==159&&positions.size()==156&&arrays==3&&names.size()==20,"singleton audit denominator");
  std::cout<<J({{"schema","bvi.cloud.singleton-input-audit.v1"},{"raw_input_unchanged",true},{"typed_frames",frames},{"singleton_objects",positions.size()},{"already_arrays",arrays},{"affected_cases",names.size()},{"all_line_values_exactly_equal_corresponding_rgb_line",true},{"positions",positions}}).dump(2)<<'\n';return 0;
 }
 if(argc==3&&std::string(argv[1])=="--check-results"){
  const std::filesystem::path p=argv[2];const auto negative=read(p/"wrong-contact.json"),suite=read(p/"suite.json");
  const int ne=code(p/"wrong-contact.exit"),se=code(p/"suite.exit");
  const auto& nr=negative.at("negative_rows");
  bool negative_ok=ne==1&&negative.at("status")=="cold_fail_requires_classification"&&negative.at("assertions")==2&&negative.at("failed_assertions")==2&&negative.at("consumer_rejects")==true&&nr.size()==2;
  for(int i=0;i<2&&negative_ok;++i)negative_ok=nr[i]["expected"]==4&&nr[i]["actual"]==1&&nr[i]["pass"]==false&&nr[i]["field"]==(i?"renaming_output_contact":"contact_id_transfer@0");
  require(negative_ok,"wrong-contact native/failed-row consumer rejection not proved");
  J summary={{"schema","bvi.cloud.suite-summary.v1"},{"suite_native_exit",se},{"wrong_contact_native_exit",ne},{"wrong_contact_verified",negative_ok},{"status",suite.at("status")},{"assertions",suite.at("assertions")},{"failed_assertions",suite.at("failed_assertions")},{"layers",J::object()},{"failed_rows",J::array()}};
  int cases=0,failed_cases=0,new_failures=0,rows_count=0,failed_rows_count=0;
  for(const auto* l:{"rgb","typed","lifecycle","e2e"}){const auto& layer=suite.at("layers").at(l);require(layer.at("cases_enumerated")==89&&layer.at("cases_with_assertions")==89&&layer.at("rows").size()==89,"layer denominator");int lf=0,lc=0;
   for(const auto& row:layer["rows"]){bool bad=false;for(const auto& a:row["assertions"]){++rows_count;if(!a.at("pass").get<bool>()){bad=true;++lf;++failed_rows_count;if(!row["original"].get<bool>())++new_failures;summary["failed_rows"].push_back({{"layer",l},{"case",row["case"]},{"original",row["original"]},{"assertion",a}});}}if(bad)++lc;}
   require(lf==layer.at("failed_assertions").get<int>(),"layer failed count mismatch");cases+=89;failed_cases+=lc;summary["layers"][l]={{"cases",89},{"failed_cases",lc},{"assertions",layer["assertions"]},{"failed_assertions",lf}};
  }
  for(const auto* bucket:{"supplemental","schema_negative_controls","renderer_controls"}){int count=0,fails=0;for(const auto& row:suite.at(bucket)){++count;for(const auto& a:row["assertions"]){++rows_count;if(!a["pass"].get<bool>()){++fails;++failed_rows_count;}}}summary[bucket]={{"cases",count},{"failed_assertions",fails}};}
  for(const auto& a:suite.at("contact_adapter_controls")){++rows_count;if(!a["pass"].get<bool>())++failed_rows_count;}
  require(rows_count==suite["assertions"].get<int>(),"aggregate assertion count mismatch");
  require(failed_rows_count+int(suite.at("unverified_oracle_fields").size())==suite["failed_assertions"].get<int>(),"aggregate failed count mismatch");
  summary["layer_cases"]=cases;summary["failed_layer_cases"]=failed_cases;summary["passed_layer_cases"]=cases-failed_cases;summary["new_case_failed_assertions"]=new_failures;summary["schema_errors"]=suite.at("schema_errors");summary["unverified_oracle_fields"]=suite.at("unverified_oracle_fields");
  require(suite.at("supplemental").size()==22&&suite.at("schema_negative_controls").size()==4,"control denominator");
  const bool pass=se==0&&suite["failed_assertions"]==0&&suite["schema_errors"].empty()&&suite["unverified_oracle_fields"].empty();summary["aggregate_pass"]=pass;
  std::cout<<summary.dump(2)<<'\n';return pass?0:1;
 }
 throw std::runtime_error("usage: audit_io --check-input r1-cases.json | --check-results RESULT_DIR");
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 2;}}
