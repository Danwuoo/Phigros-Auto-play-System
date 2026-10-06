#include "research/prelive_current/producer_timing.hpp"
#include <iostream>
using J=nlohmann::json;
int main(){pas::cost_evidence::ProducerTiming original,shifted;for(int i=0;i<3;++i){long long due=1000000000ll+i*20000000ll;J row={{"producer_due_ns",due},{"render_start_ns",due+1000000},{"render_end_ns",due+21000000},{"capture",due+21000000},{"owner_complete",due+22000000},{"owned",true}};original.observe(row,20);row["producer_due_ns"]=due+1000000000ll;shifted.observe(row,20);}std::cout<<J{{"original_overruns",original.overruns},{"shifted_overruns",shifted.overruns},{"shifted_lateness_ms",shifted.lateness_ms},{"counterexample_only",true}}.dump()<<'\n';}
