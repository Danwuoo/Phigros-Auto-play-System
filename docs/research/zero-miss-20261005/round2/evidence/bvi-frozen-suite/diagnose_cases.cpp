// Diagnostics only: include the exact isolated adapter plus unchanged original driver,
// rename its entry point, and call the same execute() without altering expected values.
#define main frozen_suite_main
#include BVI_ADAPTER_MAIN
#undef main
int main(int argc,char** argv){try{
 if(argc!=4)throw std::runtime_error("usage: diagnose normalized typed-r1 oracle");
 auto spec=read(argv[1]),ti=read(argv[2]),oracle=read(argv[3]);
 const std::set<std::string> selected={"V01-whole","V01-touching","V02-continuation","V02-stationary-current","V04-contained-effect","V07-merge","V10-rotation","V16-regions-overflow","V16-source","V18-V08-line-order","V08-forward","V17-gap40","V17-duplicate-rgb","V17-same-descriptor-new-background"};
 J result=J::array();
 for(std::size_t ci=0;ci<spec["cases"].size();++ci){auto c=spec["cases"][ci];if(!selected.contains(c["name"].get<std::string>()))continue;c["typed_frames"]=ti["cases"][ci]["frames"];c["expected"]=oracle["cases"][ci]["expected"];
  for(const auto* layer:{"e2e","typed"}){auto run=execute(c,layer);J descriptors=J::array();for(std::size_t i=0;i<run.last.count;++i){const auto& d=run.last.parts[i];descriptors.push_back({{"left",d.left},{"right",d.right},{"front_end",d.front_end},{"rear_end",d.rear_end},{"body",name(d.body)},{"contact",name(d.contact)},{"effect",d.effect},{"signature",std::to_string(d.signature)},{"rgb_signature",std::to_string(d.rgb_signature)},{"line_id",d.line_id},{"line_unique",d.line_unique}});}
   result.push_back({{"case",c["name"]},{"layer",layer},{"expected",c["expected"]},{"last_descriptors",descriptors},{"trajectory",run.frames},{"source_geometry_last",c[layer==std::string("typed")?"typed_frames":"frames"].back()}});
  }
 }
 std::cout<<J({{"schema","bvi.cloud.unchanged-execute-diagnostics.v1"},{"core_driver_unchanged",true},{"rows",result}}).dump(2)<<'\n';return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 2;}}
