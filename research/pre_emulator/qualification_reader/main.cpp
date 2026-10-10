#include "reader/compiler_environment.hpp"
#include "reader/prefix_reader.hpp"
#include "reader/production_reader.hpp"
#include "reader/cost_reader.hpp"
#include "reader/narrow_preflight.hpp"
#include <iostream>
// All inputs bind existing receipts. This command cannot issue live admission.
int main(int argc,char** argv){try{
 namespace g=pas::preflight_graph;using J=nlohmann::json;
 if(argc!=4||std::filesystem::exists(argv[3]))throw std::runtime_error("workspace existing-input fresh-output required");
 g::Reader r(argv[1]);const auto input=g::w::read(argv[2],4*g::w::MiB);
 if(input.at("schema")=="pas.readonly-narrow-preflight-input.v1"){
  g::narrow::Preflight preflight(r,argv[1],input);const auto result=preflight.run();
  std::ofstream file(argv[3],std::ios::binary);file<<result.dump(2)<<'\n';if(!file)throw std::runtime_error("narrow preflight report write");
  return result.at("pass").get<bool>()?0:2;
 }
 g::w::require(input.at("schema")=="pas.readonly-build-input.v1"&&input.at("device_operations")==0,"read-only cold input scope");
 const auto commit=input.at("source_commit").get<std::string>(),mode=input.at("mode").get<std::string>();
 r.gate(input.at("gate"));
 g::verify_cooperative_build(r,input.at("evaluation"),commit,mode,input.at("stages"));
 g::verify_compiler_environment(r,input.at("compiler_environment"),input.at("environment_process"),commit,input.at("stages"));
 r.finish();J out={{"schema","pas.readonly-build-check.v1"},{"pass",true},{"source_commit",commit},{"mode",mode},
  {"files",r.count()},{"bytes",r.bytes()},{"device_operations",0},{"qualification_issued",false},{"scope","existing cold build receipts; no live admission"}};
 std::ofstream file(argv[3],std::ios::binary);file<<out.dump(2)<<'\n';if(!file)throw std::runtime_error("cold report write");return 0;
 }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 2;}}
