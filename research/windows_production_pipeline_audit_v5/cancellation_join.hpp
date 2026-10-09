#pragma once
#include <optional>
#include <limits>
// Only a terminal Up can use this path. This is a cancellation proof, never
// a replacement for an accepted Down/Move step or a game effect claim.
template<class Packets>
std::size_t verify_cancelled_up(const J& receipt,const std::vector<J>& plans,
 const std::vector<J>& cancellations,const Packets& packets,const J& active,
 const std::set<std::size_t>& used) {
 const auto& c=receipt.at("issued");const Ns due=c.at("scheduled"),start=receipt.at("injection_start_ns"),ret=receipt.at("injection_return_ns");
 w::require(c.at("phase")==2&&active.at("intent")==c.at("intent")&&active.at("contact")==c.at("contact")&&active.at("x")==c.at("x")&&active.at("y")==c.at("y"),"cancellation must release its own last verified position");
 const J* latest=nullptr;for(const auto& p:plans)if(p.at("intent")==c.at("intent")&&p.at("evidence").template get<Ns>()<=due&&(!latest||p.at("revision")>latest->at("revision")))latest=&p;
 w::require(latest&&latest->at("source_frame")==c.at("source_frame"),"cancellation latest accepted body source");
 const Ns evidence=latest->at("evidence");w::require(evidence>=0&&due>=evidence&&due-evidence<100'000'000,"cancellation source evidence lease");
 std::optional<std::size_t> matched;
 for(std::size_t i=0;i<cancellations.size();++i){const auto& cancel=cancellations[i];if(cancel.at("intent_id")!=c.at("intent"))continue;
  w::require(!matched&&!used.contains(i),"unique cancellation receipt");const auto frame=cancel.at("source_frame").template get<std::uint64_t>();w::require(packets.contains(frame),"cancellation current packet");const auto& packet=packets.at(frame);const Ns when=cancel.at("cancel_ns");
  w::require(cancel.at("contact_started").template get<bool>()&&!cancel.at("retry_without_prior_down").template get<bool>()&&cancel.at("reason")=="current_object_missing_or_region_lost"&&cancel.at("game_effect")=="unknown","explicit active contact cancellation contract");
  const bool tap=cancel.at("kind")=="tap";
  w::require(tap||cancel.at("kind")=="hold","supported active cancellation kind");
  const bool tap_basis=latest->contains("basis")&&
      (latest->at("basis")=="live_pixels_short_linear_fit_tap"||latest->at("basis")=="live_pixels_current_tap_overlap");
  w::require(tap||!tap_basis,"Hold cancellation cannot relabel an accepted Tap");
  w::require(cancel.at("note_id")==latest->at("note")&&cancel.at("last_evidence_ns")==evidence&&frame>=c.at("source_frame").template get<std::uint64_t>(),"cancellation identity/evidence");
  w::require(packet.end&&when>=packet.start&&when<=due&&due<=start&&ret<=packet.end,"cancellation QPC in current owner invocation");
  const auto cursor=cancel.at("executed_steps").template get<std::uint64_t>(),prefix=latest->at("prefix_offset").template get<std::uint64_t>();const auto& steps=latest->at("steps");
  if(tap){
   // A Tap can lose current pixels while its own Down is still active. Prove
   // its original two-step plan and the unchanged 40 ms missing-object grace;
   // this releases the contact and makes no claim about the game judgement.
   w::require(tap_basis&&
       prefix==0&&cursor==1&&steps.is_array()&&steps.size()==2,"active Tap accepted plan kind/prefix");
   const auto& down=steps[0];const auto& up=steps[1];
   const Ns down_due=down.at("due"),up_due=up.at("due");
   w::require(down.at("phase")==0&&up.at("phase")==2&&down_due>=evidence&&
       down_due<=std::numeric_limits<Ns>::max()-18'000'000&&up_due==down_due+18'000'000&&
       down.at("x")==up.at("x")&&down.at("y")==up.at("y")&&
       when>=evidence&&when-evidence>=40'000'000,"active Tap shape and missing-object grace");
  }
  w::require(cursor>prefix&&cursor-prefix<=steps.size(),"cancellation executed prefix");const auto& position=steps[std::size_t(cursor-prefix-1)];
  w::require(position.at("phase")!=2&&position.at("x")==c.at("x")&&position.at("y")==c.at("y"),"cancellation preserves executed contact position");matched=i;
 }
 w::require(matched.has_value(),"unplanned receipt lacks explicit cancellation proof");return *matched;
}
