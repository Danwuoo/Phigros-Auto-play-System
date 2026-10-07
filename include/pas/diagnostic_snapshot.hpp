#pragma once
#include "pas/game_tracking.hpp"
#include <variant>

namespace pas {
// A private, owned copy of bounded current scalar evidence. No Frame, lease,
// runtime, owner, scheduler, callback, or mutable source reference is retained.
class DiagnosticSnapshot final {
 enum class Kind {decision,current_scene,filtered_scene,batch};
 Kind kind_;std::variant<DecisionSnapshot,CandidateBatch> value_;bool allow_=false;
 static void validate(const DecisionSnapshot&s){
  if(s.lines.size()>16||s.targets.size()>128||s.ui_basis.size()>512)throw std::invalid_argument("diagnostic scene bound");
  for(const auto&t:s.targets)if(t.reason.size()>512)throw std::invalid_argument("diagnostic target string bound");
 }
 static void validate(const CandidateBatch&b){
  if(b.lines.size()>16||b.candidates.size()>128||b.history_source.size()>512)throw std::invalid_argument("diagnostic batch bound");
  for(const auto&c:b.candidates)if(c.origin.size()>512)throw std::invalid_argument("diagnostic candidate string bound");
 }
 template<class T> DiagnosticSnapshot(Kind k,const T&value,bool allow=false):kind_(k),value_(value),allow_(allow){
  std::size_t bytes=sizeof(*this)+4096;
  const auto string_bytes=[&](const std::string&s){if(s.capacity()>1024)throw std::invalid_argument("diagnostic owned string capacity");bytes+=s.capacity()+1+256;};
  if(const auto*s=std::get_if<DecisionSnapshot>(&value_)){
   if(s->lines.capacity()>16||s->targets.capacity()>128)throw std::invalid_argument("diagnostic owned scene capacity");
   bytes+=s->lines.capacity()*sizeof(LineCandidate)+s->targets.capacity()*sizeof(GameTarget);string_bytes(s->ui_basis);for(const auto&t:s->targets)string_bytes(t.reason);
  }else{const auto&b=std::get<CandidateBatch>(value_);if(b.lines.capacity()>16||b.candidates.capacity()>128)throw std::invalid_argument("diagnostic owned batch capacity");bytes+=b.lines.capacity()*sizeof(LineCandidate)+b.candidates.capacity()*sizeof(TrackingCandidate);string_bytes(b.history_source);for(const auto&c:b.candidates)string_bytes(c.origin);}
  if(bytes>storage_charge)throw std::invalid_argument("diagnostic owned payload byte bound");
 }
public:
 static constexpr std::size_t storage_charge=256*1024;
 DiagnosticSnapshot(const DiagnosticSnapshot&)=delete;
 DiagnosticSnapshot&operator=(const DiagnosticSnapshot&)=delete;
 DiagnosticSnapshot(DiagnosticSnapshot&&)=default;
 DiagnosticSnapshot&operator=(DiagnosticSnapshot&&)=default;
 static DiagnosticSnapshot decision(const DecisionSnapshot&s){validate(s);return {Kind::decision,s};}
 static DiagnosticSnapshot current(const DecisionSnapshot&s){validate(s);return {Kind::current_scene,s};}
 static DiagnosticSnapshot filtered(const DecisionSnapshot&s,bool allow){validate(s);return {Kind::filtered_scene,s,allow};}
 static DiagnosticSnapshot batch(const CandidateBatch&b){validate(b);return {Kind::batch,b};}
 nlohmann::json encode()const{
  if(kind_==Kind::batch)return {{"event","complete_raw_candidate_batch"},{"batch",candidate_batch_json(std::get<CandidateBatch>(value_))}};
  const auto&s=std::get<DecisionSnapshot>(value_);if(kind_==Kind::decision)return decision_json(s);
  if(kind_==Kind::current_scene)return {{"event","current_observer_scene"},{"scene",decision_json(s)}};
  return {{"event","filtered_owner_scene"},{"allow",allow_},{"scene",decision_json(s)}};
 }
};
}
