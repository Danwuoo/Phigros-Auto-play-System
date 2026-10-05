#pragma once
#include "pas/game_session.hpp"
#include <cmath>
namespace r2 {
constexpr pas::Nanoseconds ms=1'000'000;
constexpr pas::Nanoseconds head_end=800*ms,visible_end=1800*ms;
// Test-only current-observation fixture. No recorded touches or runtime IDs.
// Fixed QPC-relative visible phases, independent of receipts/owner progress.
inline pas::DecisionSnapshot active_stimulus(std::uint64_t seq,pas::Nanoseconds capture,pas::Nanoseconds origin){
 pas::DecisionSnapshot s;s.sequence=seq;s.context={1,1,1,seq,capture,1280,720,1};
 const auto elapsed=capture-origin;s.playing_gate=elapsed>=0&&elapsed<visible_end;s.ui=s.playing_gate?pas::GameUi::playing:pas::GameUi::unknown;
 if(!s.playing_gate)return s;
 pas::GameTarget t;t.note_id=1;t.revision=seq;t.evidence_ns=capture;t.expires_ns=capture+100*ms;t.samples=4;t.line_id=7;
 t.note.kind=pas::NoteKind::hold;t.note.width=120;t.note.height=300;t.note.rails_geometry=true;
 const double a=elapsed/1e9*.7;t.note.tangent={std::cos(a),std::sin(a)};t.hit=t.note.center={400+60*std::sin(a),500-30*std::sin(a)};
 if(elapsed<head_end){t.note.head_on_line=true;t.crossing_ns=capture+35*ms;t.uncertainty_ns=2*ms;t.reason="prediction_observe_only";}
 else{t.note.held_body_evidence=true;t.reason="held_body_touch_only";}
 s.targets.push_back(t);return s;
}
inline nlohmann::json active_coverage(const std::vector<pas::TouchReceipt>& receipts,const std::vector<pas::ReleaseReport>& releases,std::size_t body_active_samples,std::size_t exit_contacts){
 std::size_t down=0,move=0,up=0,failed=0,unknown=0,requested=0,release_failed=0,release_unknown=0;int contact=-1;bool same_contact=true;
 for(const auto& r:receipts){if(r.command.phase==pas::Phase::down){++down;if(contact==-1)contact=r.command.contact_id;}else if(r.command.phase==pas::Phase::move)++move;else ++up;
 same_contact=same_contact&&r.command.contact_id==contact;failed+=!r.success;unknown+=r.reason.find("unknown")!=std::string::npos;}
 for(const auto& r:releases){requested+=r.requested_ids.size();release_failed+=r.failed_ids.size();release_unknown+=r.unknown_ids.size();for(auto id:r.requested_ids)same_contact=same_contact&&id==contact;}
 const bool complete=down==1&&move>=2&&up==0&&body_active_samples>=2&&same_contact&&requested==1&&!failed&&!unknown&&!release_failed&&!release_unknown&&exit_contacts==0&&!releases.empty();
 return {{"covered",complete},{"receipt_n",receipts.size()},{"down",down},{"move",move},{"up",up},{"body_active_samples",body_active_samples},{"same_contact",same_contact},{"contact_id",contact},{"failed_receipts",failed},{"unknown_receipts",unknown},{"release_n",releases.size()},{"release_requested_ids",requested},{"release_failed_ids",release_failed},{"release_unknown_ids",release_unknown},{"contacts_at_exit",exit_contacts},{"purpose","active concurrency/memory coverage; no Release latency qualification"}};
}
}
