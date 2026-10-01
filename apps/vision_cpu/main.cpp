#include "pas/vision_cpu.hpp"
#include <iostream>
#include <string>
int main(int argc,char** argv) {
    try {
        nlohmann::json result;
        if(argc==4&&std::string(argv[1])=="train-synthetic")
            result=pas::train_synthetic_cpu_vision(argv[2],std::stoi(argv[3]));
        else if(argc==5&&std::string(argv[1])=="train-reviewed")
            result=pas::train_reviewed_cpu_vision(argv[2],argv[3],std::stoi(argv[4]));
        else if(argc==5&&std::string(argv[1])=="prepare")
            result=pas::prepare_cpu_vision_packet(argv[2],argv[3],argv[4]);
        else if(argc==3&&std::string(argv[1])=="audit")
            result=pas::audit_cpu_vision_packet(argv[2]);
        else if(argc==4&&std::string(argv[1])=="rasterize")
            result=pas::rasterize_cpu_vision_packet(argv[2],argv[3]);
        else if(argc==5&&std::string(argv[1])=="predict")
            result=pas::predict_cpu_vision(argv[2],argv[3],argv[4]);
        else if(argc==3&&std::string(argv[1])=="self-test")
            result=pas::cpu_vision_self_test(argv[2]);
        else throw std::invalid_argument("usage: pas_vision_cpu train-synthetic new-output steps | prepare recording focus.json new-packet | audit packet | rasterize edited-packet new-packet | train-reviewed packet new-output steps | predict checkpoint-folder packet new-output | self-test new-output");
        std::cout<<result.dump(2)<<'\n';return result.contains("valid")&&!result.at("valid").get<bool>()?2:0;
    }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
