#include "verifier.hpp"
#include <iostream>
int main(int argc,char** argv) {
    if(argc!=7||std::string_view(argv[1])!="--build-binding"||
       std::string_view(argv[3])!="--expected-sha256"||std::string_view(argv[5])!="--source-commit") {
        std::cerr<<"usage: pas_qualification_verify --build-binding FILE --expected-sha256 SHA256 --source-commit COMMIT\n";
        return 2;
    }
    try {
        pas::windows_qualification::Reader reader;
        const auto result=reader.build(argv[2],argv[4],argv[6]);
        std::cout<<result.dump(2)<<'\n';return 0;
    } catch(const std::exception& error) {
        std::cout<<nlohmann::json{{"schema","pas.windows-cold-qualification-verification.v1"},
            {"cold_build_verified",false},{"live_ready",false},{"device_operations",0},
            {"error",error.what()}}.dump(2)<<'\n';return 2;
    }
}
