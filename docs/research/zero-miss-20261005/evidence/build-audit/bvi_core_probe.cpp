// Independent Linux research smoke probe. It links the unchanged BVI candidate.
// This is not the frozen JSON suite, a Windows test, or a physical-owner oracle.
#include "bvi.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <iostream>
#include <limits>
#include <string_view>
#include <vector>

using namespace bvi;
namespace {
int total{}, failed{};
void check(std::string_view name, bool ok) {
  ++total;
  if (!ok) ++failed;
  std::cout << (ok ? "PASS " : "FAIL ") << name << '\n';
}
Key key(Ns t, std::uint64_t sequence) {
  Key k; k.capture = k.ready = t; k.sequence = sequence; return k;
}
Observation measured(Ns t, std::uint64_t sequence, std::uint64_t signature) {
  Observation o; o.count = 1;
  auto& d = o.parts[0]; d.q = {{32, 48}, 20, 38, 0, false};
  d.key = key(t, sequence); d.body = d.contact = Support::supported;
  d.left = d.right = d.front_end = d.line_unique = true;
  d.measured_depth = 38; d.line_id = 7; d.hit = {32, 32};
  d.signature = signature; d.rgb_signature = signature + 10000;
  return o;
}
Guard guard() { return {51000000, 200000000, 200000000, 50000000,
                        "known_down", 1, 4, 0, 4, false}; }
std::vector<std::uint8_t> image() {
  std::vector<std::uint8_t> p(64 * 64 * 3);
  auto set = [&](int x, int y, int r, int g, int b) {
    const auto i = static_cast<std::size_t>((y * 64 + x) * 3);
    p[i] = static_cast<std::uint8_t>(r);
    p[i+1] = static_cast<std::uint8_t>(g);
    p[i+2] = static_cast<std::uint8_t>(b);
  };
  for (int y = 10; y <= 48; ++y)
    for (int x = 22; x <= 42; ++x) set(x, y, 40, 190, 255);
  for (int y = 10; y <= 48; ++y) {
    set(22, y, 255, 255, 255); set(42, y, 255, 255, 255);
  }
  for (int y = 30; y <= 34; ++y)
    for (int x = 0; x < 64; ++x) set(x, y, 255, 255, 255);
  return p;
}
}
int main() {
  std::cout << "platform=Linux independent_probe=true frozen_suite=false\n"
            << "sizeof_candidate=" << sizeof(Candidate)
            << " sizeof_observation=" << sizeof(Observation)
            << " sizeof_descriptor=" << sizeof(Descriptor)
            << " metadata_bytes=" << Candidate::metadata_bytes() << '\n';
  check("capacity inclusive maxima", capacity_valid(128,16,1048576,6291456));
  check("capacity rejects region/line/metadata/probe overflow",
        !capacity_valid(129,16,1048576,6291456) &&
        !capacity_valid(128,17,1048576,6291456) &&
        !capacity_valid(128,16,1048577,6291456) &&
        !capacity_valid(128,16,1048576,6291457));
  auto pixels = image();
  std::array<Query,1> qs{{{{32,48},20,38,0,false}}};
  std::array<Line,1> ls{{{{32,32},0,64,7}}};
  View view{pixels,64,64,192,key(10000000,1)};
  Candidate extractor;
  auto rgb = extractor.extract(view,qs,ls);
  std::cout << "rgb_probes=" << rgb.probes << '\n';
  check("RGB body rails endpoints current contact", !rgb.invalid &&
        rgb.parts[0].body == Support::supported && rgb.parts[0].left &&
        rgb.parts[0].right && rgb.parts[0].front_end && rgb.parts[0].rear_end &&
        rgb.parts[0].contact == Support::supported && rgb.parts[0].line_unique &&
        rgb.parts[0].line_id == 7 && std::abs(rgb.parts[0].hit.y - 32) < 1e-12);
  auto badview = view; badview.rgb = std::span(pixels).first(pixels.size()-1);
  check("RGB byte-count invalid", extractor.extract(badview,qs,ls).invalid);
  badview = view; badview.key.source_valid = false;
  check("RGB source invalid", extractor.extract(badview,qs,ls).invalid);
  auto badqs = qs; badqs[0].width = std::numeric_limits<double>::quiet_NaN();
  check("nonfinite query invalid", extractor.extract(view,badqs,ls).invalid);
  auto badls = ls; badls[0].angle = std::numeric_limits<double>::infinity();
  check("nonfinite line invalid", extractor.extract(view,qs,badls).invalid);
  std::vector<Query> manyq(129,qs[0]);
  check("actual 129 queries fail closed", extractor.extract(view,manyq,ls).invalid);
  std::array<Line,2> two{{ls[0],{{32,32},0,64,8}}};
  auto ambiguous = extractor.extract(view,qs,two);
  check("duplicate supported lines remain nonunique", !ambiguous.invalid &&
        ambiguous.parts[0].contact == Support::supported && !ambiguous.parts[0].line_unique);
  {
    Candidate c; Observation o;
    for (int i=0;i<3;++i) {
      const Ns t=10000000+15000000LL*i; o=measured(t,i+1,i+1); c.relate(o,key(t,i+1),t+1000000);
    }
    check("three independent samples over 30ms usable", o.relations[0].usable &&
          o.relations[0].independent==3 && o.relations[0].span==30000000 && c.samples()==3);
    auto g=guard(); g.now=41000000; g.execution="never_executed"; g.prefix=0; g.attachment_query=-1;
    auto r=constrain(o,g);
    check("qualified never-executed permits one Down", r.down[0] && !r.move && !r.release);
    g.free_contacts=0;
    check("no free contacts denies new Down", !constrain(o,g).down[0]);
  }
  {
    Candidate c; Observation o;
    for (int i=0;i<3;++i) { const Ns t=10000000+15000000LL*i; o=measured(t,i+1,1); c.relate(o,key(t,i+1),t); }
    check("duplicate signatures do not accumulate confirmation", !o.relations[0].usable && o.relations[0].independent==1);
  }
  {
    Candidate c; Observation o;
    for (int i=0;i<7;++i) { const Ns t=10000000+10000000LL*i; o=measured(t,i+1,i+1); c.relate(o,key(t,i+1),t); }
    check("six-sample bound evicts oldest before output", c.samples()==6 && !c.retains(10000000) && c.window_span()==50000000);
  }
  {
    Candidate c; auto o=measured(10000000,1,1); c.relate(o,key(10000000,1),10000000);
    o=measured(50000000,2,2); c.relate(o,key(50000000,2),50000000);
    check("40ms gap included", c.samples()==2);
    o=measured(90000001,3,3); c.relate(o,key(90000001,3),90000001);
    check("40ms+1 gap resets history", c.samples()==1 && o.reason=="adjacent gap reset");
  }
  {
    Candidate c; auto o=measured(10000000,1,1); c.relate(o,key(10000000,1),109999999);
    check("source age 100ms-1 accepted", !o.context_invalid && c.samples()==1);
    Candidate d; o=measured(10000000,1,1); d.relate(o,key(10000000,1),110000000);
    check("source age 100ms rejected", o.context_invalid && d.samples()==0);
  }
  {
    Candidate c; auto o=measured(10000000,1,1); c.relate(o,key(10000000,1),10000000);
    o=measured(20000000,2,2); auto k=key(20000000,2); k.context.epoch=2; c.relate(o,k,20000000);
    check("context change revokes current qualification", o.context_invalid && c.samples()==0);
  }
  {
    Candidate c; auto o=measured(10000000,1,1); c.relate(o,key(10000000,1),10000000);
    o=measured(10000000,1,2); c.relate(o,key(10000000,1),11000000);
    check("nonincreasing key rejected", o.context_invalid && c.samples()==0);
  }
  auto o=measured(50000000,1,1); auto g=guard();
  auto r=constrain(o,g);
  check("active current body refresh uses same contact without new confirmation", r.move && r.refresh && !r.down[0] && !r.release && r.contact_id==4);
  g.execution="unknown_down"; g.receipt_unknown=true; r=constrain(o,g);
  check("unknown Down releases without replay", r.release && !r.move && !r.refresh && !r.down[0]);
  g=guard(); g.execution="completed_up"; g.prefix=2; r=constrain(o,g);
  check("completed remains complete without replay", r.completed && !r.move && !r.refresh && !r.down[0]);
  g=guard(); g.gate_deadline=g.now; r=constrain(o,g);
  check("gate deadline equality releases", r.invalid && r.release && !r.move);
  g=guard(); g.plan_deadline=g.now; r=constrain(o,g);
  check("plan deadline equality releases", r.invalid && r.release && !r.move);
  g=guard(); g.prefix=0; r=constrain(o,g);
  check("inconsistent receipt fails closed", r.invalid && r.release && !r.move);
  o.parts[0].contact=Support::absent; g=guard(); g.now=109999999;
  check("missing 60ms-1 no refresh no premature release", !constrain(o,g).refresh && !constrain(o,g).release);
  g.now=110000000;
  check("missing 60ms releases", constrain(o,g).release);
  std::cout << "assertions=" << total << " passed=" << total-failed << " failed=" << failed << '\n';
  return failed ? 1 : 0;
}
