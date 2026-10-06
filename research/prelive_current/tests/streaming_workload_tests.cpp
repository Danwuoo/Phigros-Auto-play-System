#include "../streaming_workload.hpp"
#include <iostream>
int main(){try{using namespace pas::current_rails;std::vector<std::uint8_t> p(streaming_workload::bytes);auto* storage=p.data();int checks=0;
 for(int i=1;i<=243;++i){streaming_workload::render(p,i);const auto expected=cost_family_v2::image((i-1)%81,std::array{-2,0,2}[std::size_t((i-1)/81)%3],cost_family_v2::Control::positive);if(p!=expected||p.size()!=streaming_workload::bytes||p.data()!=storage)throw std::runtime_error("frozen pixel or storage parity");++checks;}
 for(int i:{244,256,257,2560,10000}){streaming_workload::render(p,i);const auto expected=cost_family_v2::image((i-1)%81,std::array{-2,0,2}[std::size_t((i-1)/81)%3],cost_family_v2::Control::positive);if(p!=expected||p.data()!=storage)throw std::runtime_error("boundary parity");++checks;}
 for(int i:{-1,0,10001,std::numeric_limits<int>::max()}){bool fail=false;try{streaming_workload::render(p,i);}catch(const std::invalid_argument&){fail=true;}if(!fail)throw std::runtime_error("invalid bound accepted");++checks;}
 std::cout<<"{\"checks\":"<<checks<<",\"pass\":true,\"all_243_variants_equal\":true,\"storage_reused\":true,\"rgb_bytes\":"<<p.size()<<",\"cost_qualification\":false}\n";return 0;
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
