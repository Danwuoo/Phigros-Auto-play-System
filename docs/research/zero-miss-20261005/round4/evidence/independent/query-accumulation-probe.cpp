#define main independent_suite_main
#include "research/bvi_cold_v3/independent_v3_tests.cpp"
#undef main
int main(int argc,char**argv){using namespace independent_v3;std::vector<Frame>fs;int tag=1;for(double x:{318.,318.5,319.,319.5,320.}){auto f=frame(tap(x,500));f.background=tag++;fs.push_back(f);}auto r=execute(fs);const bool pass=downs(r.c)==1&&r.o.relations[0].independent>=3&&r.o.relations[0].span>=30000000;json report={{"schema","bvi-v3-independent-query-accumulation-result-v1"},{"id","P25_accumulated_half_pixel_note_translation"},{"expected_down_count",1},{"expected_minimum_independent",3},{"actual",describe(r)},{"trace",r.trace},{"pass",pass}};if(argc==2){std::ofstream o(argv[1]);o<<report.dump(2)<<'\n';}else std::cout<<report.dump(2)<<'\n';return pass?0:1;}
