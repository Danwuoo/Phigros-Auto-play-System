// Preserved production compile counterexample. This is expected NOT to compile:
// std::make_unique cannot deduce the untyped braced GameActionOptions argument.
#include "pas/game_session.hpp"
#include <memory>
std::unique_ptr<pas::SessionGameOwner> initial_production_construction(
 const pas::Clock& clock,pas::TouchBackend& backend,int contacts) {
 return std::make_unique<pas::SessionGameOwner>(clock,backend,contacts,
   {15,pas::Nanoseconds{35'000'000},pas::Nanoseconds{30'000'000}});
}
