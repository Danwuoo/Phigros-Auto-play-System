#pragma once
#include "cooperative_build.hpp"
namespace pas::preflight_graph {
inline void verify_compiler_environment(Reader& r,const J& pin,const J& process,const std::string& commit,const J& stages){
 const auto e=r.object(pin,131072);
 w::require(e.at("schema")=="pas.msvc-environment-binding.v1"&&e.at("source_commit")==commit&&
  e.at("device_operations")==0&&!e.at("live_ready").get<bool>()&&e.at("verification")==process.at("verification")&&e.at("state")==process.at("state"),"same source compiler environment proof");
 r.process(process);const auto spec=r.object(process.at("specification"));
 w::require(same_path_text(final_path(spec.at("exe").get<std::string>()),fs::path(L"C:/Windows/System32/cmd.exe"))&&
  spec.at("argv").size()==4&&spec.at("argv")[0]=="/d"&&spec.at("argv")[1]=="/s"&&spec.at("argv")[2]=="/c","actual compiler environment capture command");
 const auto env=r.object(e.at("environment"),131072);
 const std::set<std::string> names={"INCLUDE","LIB","LIBPATH","PATH","VCToolsInstallDir","VCToolsVersion","WindowsSdkDir","WindowsSDKVersion","UniversalCRTSdkDir","UCRTVersion","VSINSTALLDIR","VCINSTALLDIR","DevEnvDir","VisualStudioVersion"};
 w::require(env.at("schema")=="pas.msvc-environment.v1"&&env.at("variable_count")==14&&env.at("variables").is_object()&&
  env.at("variables").size()==names.size()&&env.at("device_operations")==0&&!env.at("live_ready").get<bool>(),"complete bounded compiler variables");
 for(const auto& [key,value]:env.at("variables").items())w::require(names.contains(key)&&value.is_string()&&
  !value.get_ref<const std::string&>().empty()&&value.get_ref<const std::string&>().size()<=8192,"compiler variable name/type/length");
 w::require(env.at("variables").at("VCToolsVersion")=="14.51.36231"&&env.at("variables").at("VisualStudioVersion")=="18.0","actual pinned compiler version");
 const auto env_path=r.bound(e.at("environment")),pin_path=r.bound(pin);
 w::require(env_path.parent_path()==final_path(spec.at("argv")[3].get<std::string>()).parent_path(),"capture output directory");
 for(const auto& stage:stages){const auto inputs=r.process(stage);w::require(inputs.contains(env_path)&&inputs.contains(pin_path),"same environment frozen in every configure/build/test stage");}
}
}
