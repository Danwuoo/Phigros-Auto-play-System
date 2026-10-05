#pragma once
// Isolated cold v3 candidate, derived from research/x10d_o_bvi_build/bvi.hpp
// at acb27fdb095b82253ef9388eab52948a59663d02. Not a production integration.
#include <array>
#include <cstdint>
#include <span>
#include <string_view>
#include <vector>
namespace bvi {
using Ns=std::int64_t;
struct Point {double x{},y{};};
struct Context {std::uint64_t epoch{1},generation{1},geometry{1};int rotation{};bool operator==(const Context&)const=default;};
struct Key {Context context;Ns capture{},ready{};std::uint64_t sequence{};bool source_valid{true};};
struct View {std::span<const std::uint8_t> rgb;int width{},height{},stride{};Key key;};
struct Query {Point front;double width{},depth{},angle{};bool tap{};};
struct Line {Point center;double angle{},length{};std::uint64_t id{};};
enum class Support {absent,supported,occluded,invalid};
enum class RelationLineState : std::uint8_t { absent, unique, multiple };
struct RelationLine {Point center;double angle{},length{},separation{};std::uint64_t id{};RelationLineState state{RelationLineState::absent};};
struct Descriptor {Query q;Key key;Support body{Support::absent},contact{Support::absent};bool front_end{},rear_end{},effect{},left{},right{},line_unique{};double measured_depth{};Point rear,hit;std::uint64_t line_id{},signature{},rgb_signature{};std::uint32_t probes{};RelationLine approach;};
struct Relation {std::uint8_t alternatives{},independent{};Ns span{},first_independent{},last_independent{},freshness{};bool usable{},ambiguous{},invalid{};std::array<std::uint8_t,2> links{};};
struct Observation {std::array<Descriptor,128> parts{};std::array<Relation,128> relations{};std::array<std::uint8_t,128> canonical{};bool aliases_valid{};std::size_t count{};std::uint64_t probes{},rgb_signature{};bool invalid{},context_invalid{};std::string_view reason;};
struct Sample {std::array<Descriptor,128> parts{};std::size_t count{};Key key;};
class Candidate {
 std::array<Sample,6> history_{};std::size_t size_{};bool initialized_{};Key last_{};
public:
 Observation extract(const View&,std::span<const Query>,std::span<const Line>)const;
 void relate(Observation&,Key,Ns now);
 std::size_t samples()const{return size_;}
 Ns window_span()const{return size_>1?history_[size_-1].key.capture-history_[0].key.capture:0;}
 bool retains(Ns time)const;
 static constexpr std::size_t metadata_bytes(){return sizeof(Candidate)+sizeof(Observation);}
};
struct Guard {Ns now{},gate_deadline{},plan_deadline{},last_contact{};std::string_view execution;int prefix{},contact_id{1},attachment_query{},free_contacts{4};bool receipt_unknown{};};
struct Constraints {bool move{},refresh{},release{},invalid{},completed{},up_geometry_opportunity{};std::array<bool,128> down{};Point hit;int contact_id{};};
// New fake constraint harness. No scheduler, owner, actuator, network or injection API.
Constraints constrain(const Observation&,const Guard&);
bool capacity_valid(std::size_t regions,std::size_t lines,std::size_t metadata,std::uint64_t probes);
std::string_view name(Support);
}
