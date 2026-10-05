// Frozen after legacy V08 regression, before the all-lines gate repair.
// Reuses the already frozen synthetic renderer, never historical output as gold.
#define main contact_fixture_unused_main
#include "contact_tests.cpp"
#undef main
int main(int argc,char**argv){
 if(argc!=2)return 64;Recorder r;
 for(const std::string mode:{"two_crossing","two_crossing_reverse","single_oblique","unrelated_line"}){
  Test t;t.q=hold(308,504,200);t.l=line(320,500,.04,640,8);
  std::vector<Line>ls{t.l};const bool positive=mode=="single_oblique"||mode=="unrelated_line";
  if(!positive)ls.push_back(line(320,500,0,640,7));
  if(mode=="two_crossing_reverse")std::reverse(ls.begin(),ls.end());
  if(mode=="unrelated_line")ls.push_back(line(320,100,0,640,9));
  auto rgb=render(t,ls);Key k;k.capture=k.ready=100000000;k.sequence=1;Candidate c;std::array<Query,1>qs{t.q};auto o=c.extract({rgb,W,H,W*3,k},qs,ls);c.relate(o,k,101000000);
  Guard g;g.execution="known_down";g.prefix=1;g.now=101000000;g.gate_deadline=g.plan_deadline=200000000;g.last_contact=100000000;g.attachment_query=0;g.contact_id=3;auto action=constrain(o,g);
  J row={{"id",mode},{"checks",J::array()}};r.check(row,"current_contact_preserved",true,o.parts[0].contact==Support::supported);r.check(row,"line_unique",positive,o.parts[0].line_unique);r.check(row,"move",positive,action.move);r.check(row,"no_down",false,action.down[0]);r.push(row);
 }
 J out={{"schema","bvi.v3.crossing-line-regression.v1"},{"cases",r.rows},{"assertions",r.assertions},{"failed_assertions",r.failures},{"physical_human_gold",0}};std::ofstream f(argv[1]);if(!f)return 2;f<<out.dump(2)<<'\n';return r.failures?1:0;
}
