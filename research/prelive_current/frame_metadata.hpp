#pragma once
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
