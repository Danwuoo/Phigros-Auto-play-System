#include "../producer_timing.hpp"
#include <iostream>
using J=nlohmann::json;
int main(){try{using pas::cost_evidence::ProducerTiming;int checks=0;auto row=[](int i,bool owned=true){const long long due=1000000000ll+(i-1)*20000000ll;return J{{"producer_due_ns",due},{"render_start_ns",due+1000000},{"render_end_ns",due+3000000},{"capture",due+3000000},{"publication_return_ns",due+4000000},{"owner_complete",due+5000000},{"owned",owned}};};
 ProducerTiming p;for(int i=1;i<=2560;++i)p.observe(row(i,i%2==0),20);
 auto check=[&](bool b){++checks;if(!b)throw std::runtime_error("producer timing assertion");};
 check(p.count==2560&&p.overruns==0&&p.render_ms.size()==2560&&p.render_to_owner_ms.size()==1280);check(p.render_ms.front()==2&&p.lateness_ms.back()==1&&p.render_to_owner_ms.front()==4);
 {ProducerTiming t;auto r=row(1);r["render_end_ns"]=r["capture"]=1021000000;r["owner_complete"]=1022000000;r["publication_return_ns"]=1021500000;t.observe(r,20);check(t.overruns==1);}
 {ProducerTiming t;auto r=row(1);r["render_start_ns"]=999999999;bool fail=false;try{t.observe(r,20);}catch(...){fail=true;}check(fail);}
 {ProducerTiming t;auto r=row(1);r["publication_return_ns"]=1021000000;t.observe(r,20);check(t.overruns==0&&t.publication_overruns==1);}
 {ProducerTiming t;auto r=row(1);r["publication_return_ns"]=1001000000;bool fail=false;try{t.observe(r,20);}catch(...){fail=true;}check(fail);}
 for(int k=0;k<8;++k){ProducerTiming t;auto r=row(1);int cadence=20;if(k==0)r["render_start_ns"]=0;if(k==1)r["render_end_ns"]=1;if(k==2)r["capture"]=0;if(k==3)r["owner_complete"]=1;if(k==4)cadence=30;if(k==5)r["producer_due_ns"]=std::numeric_limits<long long>::max();if(k==6){t.observe(r,20);r=row(3);}if(k==7)r.erase("render_start_ns");bool rejected=false;try{t.observe(r,cadence);}catch(...){rejected=true;}check(rejected);}
 {bool fail=false;try{ProducerTiming::delta(std::numeric_limits<long long>::max(),-1);}catch(...){fail=true;}check(fail);}
 std::cout<<"{\"checks\":"<<checks<<",\"pass\":true,\"scope\":\"raw producer timing arithmetic; not measured pipeline cost\"}\n";return 0;
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
