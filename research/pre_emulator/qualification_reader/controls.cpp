#include "reader/graph_reader.hpp"
#include <iostream>
int main(int argc,char** argv){try{
 namespace g=pas::preflight_graph;namespace fs=std::filesystem;using J=nlohmann::json;
 if(argc!=2||fs::exists(argv[1]))throw std::runtime_error("fresh report required");
 const fs::path output=fs::absolute(argv[1]),root=output.string()+".fixtures";
 if(fs::exists(root))throw std::runtime_error("fresh fixture root required");fs::create_directory(root);
 J cases=J::array();const auto check=[&](const char* id,bool pass){cases.push_back({{"id",id},{"pass",pass}});if(!pass)throw std::runtime_error(id);};
 const auto save=[&](const char* name,const std::string& value){auto p=root/name;std::ofstream f(p,std::ios::binary);f<<value;f.close();if(!f)throw std::runtime_error("fixture write");return p;};
 const auto rejects=[&](auto&& fn){try{fn();return false;}catch(const std::exception&){return true;}};
 auto empty=save("empty","");check("cng_empty_sha",g::streamed_sha256(empty,0)=="e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");
 auto abc=save("abc","abc");const auto sha="ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad";
 check("cng_abc_sha",g::streamed_sha256(abc,3)==sha);check("streamed_cap_reject",rejects([&]{g::streamed_sha256(abc,2);}));
 auto blocks=save("blocks",std::string(131073,'x'));
 check("multi_buffer_portable_parity",g::streamed_sha256(blocks,131073)==pas::evidence::sha256(blocks));
 g::Reader r(root);J pin={{"path",abc.string()},{"bytes",std::uint64_t(3)},{"sha256",sha}};
 r.bound(pin);r.bound(pin);check("deduplicate_identical_binding",r.count()==1&&r.bytes()==3);
 auto bad=pin;bad["sha256"]=std::string(64,'0');check("conflicting_binding_reject",rejects([&]{r.bound(bad);}));
 bad=pin;bad["bytes"]=std::uint64_t(4);check("wrong_size_reject",rejects([&]{r.bound(bad);}));
 bad=pin;bad["path"]=(root/".."/root.filename()/"abc").string();check("traversal_reject",rejects([&]{r.bound(bad);}));
 bad=pin;bad["path"]=abc.string()+":stream";check("ads_reject",rejects([&]{r.bound(bad);}));
 bad=pin;bad["path"]=abc.string()+".";check("trailing_dot_reject",rejects([&]{r.bound(bad);}));
 bad=pin;bad["path"]=abc.string()+" ";check("trailing_space_reject",rejects([&]{r.bound(bad);}));
 const auto outside=save("outside","xyz");fs::create_directory(root/"nested");
 check("workspace_containment_reject",rejects([&]{g::Reader isolated(root/"nested");isolated.bound(J{{"path",outside.string()},{"bytes",std::uint64_t(3)},{"sha256",g::streamed_sha256(outside,3)}});}));
 check("mixed_slash_case_comparison",g::same_path_text(fs::path("C:/Windows/System32/cmd.exe"),fs::path("c:\\WINDOWS\\system32\\CMD.EXE")));
 auto duplicate=save("duplicate.json","{\"x\":1,\"x\":2}");check("duplicate_key_reject",rejects([&]{g::w::read(duplicate);}));
 auto array=save("array.json","[]");check("nonobject_reject",rejects([&]{g::w::read(array);}));
 std::string deep;for(int i=0;i<66;++i)deep+="{\"x\":";deep+="0";deep+=std::string(66,'}');
 auto depth=save("depth.json",deep);check("valid_excessive_depth_reject",rejects([&]{g::w::read(depth);}));
 r.finish();save("abc","abd");check("post_read_mutation_reject",rejects([&]{r.finish();}));
 J report={{"schema","pas.qualification-reader-controls.v1"},{"failed",0},{"cases",cases},{"qualification_issued",false},{"device_operations",0}};
 std::ofstream f(output,std::ios::binary);f<<report.dump(2)<<'\n';if(!f)throw std::runtime_error("report write");return 0;
 }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 2;}}
