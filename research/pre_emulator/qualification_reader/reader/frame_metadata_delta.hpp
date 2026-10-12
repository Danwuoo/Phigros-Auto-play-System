#pragma once
// Include at the end of component_delta.hpp AFTER its namespace closes.
namespace pas::preflight_graph::component_delta {
// Exact fixed metadata source and independent native controls.
// Existing graph/native helpers must be defined before this include.
// Performance and live qualification remain separate.
inline constexpr const char* frame_base_commit="3ee69edc85acb6b9b968424374a376f351d0e2dd";
inline constexpr const char* frame_policy="fixed_metadata_before_epoch.v1";
inline constexpr const char* frame_helper_relative="research/prelive_current/frame_metadata.hpp";
inline constexpr const char* frame_tests_relative="research/prelive_current/tests/frame_metadata_tests.cpp";
inline constexpr const char* frame_chain_relative="research/prelive_current/chain.cpp";
inline bool frame_metadata_delta(const J& d){if(!d.contains("frame_metadata_delta"))return false;w::require(d.at("frame_metadata_delta").is_object(),"explicit Frame delta object");return true;}
inline J frame_case_names(){return J::array({
 "full_public_metadata_original_baseline_1000","immutable_backend_storage_1000",
 "typed_fake_clock_no_extra_reads_or_domain_gate","latestframe_published_pixels_metadata_equal",
 "all_leases_drop_release_equal","latest_reuse_and_consumer_skips_equal",
 "invalid_sequence_payload_and_closed_equal","no_slot_feedback_or_borrowed_helper_lifetime"});}
inline constexpr const char* frame_include_old="#include \"producer_wait.hpp\"\n";
inline constexpr const char* frame_include_new="#include \"producer_wait.hpp\"\n#include \"frame_metadata.hpp\"\n";
inline constexpr const char* frame_begin_old=" const auto begin=clock.now_ns();";
inline constexpr const char* frame_begin_new=" cold::frame_metadata::FixedMetadata producer_metadata;\n const auto begin=clock.now_ns();";
inline constexpr const char* frame_loop_old="Frame f;f.sequence=i;f.epoch=f.generation=f.geometry_version=1;f.width=1280;f.height=720;f.stride=3840;f.source_rotation=1;f.capture_backend=\"offline_memory_stub\";f.capture_complete_ns=f.pixels_ready_ns=sample.render_end;";
inline constexpr const char* frame_loop_new="const auto& f=producer_metadata.for_attempt(static_cast<std::uint64_t>(i),sample.render_end);";
inline std::string frame_replace_once(std::string text,std::string_view old,std::string_view fresh){
 const auto at=text.find(old);w::require(at!=std::string::npos&&text.find(old,at+old.size())==std::string::npos,"one exact Frame source literal");
 text.replace(at,old.size(),fresh);return text;
}
inline std::string frame_expected_chain(const std::string& raw){auto text=motion_lf(raw);text=frame_replace_once(std::move(text),frame_include_old,frame_include_new);text=frame_replace_once(std::move(text),frame_begin_old,frame_begin_new);return frame_replace_once(std::move(text),frame_loop_old,frame_loop_new);}
inline void frame_source_equivalence(const std::string& base,const std::string& current){
 const auto actual=motion_lf(current);w::require(frame_expected_chain(base)==actual,"Frame chain has only three approved literal changes; original clocks/epoch/R2/actions unchanged");
 auto inverse=frame_replace_once(actual,frame_loop_new,frame_loop_old);inverse=frame_replace_once(std::move(inverse),frame_begin_new,frame_begin_old);inverse=frame_replace_once(std::move(inverse),frame_include_new,frame_include_old);
 w::require(inverse==motion_lf(base),"Frame chain inverse preserves entire original main80 body");
}
// Exact whole helper text, including defaults/no-copy/no-move/noexcept body.
// Rebind this literal only if root deliberately changes the helper source;
// never accept a caller-supplied expected-helper string or bool instead.
inline constexpr const char* frame_helper_literal=R"FRAME(#pragma once
#include "pas/core.hpp"
#include <type_traits>

// AA cold-chain metadata only. This is neither a capture/callback change nor
// an ownership, clock, geometry, deadline or acceptance-policy change.
namespace cold::frame_metadata {
class FixedMetadata final {
 pas::Frame value_;
public:
 FixedMetadata(){
  value_.epoch=value_.generation=value_.geometry_version=1;
  value_.width=1280;value_.height=720;value_.stride=3840;value_.source_rotation=1;
  value_.capture_backend="offline_memory_stub";
  // All other members retain exactly fresh Frame defaults. In particular
  // source_valid=true, published_ns=0, strings/vector empty, optionals null.
 }
 FixedMetadata(const FixedMetadata&)=delete;
 FixedMetadata& operator=(const FixedMetadata&)=delete;
 FixedMetadata(FixedMetadata&&)=delete;
 FixedMetadata& operator=(FixedMetadata&&)=delete;
 const pas::Frame& for_attempt(std::uint64_t sequence,pas::Nanoseconds render_end) noexcept {
  value_.sequence=sequence;
  value_.capture_complete_ns=render_end;
  value_.pixels_ready_ns=render_end;
  return value_;
 }
};
static_assert(!std::is_copy_constructible_v<FixedMetadata> && !std::is_move_constructible_v<FixedMetadata>);
}
)FRAME";
inline void frame_helper_equivalence(const std::string& text){w::require(motion_lf(text)==frame_helper_literal,"exact whole immutable Frame helper literal");}
inline void frame_static(const J& f,const J& d){
 w::require(f.at("schema")=="pas.cold-frame-metadata-delta.v1"&&f.at("policy")==frame_policy&&f.at("source_commit")==d.at("source_commit")&&f.at("base_source_commit")==frame_base_commit,"Frame current/main80/policy identity");
 eligibility_bool(f,"source_dirty",false);eligibility_scope(f);
 for(const auto* field:{"base_chain_source","current_chain_source","helper_source","tests_source"})motion_pin_shape(f.at(field));
 // This is the original actual main80 short-copy identity, not a read of the
 // now-mutated main workspace chain or a rewritten base55 manifest.
 w::require(eligibility_uint(f.at("base_chain_source"),"bytes")==29747&&f.at("base_chain_source").at("sha256")=="76e8a662bfdb2947349a32620277da315a2f9cbd6981a74978f1b62cc2ba3f22","preserved actual main80 chain bytes/SHA");
 const auto& c=f.at("frame_controls");w::require(c.at("target")=="frame_metadata_controls"&&c.at("schema")=="pas.cold-frame-metadata-reuse-controls.v1"&&eligibility_uint(c,"assertions")==8,"separate exact Frame8 control declaration");
}
inline void frame_metadata_declaration(const J& d,const J& p){
 w::require(frame_metadata_delta(d)==frame_metadata_delta(p),"Frame plan/build attachments agree");if(!frame_metadata_delta(d))return;
 const auto& a=d.at("frame_metadata_delta");const auto& b=p.at("frame_metadata_delta");frame_static(a,d);frame_static(b,p);
 for(const auto* field:{"schema","policy","source_commit","source_dirty","base_source_commit","base_chain_source","current_chain_source","helper_source","tests_source"})w::require(a.at(field)==b.at(field),"Frame exact static plan/build join");
 for(const auto* field:{"target","schema","assertions"})w::require(a.at("frame_controls").at(field)==b.at("frame_controls").at(field),"Frame control tuple exact plan/build join");
}
inline void frame_manifest_join(const J& f,const J& manifest){
 for(const auto& [field,relative]:std::array<std::pair<const char*,const char*>,3>{{{"current_chain_source",frame_chain_relative},{"helper_source",frame_helper_relative},{"tests_source",frame_tests_relative}}})
  w::require(pin_equal(f.at(field),eligibility_manifest_pin(manifest,relative)),"Frame current manifest exact path/bytes/SHA join");
}
inline void frame_report(const J& report,const J& d){
 eligibility_checks(report,"pas.cold-frame-metadata-reuse-controls.v1",8);
 w::require(report.at("source_build_commit")==d.at("source_commit")&&report.at("source_commit")==d.at("source_commit"),"Frame actual compiled control source identity");
 eligibility_bool(report,"source_build_dirty",false);eligibility_bool(report,"source_dirty",false);
 std::set<std::string> actual,expected;for(const auto& c:report.at("checks"))actual.insert(c.at("id").get<std::string>());for(const auto& id:frame_case_names())expected.insert(id.get<std::string>());w::require(actual==expected,"all and only separate original Frame8 cases");
 // These are the already-defined component test facts, not performance gates.
 eligibility_bool(report,"long_gap_root_cause_claim",false);eligibility_bool(report,"performance_measurement",false);eligibility_bool(report,"original_control_denominators_modified",false);
}
inline void frame_require_declared_source(bool declared,const std::string& raw){
 const auto text=motion_lf(raw);const bool used=text.find("#include \"frame_metadata.hpp\"")!=std::string::npos||text.find("producer_metadata.for_attempt(")!=std::string::npos;
 w::require(declared||!used,"Frame helper/getter source requires explicit Frame delta declaration");
}
inline bool frame_metadata_validate(Reader& r,const J& d){
 const auto read=[&](const J& pin){w::require(eligibility_uint(pin,"bytes")<=4*w::MiB,"small full Frame source");std::ifstream in(r.bound(pin),std::ios::binary);std::ostringstream s;s<<in.rdbuf();w::require(bool(in)&&s.str().size()==eligibility_uint(pin,"bytes"),"read complete bound Frame source");return s.str();};
 const auto manifest=r.object(d.at("source_manifest"));const auto chain=eligibility_manifest_pin(manifest,frame_chain_relative);const auto current_text=read(chain);
 const bool declared=frame_metadata_delta(d);frame_require_declared_source(declared,current_text);if(!declared)return false;
 const auto& f=d.at("frame_metadata_delta");frame_static(f,d);frame_manifest_join(f,manifest);
 frame_source_equivalence(read(f.at("base_chain_source")),current_text);frame_helper_equivalence(read(f.at("helper_source")));
 const auto& c=f.at("frame_controls");for(const auto* field:{"binary","report"})motion_pin_shape(c.at(field));
 w::require(pin_equal(c.at("binary"),eligibility_image(d,"frame_metadata_controls.exe")),"Frame control actual newly sealed image");
 const auto report=eligibility_control_native(r,d,c.at("process"),c.at("binary"),c.at("report"),J::array({c.at("report").at("path")}));
 // The common helper authenticates actual Native8/image/argv/full source,
 // plan/graph/all outputs/imports. Require Frame-specific sources too; no
 // caller bool or fixture Native-looking metadata may replace this call.
 const auto inputs=r.process(c.at("process"));for(const auto* field:{"base_chain_source","current_chain_source","helper_source","tests_source"})w::require(inputs.contains(r.bound(f.at(field))),"Frame control full current/original source closure");
 frame_report(report,d);return true;
}
inline void frame_link_shape(const J& d,const Graph& g){
 if(!frame_metadata_delta(d))return;const std::string image="frame_metadata_controls.exe";w::require(g.links.contains(image)&&g.link_libraries.contains(image),"actual Frame control object/link fields");
 std::string own;for(const auto& [object,source]:g.objects)if(source==frame_tests_relative){w::require(own.empty()&&object.starts_with("CMakeFiles/frame_metadata_controls.dir/"),"one Frame control own TU");own=object;}w::require(!own.empty(),"Frame own source object present");
 std::istringstream edge(g.links.at(image));std::set<std::string> actual_objects;std::string token;while(edge>>token){if(token=="|"||token=="||")break;w::require(actual_objects.insert(token).second,"no duplicate Frame control object");}
 w::require(actual_objects==std::set<std::string>{own},"Frame control owns only its TU; no game/motion object forced into image");
 const fs::path build=d.at("build_root").get<std::string>();std::set<std::string> seen;
 for(const auto& item:link_tokens(g.link_libraries.at(image))){const fs::path raw(item),resolved=raw.is_absolute()?raw:build/raw;bool named=false,matched=false;
  for(const auto& lib:d.at("imported_static_libraries")){const fs::path path=lib.at("binary").at("path").get<std::string>();named|=same_path_text(raw.filename(),path.filename());if(same_path_text(resolved.lexically_normal(),path)){matched=true;w::require(seen.insert(lib.at("role").get<std::string>()).second,"Frame library role unique");}}
  w::require(!named||matched,"no shadow Frame imported library path");}
 w::require(seen==std::set<std::string>{"cpu_pas_core"},"Frame controls link only unchanged actual CPU core role");
}

} // namespace pas::preflight_graph::component_delta
