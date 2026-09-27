#pragma once
#include "pas/game.hpp"

namespace pas {
// Exact legacy association/predictor, also used by the immutable-bank runner.
void track_legacy_batch(DecisionSnapshot& out,const std::vector<NoteCandidate>& notes,
                       const std::vector<std::optional<NoteCandidate>>& shortened_holds,
                       std::vector<GameTrackHistory>& tracks,std::uint64_t& next_id,
                       const std::vector<int>* forced_assignment=nullptr,
                       std::vector<std::size_t>* source_indices=nullptr);
CandidateBatch make_candidate_batch(const DecisionSnapshot&,const Frame&,
    const std::vector<NoteCandidate>&,const std::vector<std::optional<NoteCandidate>>&,
    const std::vector<GameTrackHistory>&);
nlohmann::json candidate_batch_json(const CandidateBatch&);
CandidateBatch parse_candidate_batch(const nlohmann::json&);

struct TrackedObservation {
    std::uint64_t track_id=0,birth_id=0,revision=0,candidate_id=0,line_id=0;
    std::string state="tentative",stage="none",reason;
    double cost=0,competition_margin=0,uncertainty_px=0;
    Vec2 estimated,velocity;
    Nanoseconds observed_ns=0,supported_ns=0;
    bool prediction_only=true,identity_supported=false,action_evidence_valid=false;
    bool head_visible=false,body_visible=false,left_rail=false,right_rail=false;
    std::optional<Vec2> tail;
    std::size_t synthetic_updates=0;
};
struct TrackingResult {
    bool capacity_valid=true;
    std::string reset_reason;
    std::vector<TrackedObservation> observations;
    DecisionSnapshot executable;
};
class GameTrackingComparison final {
public:
    explicit GameTrackingComparison(std::string method,bool observation_reupdate=false);
    TrackingResult update(const CandidateBatch&);
    void reset();
    const std::string& method() const {return method_;}
private:
    struct Axis {double p=0,v=0,p00=16,p01=0,p11=40000;};
    struct State {
        GameTrackHistory history;
        Axis x,y;
        Nanoseconds predicted_ns=0,supported_ns=0;
        std::uint64_t birth_id=0;
    };
    struct Retired {std::uint64_t birth_id;NoteCandidate appearance;Nanoseconds observed;};
    std::string method_;
    bool reupdate_=false;
    SceneContext previous_;
    std::uint64_t next_id_=0;
    std::vector<State> states_;
    std::vector<Retired> retired_;
    std::vector<GameTrackHistory> baseline_;
    bool retirement_overflow_=false;
};
nlohmann::json tracked_json(const TrackingResult&);
nlohmann::json analyze_tracking_bank(const std::filesystem::path&,const std::vector<std::string>&,
                                     int repeats=10000,bool reupdate=false);
void write_tracking_challenge(const std::filesystem::path&);
} // namespace pas
