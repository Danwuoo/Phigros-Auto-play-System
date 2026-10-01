#include "pas/vision_cpu.hpp"
#include "pas/adb.hpp"
#include "pas/analysis.hpp"
#include "pas/game_dataset.hpp"
#include <torch/torch.h>
#include <torch/version.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <fstream>
#include <iostream>
#include <map>
#include <random>
#include <set>
#include <sstream>

namespace pas {
namespace {
using json=nlohmann::json;namespace fs=std::filesystem;
constexpr int classes=9,side=64,batch_cap=4,input_cap=256;
constexpr std::array<const char*,classes> names{"background","line_appearance","tap_core",
    "hold_head","hold_body","hold_tail","drag_core","flick_core","hit_effect"};
constexpr std::array<std::array<std::uint8_t,3>,classes> colors{{{0,0,0},{255,255,255},
    {40,255,80},{255,130,20},{30,190,255},{190,40,255},{255,255,40},{255,50,150},{255,170,40}}};
json read_json(const fs::path& p) {
    if(fs::file_size(p)>1024*1024)throw std::invalid_argument("vision JSON capacity 1 MiB");
    std::ifstream in(p,std::ios::binary);return json::parse(in);
}
void save(const fs::path& p,const json& j) {
    if(fs::exists(p))throw std::invalid_argument("immutable output exists");
    std::ofstream out(p,std::ios::binary);out<<j.dump(2)<<'\n';if(!out)throw std::runtime_error("vision output write");
}
void new_folder(const fs::path& p) {
    if(fs::exists(p))throw std::invalid_argument("output folder exists");fs::create_directories(p);
}
fs::path bounded_path(const fs::path& root,const std::string& value) {
    fs::path p(value);if(p.empty()||p.is_absolute()||p.has_root_name())throw std::invalid_argument("relative path required");
    for(auto& part:p)if(part=="..")throw std::invalid_argument("escaping data path");
    const auto base=fs::weakly_canonical(root),resolved=fs::weakly_canonical(root/p),r=resolved.lexically_relative(base);
    if(r.empty()||*r.begin()=="..")throw std::invalid_argument("linked path leaves packet");return resolved;
}
void initialize() {
    static const bool initialized=[] {torch::set_num_threads(4);torch::set_num_interop_threads(1);return true;}();
    (void)initialized;
}
struct PartNetImpl:torch::nn::Module {
    torch::nn::Conv2d a{nullptr},b{nullptr},c{nullptr},d{nullptr},head{nullptr};
    PartNetImpl() {
        a=register_module("a",torch::nn::Conv2d(torch::nn::Conv2dOptions(3,12,3).padding(1)));
        b=register_module("b",torch::nn::Conv2d(torch::nn::Conv2dOptions(12,12,3).padding(2).dilation(2)));
        c=register_module("c",torch::nn::Conv2d(torch::nn::Conv2dOptions(12,12,3).padding(4).dilation(4)));
        d=register_module("d",torch::nn::Conv2d(torch::nn::Conv2dOptions(12,12,3).padding(8).dilation(8)));
        head=register_module("head",torch::nn::Conv2d(torch::nn::Conv2dOptions(12,classes,1)));
    }
    torch::Tensor forward(torch::Tensor x) {
        if(x.device().type()!=torch::kCPU||x.scalar_type()!=torch::kFloat32||x.dim()!=4||x.size(1)!=3||
            x.size(0)<1||x.size(0)>batch_cap||x.size(2)<1||x.size(3)<1||x.size(2)>input_cap||x.size(3)>input_cap)
            throw std::invalid_argument("CPU FP32 RGB / batch4 / 256x256 input contract");
        return head->forward(torch::relu(d->forward(torch::relu(c->forward(torch::relu(b->forward(torch::relu(a->forward(x)))))))));
    }
};
TORCH_MODULE(PartNet);
torch::Tensor input(const Frame& f) {
    if(f.width<1||f.height<1||f.width>input_cap||f.height>input_cap||f.stride!=f.width*3||
        f.rgb.size()!=static_cast<std::size_t>(f.width)*f.height*3)throw std::invalid_argument("native RGB tensor shape");
    return torch::from_blob(const_cast<std::uint8_t*>(f.rgb.data()),{f.height,f.width,3},torch::kUInt8)
        .clone().permute({2,0,1}).to(torch::kFloat32).div(255).unsqueeze(0);
}
torch::Tensor labels(const LabelMask& m) {
    return torch::from_blob(const_cast<std::uint16_t*>(m.values.data()),{m.height,m.width},torch::kInt16)
        .clone().to(torch::kInt64).unsqueeze(0);
}
torch::Tensor loss(torch::Tensor logits,torch::Tensor truth) {
    const auto weights=torch::tensor({.10f,2.f,2.f,3.f,1.f,3.f,2.f,2.f,1.f});
    return torch::nn::functional::cross_entropy(logits,truth,
        torch::nn::functional::CrossEntropyFuncOptions().weight(weights).ignore_index(255));
}
Frame crop(const Frame& f,int x,int y,int w,int h) {
    if(x<0||y<0||w<1||h<1||w>input_cap||h>input_cap||x+w>f.width||y+h>f.height)
        throw std::invalid_argument("native ROI bound");
    Frame out;out.width=w;out.height=h;out.stride=w*3;out.rgb.resize(static_cast<std::size_t>(w)*h*3);
    for(int row=0;row<h;++row)std::copy_n(f.rgb.data()+static_cast<std::size_t>(y+row)*f.stride+x*3,w*3,out.rgb.data()+static_cast<std::size_t>(row)*out.stride);
    return out;
}
void validate_objects(const json& a,const LabelMask& semantic,const LabelMask& instance) {
    if(!a.at("objects").is_array()||a.at("objects").size()>256)throw std::invalid_argument("object array cap256");
    std::map<int,std::string> objects;
    for(const auto& object:a.at("objects")) {
        const auto id=object.at("instance").get<int>();const auto type=object.at("type").get<std::string>();
        if(id<=0||id>=65535||!objects.emplace(id,type).second)throw std::invalid_argument("object instance/duplicate");
        if(type!="hold"&&type!="tap"&&type!="drag"&&type!="flick"&&type!="line_appearance")throw std::invalid_argument("object type");
        if(object.value("amodal",true)||!object.at("truncated").is_boolean()||!object.at("visibility").is_string()||
            !object.at("type_uncertainty").is_string())throw std::invalid_argument("visible-only object metadata");
        if(type=="line_appearance") {
            if(object.at("line_role")!="unknown")throw std::invalid_argument("this pixel pilot cannot certify actionable line role");
            const auto& endpoints=object.at("centerline");if(!endpoints.is_array()||endpoints.size()!=2)throw std::invalid_argument("line centerline");
            for(const auto& p:endpoints)if(!p.is_array()||p.size()!=2||!std::isfinite(p[0].get<double>())||!std::isfinite(p[1].get<double>())||
                p[0].get<double>()<0||p[0].get<double>()>semantic.width||p[1].get<double>()<0||p[1].get<double>()>semantic.height)
                throw std::invalid_argument("line endpoint bound");
        }
    }
    for(std::size_t i=0;i<semantic.values.size();++i) {
        const int cls=semantic.values[i],id=instance.values[i];if(cls==255||cls==0||cls==8)continue;
        const auto expected=cls==1?"line_appearance":cls==2?"tap":cls<=5?"hold":cls==6?"drag":"flick";
        if(!objects.contains(id)||objects.at(id)!=expected)throw std::invalid_argument("pixel instance missing or inconsistent object type");
    }
}
struct Example {Frame rgb;LabelMask semantic;};
// Independent rasterizer truth for ENGINEERING bootstrap, never game gold.
Example synthetic(std::mt19937& rng) {
    Example e;e.rgb.width=e.rgb.height=side;e.rgb.stride=side*3;e.rgb.rgb.resize(side*side*3);
    e.semantic={side,side,8,std::vector<std::uint16_t>(side*side,0)};
    auto uniform=[&](int lo,int hi){return std::uniform_int_distribution<int>(lo,hi)(rng);};
    const int bg=uniform(12,55);
    for(int y=0;y<side;++y)for(int x=0;x<side;++x){const int v=std::clamp(bg+uniform(-6,6)+y/12,0,255);
        for(int ch=0;ch<3;++ch)e.rgb.rgb[(y*side+x)*3+ch]=static_cast<std::uint8_t>(v+(ch==2?8:0));}
    auto box=[&](double cx,double cy,double ux,double uy,double w,double h,int cls,std::array<int,3> rgb) {
        for(int y=0;y<side;++y)for(int x=0;x<side;++x){const double dx=x+.5-cx,dy=y+.5-cy;
            if(std::abs(dx*ux+dy*uy)<=w/2&&std::abs(-dx*uy+dy*ux)<=h/2){const auto i=y*side+x;
                e.semantic.values[i]=static_cast<std::uint16_t>(cls);for(int ch=0;ch<3;++ch)e.rgb.rgb[i*3+ch]=static_cast<std::uint8_t>(std::clamp(rgb[ch]+uniform(-8,8),0,255));}}
    };
    const double angle=uniform(0,179)*3.141592653589793/180,ux=std::cos(angle),uy=std::sin(angle),nx=-uy,ny=ux;
    box(32+uniform(-8,8),32+uniform(-8,8),ux,uy,90,uniform(1,3),1,{240,240,238});
    const int kind=uniform(0,3),cx=uniform(20,44),cy=uniform(20,44),width=uniform(19,32);
    if(kind==0)box(cx,cy,ux,uy,width,5,2,{40,185,245});
    else if(kind==1) {
        const int height=uniform(18,36),gray=uniform(0,1);
        box(cx-nx*height/2,cy-ny*height/2,ux,uy,width,height,4,gray?std::array<int,3>{143,159,164}:std::array<int,3>{45,156,204});
        box(cx,cy,ux,uy,width,4,3,{45,185,245});
        box(cx-nx*height,cy-ny*height,ux,uy,width,3,5,{170,220,245});
        for(int sign:{-1,1})box(cx+sign*ux*(width/2)-nx*height/2,cy+sign*uy*(width/2)-ny*height/2,nx,ny,height,1.5,4,{224,225,224});
    }else if(kind==2)box(cx,cy,ux,uy,width,4,6,{240,222,60});
    else {
        box(cx,cy,ux,uy,width,5,7,{235,65,140});
        // White arrow is part of the Flick appearance, not a new line.
        box(cx,cy,nx,ny,10,2,7,{235,235,235});
    }
    if(uniform(0,3)==0) {
        const int fx=uniform(8,55),fy=uniform(8,55),radius=uniform(4,10);
        for(int y=0;y<side;++y)for(int x=0;x<side;++x){const double r=std::hypot(x+.5-fx,y+.5-fy);
            if(std::abs(r-radius)<1.3||r<2){const int i=y*side+x;e.semantic.values[i]=8;for(int ch=0;ch<3;++ch)e.rgb.rgb[i*3+ch]=static_cast<std::uint8_t>(ch==2?110:210);}}
    }
    return e;
}
std::uint64_t parameters(const PartNet& net) {std::uint64_t n=0;for(const auto& p:net->parameters())n+=p.numel();return n;}
json evaluate(PartNet& net,int count=64) {
    std::mt19937 rng(0x895714);std::array<std::array<std::uint64_t,classes>,classes> matrix{};
    net->eval();torch::NoGradGuard no_grad;std::vector<double> costs;HostClock clock;int failures=0;
    for(int sample=0;sample<count;++sample){auto e=synthetic(rng);const auto x=input(e.rgb);const auto begin=clock.now_ns();
        const auto logits=net->forward(x);if(!torch::isfinite(logits).all().item<bool>())++failures;
        auto predicted=logits.argmax(1).contiguous();costs.push_back((clock.now_ns()-begin)/1e6);
        const auto* data=predicted.data_ptr<std::int64_t>();for(int i=0;i<side*side;++i)++matrix[e.semantic.values[i]][data[i]];
    }
    json per=json::array();double sum=0;int valid=0;
    for(int cls=0;cls<classes;++cls){std::uint64_t truth=0,pred=0;for(int j=0;j<classes;++j){truth+=matrix[cls][j];pred+=matrix[j][cls];}
        const auto denominator=truth+pred-matrix[cls][cls];json iou=nullptr;if(denominator){const double v=static_cast<double>(matrix[cls][cls])/denominator;iou=v;sum+=v;++valid;}
        per.push_back({{"id",cls},{"name",names[cls]},{"true_pixels",truth},{"predicted_pixels",pred},{"iou",iou}});
    }
    return {{"frames",count},{"confusion",matrix},{"classes",per},{"macro_iou",valid?json(sum/valid):json(nullptr)},
        {"inference_ms",distribution(costs)},{"nonfinite_failures",failures},{"scope","independent-seed synthetic renderer; no real game labels or cross-song validation"}};
}
void output_mask(const fs::path& root,const Frame& rgb,const LabelMask& mask) {
    write_label_png(root/"proposed-semantic.png",mask);auto overlay=rgb;
    for(std::size_t i=0;i<mask.values.size();++i)if(mask.values[i]!=255)for(int c=0;c<3;++c)
        overlay.rgb[i*3+c]=static_cast<std::uint8_t>((overlay.rgb[i*3+c]*2+colors[mask.values[i]][c])/3);
    write_diagnostic_png(root/"proposed-overlay.png",overlay);
}
json model_metadata(PartNet& net,const std::string& source) {
    return {{"schema",1},{"model_kind","single-frame dilated part segmentation"},{"torch_version",TORCH_VERSION},
        {"device","cpu"},{"dtype","float32"},{"threads",4},{"interop_threads",1},{"parameter_count",parameters(net)},
        {"classes",names},{"input","native RGB888 top-down / 255; no position, clock, song, chart or action features"},
        {"maximum_batch",batch_cap},{"maximum_input_side",input_cap},{"receptive_field_px",31},
        {"training_source",source},{"runtime_feedback",false},{"real_input_backend_constructed",false},
        {"line_role","appearance proposal; true/fake/actionable role remains separate and unknown"},
        {"architecture_scope","small engineering prototype; not the proposed 1-5M parameter production model"},
        {"real_game_accuracy",nullptr},{"live_enabled",false}};
}
json train(PartNet& net,const fs::path& output,int steps,const std::vector<Example>* reviewed=nullptr) {
    if(steps<1||steps>2000)throw std::invalid_argument("steps must be 1..2000");
    new_folder(output);const auto untrained=evaluate(net);torch::optim::Adam optimizer(net->parameters(),torch::optim::AdamOptions(.003));
    std::mt19937 rng(0x123455);HostClock clock;const auto start=clock.now_ns();std::vector<double> costs;
    json history=json::array();int completed=0;
    for(int step=0;step<steps;++step) {
        if(clock.now_ns()-start>240'000'000'000LL)break;
        std::vector<torch::Tensor> xs,ys;for(int i=0;i<batch_cap;++i){const auto e=reviewed?reviewed->at((step*batch_cap+i)%reviewed->size()):synthetic(rng);xs.push_back(input(e.rgb));ys.push_back(labels(e.semantic));}
        auto x=torch::cat(xs),y=torch::cat(ys);net->train();const auto begin=clock.now_ns();optimizer.zero_grad();
        auto value=loss(net->forward(x),y);if(!torch::isfinite(value).item<bool>())throw std::runtime_error("nonfinite CPU loss");
        value.backward();for(const auto& p:net->parameters())if(!torch::isfinite(p.grad()).all().item<bool>())throw std::runtime_error("nonfinite CPU gradient");
        optimizer.step();costs.push_back((clock.now_ns()-begin)/1e6);++completed;
        if(step%25==0||step==steps-1){history.push_back({{"step",step+1},{"loss",value.item<double>()}});std::cout<<"CPU step "<<step+1<<'/'<<steps<<" loss="<<value.item<double>()<<'\n'<<std::flush;}
    }
    net->eval();torch::save(net,(output/"model.pt").string());
    auto meta=model_metadata(net,reviewed?"human reviewed visible pixels development only":"synthetic renderer oracle only");
    meta["checkpoint_sha256"]=sha256_file(output/"model.pt");meta["seed"]=1337;meta["train_renderer_seed"]=0x123455;
    meta["real_training_samples"]=reviewed?reviewed->size():0;save(output/"model.json",meta);
    json report={{"requested_steps",steps},{"completed_steps",completed},{"completed",completed==steps},{"training_ms",(clock.now_ns()-start)/1e6},
        {"step_ms",distribution(costs)},{"training_failures",0},{"history",history},{"capacity",{{"wall_limit_s",240},{"batch",batch_cap},{"model_checkpoint_bytes",fs::file_size(output/"model.pt")}}},
        {"synthetic_untrained",untrained},{"synthetic_validation",evaluate(net)},{"real_game_validation",nullptr},{"human_gold_added",0},{"model",meta}};
    save(output/"training.json",report);return report;
}
} // namespace

json train_synthetic_cpu_vision(const fs::path& output,int steps) {
    initialize();torch::manual_seed(1337);PartNet net;return train(net,output,steps);
}

json prepare_cpu_vision_packet(const fs::path& recording,const fs::path& focus,const fs::path& output) {
    const auto f=read_json(focus);if(f.at("samples").empty()||f.at("samples").size()>24)throw std::invalid_argument("focus cap24");
    const auto summary=read_json(recording/"summary.json");if(sha256_file(recording/"index.jsonl")!=summary.at("index_sha256").get<std::string>())throw std::invalid_argument("recording index SHA");
    if(fs::file_size(recording/"index.jsonl")>64*1024*1024)throw std::invalid_argument("recording index byte cap");
    std::vector<json> index;std::ifstream in(recording/"index.jsonl");std::string row;
    while(std::getline(in,row)){if(index.size()>=36000||row.size()>64*1024)throw std::invalid_argument("recording index row cap");index.push_back(json::parse(row));}
    // Validate all focus references before creating output.
    for(const auto& s:f.at("samples")){const auto ordinal=s.at("ordinal").get<std::size_t>();const auto& e=index.at(ordinal);
        if(sha256_file(bounded_path(recording,e.at("path").get<std::string>()))!=e.at("png_sha256").get<std::string>())throw std::invalid_argument("focus PNG SHA");}
    new_folder(output);json samples=json::array();std::set<std::uint64_t> unique;
    for(const auto& s:f.at("samples")){const auto ordinal=s.at("ordinal").get<std::size_t>();if(!unique.insert(ordinal).second)throw std::invalid_argument("duplicate focus frame");
        const auto& e=index.at(ordinal);const auto source=bounded_path(recording,e.at("path").get<std::string>());const auto pixels=load_diagnostic_png(source);
        const int x=s.at("x"),y=s.at("y"),w=s.at("width"),h=s.at("height");const auto rgb=crop(pixels,x,y,w,h);
        const auto folder="sample-"+std::to_string(ordinal);fs::create_directories(output/folder);write_diagnostic_png(output/folder/"image.png",rgb);
        const auto image_hash=sha256_file(output/folder/"image.png");
        json annotation={{"schema",1},{"image_sha256",image_hash},{"review_status","proposed"},{"reviewer",""},{"objects",json::array()},
            {"polygons",json::array()},{"relations",json::array()},{"track_links",json::array()},{"source_png",fs::absolute(source).generic_string()},
            {"source_sha256",e.at("png_sha256")},{"roi",{{"x",x},{"y",y},{"width",w},{"height",h},{"scale",1}}},
            {"note",s.at("reason")},{"unknown_contract","unlabelled pixels 255; no invisible head/tail completion; no action or runtime-ID gold"}};
        save(output/folder/"annotation.json",annotation);rasterize_annotation(output/folder,annotation);
        samples.push_back({{"folder",folder},{"ordinal",ordinal},{"source_frame",e.at("source_frame")},{"capture_complete_ns",e.at("capture_complete_ns")},
            {"source_png",fs::absolute(source).generic_string()},{"source_sha256",e.at("png_sha256")},{"image_sha256",image_hash},
            {"roi",annotation.at("roi")},{"song_family",f.at("song_family")},{"split","development"},{"reason",s.at("reason")}});
    }
    const json result={{"schema",1},{"dataset_id","cpu-vision-pilot-v1"},{"samples",samples},{"classes",names},
        {"recording_index_sha256",sha256_file(recording/"index.jsonl")},{"focus_sha256",sha256_file(focus)},
        {"human_gold",0},{"training_ready",false},{"split_policy","entire chart family is development; adjacent frames never create a validation set"},
        {"maximum_samples",24},{"maximum_native_roi_side",input_cap},{"line_role_truth","unknown unless independently reviewed"}};
    save(output/"manifest.json",result);return result;
}

json audit_cpu_vision_packet(const fs::path& packet) {
    const auto manifest=read_json(packet/"manifest.json");if(manifest.at("schema")!=1||manifest.at("samples").empty()||manifest.at("samples").size()>24)
        throw std::invalid_argument("packet schema/count");
    std::map<std::string,std::string> families;std::set<std::string> folders,hashes;json errors=json::array();int reviewed=0;std::uint64_t labeled=0;
    for(const auto& s:manifest.at("samples"))try {
        const auto folder_name=s.at("folder").get<std::string>();if(!folders.insert(folder_name).second)throw std::invalid_argument("duplicate folder");const auto folder=bounded_path(packet,folder_name);
        const auto hash=sha256_file(folder/"image.png");if(hash!=s.at("image_sha256").get<std::string>())throw std::invalid_argument("packet RGB SHA");
        if(!hashes.insert(hash).second)throw std::invalid_argument("duplicate RGB sample");
        const auto a=read_json(folder/"annotation.json");if(a.at("image_sha256").get<std::string>()!=hash)throw std::invalid_argument("annotation RGB SHA");
        if(sha256_file(s.at("source_png").get<std::string>())!=s.at("source_sha256").get<std::string>())throw std::invalid_argument("original PNG changed");
        const auto family=s.at("song_family").get<std::string>(),split=s.at("split").get<std::string>();
        if(family.empty()||split!="development")throw std::invalid_argument("pilot is development only; no cross-song validation");
        if(families.contains(family)&&families[family]!=split)throw std::invalid_argument("family split leakage");families[family]=split;
        const auto rgb=load_diagnostic_png(folder/"image.png");if(rgb.width>input_cap||rgb.height>input_cap)throw std::invalid_argument("ROI side cap");
        if(a.at("schema")!=1||a.at("source_sha256")!=s.at("source_sha256")||a.at("source_png")!=s.at("source_png")||a.at("roi")!=s.at("roi"))
            throw std::invalid_argument("annotation provenance mismatch");
        const auto& roi=s.at("roi");if(roi.at("scale")!=1)throw std::invalid_argument("native crop scale must be one");
        const auto source=load_diagnostic_png(s.at("source_png").get<std::string>());
        const auto expected=crop(source,roi.at("x"),roi.at("y"),roi.at("width"),roi.at("height"));
        if(rgb.width!=expected.width||rgb.height!=expected.height||rgb.rgb!=expected.rgb)throw std::invalid_argument("native crop pixels differ from source");
        const auto [semantic,instance]=annotation_masks(rgb.width,rgb.height,a);validate_objects(a,semantic,instance);
        const auto disk_semantic=read_label_png(folder/"semantic.png",8),disk_instance=read_label_png(folder/"instance.png",16);
        if(disk_semantic.width!=rgb.width||disk_semantic.height!=rgb.height||disk_instance.width!=rgb.width||disk_instance.height!=rgb.height||
            disk_semantic.values!=semantic.values||disk_instance.values!=instance.values)throw std::invalid_argument("polygons and saved masks differ");
        const auto status=a.at("review_status").get<std::string>();if(status!="proposed"&&status!="unknown"&&status!="reviewed_human")throw std::invalid_argument("review status");
        const auto count=std::count_if(semantic.values.begin(),semantic.values.end(),[](auto c){return c!=255;});
        if(a.at("review_status")=="reviewed_human"&&!a.value("reviewer",std::string{}).empty()&&count>0){++reviewed;labeled+=count;}
    }catch(const std::exception& e){errors.push_back({{"folder",s.at("folder")},{"error",e.what()}});}
    return {{"valid",errors.empty()},{"samples",manifest.at("samples").size()},{"reviewed_human_samples",reviewed},
        {"human_reviewed_labeled_pixels",labeled},{"development_training_ready",errors.empty()&&reviewed==manifest.at("samples").size()&&labeled>0},
        {"cross_song_validation_ready",false},{"families",families},{"errors",errors},{"proposed_or_unknown_are_not_gold",true}};
}

json rasterize_cpu_vision_packet(const fs::path& packet,const fs::path& output) {
    auto manifest=read_json(packet/"manifest.json");if(manifest.at("schema")!=1||manifest.at("samples").empty()||manifest.at("samples").size()>24)
        throw std::invalid_argument("packet schema/count");
    std::set<std::string> folders;
    for(const auto& s:manifest.at("samples")) {
        const auto name=s.at("folder").get<std::string>();if(!folders.insert(name).second)throw std::invalid_argument("duplicate folder");
        const auto folder=bounded_path(packet,name);const auto a=read_json(folder/"annotation.json");const auto rgb=load_diagnostic_png(folder/"image.png");
        if(fs::file_size(folder/"image.png")>1024*1024||sha256_file(folder/"image.png")!=s.at("image_sha256").get<std::string>()||
            a.at("image_sha256")!=s.at("image_sha256")||rgb.width>input_cap||rgb.height>input_cap)throw std::invalid_argument("rasterize RGB contract");
        const auto [semantic,instance]=annotation_masks(rgb.width,rgb.height,a);validate_objects(a,semantic,instance);
    }
    new_folder(output);
    for(const auto& s:manifest.at("samples")) {
        const auto name=s.at("folder").get<std::string>();const auto source=bounded_path(packet,name);fs::create_directories(output/name);
        fs::copy_file(source/"image.png",output/name/"image.png");const auto a=read_json(source/"annotation.json");save(output/name/"annotation.json",a);
        rasterize_annotation(output/name,a);
    }
    manifest["rasterized_from_manifest_sha256"]=sha256_file(packet/"manifest.json");manifest["human_gold"]=0;manifest["training_ready"]=false;
    save(output/"manifest.json",manifest);const auto qa=audit_cpu_vision_packet(output);save(output/"audit.json",qa);return qa;
}

json train_reviewed_cpu_vision(const fs::path& packet,const fs::path& output,int steps) {
    const auto qa=audit_cpu_vision_packet(packet);if(!qa.at("development_training_ready").get<bool>())
        throw std::invalid_argument("No complete human-reviewed pixel set; proposed/unknown cannot train as gold");
    initialize();torch::manual_seed(1337);std::vector<Example> examples;
    const auto manifest=read_json(packet/"manifest.json");for(const auto& s:manifest.at("samples")){
        const auto folder=bounded_path(packet,s.at("folder").get<std::string>());auto rgb=load_diagnostic_png(folder/"image.png");const auto a=read_json(folder/"annotation.json");
        auto [semantic,instance]=annotation_masks(rgb.width,rgb.height,a);(void)instance;
        // Batch shapes and training memory remain bounded. Partial labels in
        // a native 64x64 tile are used; never resize tiny line/head labels.
        for(int y=0;y+side<=rgb.height;y+=side)for(int x=0;x+side<=rgb.width;x+=side){Example e;e.rgb=crop(rgb,x,y,side,side);e.semantic={side,side,8,std::vector<std::uint16_t>(side*side)};
            for(int r=0;r<side;++r)std::copy_n(semantic.values.data()+(y+r)*rgb.width+x,side,e.semantic.values.data()+r*side);
            if(std::any_of(e.semantic.values.begin(),e.semantic.values.end(),[](auto c){return c!=255;})){if(examples.size()==512)throw std::invalid_argument("training tile cap512");examples.push_back(std::move(e));}}
    }
    if(examples.empty())throw std::invalid_argument("No labeled native 64x64 tile");PartNet net;
    const auto result=train(net,output,steps,&examples);save(output/"reviewed-source.json",{{"manifest_sha256",sha256_file(packet/"manifest.json")},{"qa",qa},{"tile_count",examples.size()}});return result;
}

json predict_cpu_vision(const fs::path& checkpoint_folder,const fs::path& packet,const fs::path& output) {
    initialize();const auto meta=read_json(checkpoint_folder/"model.json");const auto checkpoint=checkpoint_folder/"model.pt";
    if(meta.at("device")!="cpu"||meta.at("classes")!=json(names)||fs::file_size(checkpoint)>1024*1024||sha256_file(checkpoint)!=meta.at("checkpoint_sha256").get<std::string>())
        throw std::invalid_argument("checkpoint CPU/class/hash/byte contract");
    const auto qa=audit_cpu_vision_packet(packet);if(!qa.at("valid").get<bool>())throw std::invalid_argument("invalid prediction packet");
    PartNet net;torch::load(net,checkpoint.string());net->eval();torch::NoGradGuard no_grad;
    new_folder(output);const auto manifest=read_json(packet/"manifest.json");json rows=json::array();std::vector<double> costs;HostClock clock;int failures=0;
    std::ofstream html(output/"review.html",std::ios::binary);html<<"<!doctype html><meta charset=utf-8><title>CPU visual proposals</title><style>body{font:16px system-ui;background:#151921;color:#eee;margin:24px}section{margin:20px 0}img{width:256px;image-rendering:auto}a{color:#7edfff}</style><h1>CPU 視覺提議</h1><p>模型訓練來源請見 predictions.json。這些是未經校準的分割提議，不能代表真正判定線、物理身分或可執行觸控。</p>";
    for(const auto& s:manifest.at("samples")){const auto name=s.at("folder").get<std::string>();const auto folder=bounded_path(packet,name);const auto rgb=load_diagnostic_png(folder/"image.png");
        const auto x=input(rgb);for(int i=0;i<3;++i)net->forward(x);const auto begin=clock.now_ns();auto probabilities=torch::softmax(net->forward(x),1).squeeze(0).contiguous();
        costs.push_back((clock.now_ns()-begin)/1e6);if(!torch::isfinite(probabilities).all().item<bool>())++failures;
        LabelMask mask{rgb.width,rgb.height,8,std::vector<std::uint16_t>(static_cast<std::size_t>(rgb.width)*rgb.height,255)};std::array<std::uint64_t,classes+1> counts{};
        const auto* p=probabilities.data_ptr<float>();for(std::size_t i=0;i<mask.values.size();++i){int best=0;float first=-1,second=-1;
            for(int cls=0;cls<classes;++cls){const auto v=p[cls*mask.values.size()+i];if(v>first){second=first;first=v;best=cls;}else second=std::max(second,v);}
            if(first>=.7f&&first-second>=.15f){mask.values[i]=static_cast<std::uint16_t>(best);++counts[best];}else ++counts[classes];}
        fs::create_directories(output/name);write_diagnostic_png(output/name/"image.png",rgb);output_mask(output/name,rgb,mask);
        rows.push_back({{"folder",name},{"source_frame",s.at("source_frame")},{"source_png_sha256",s.at("source_sha256")},{"roi",s.at("roi")},
            {"counts_classes_then_unknown",counts},{"label_status","proposed"},{"human_gold",false},{"confidence_calibrated",false}});
        html<<"<section><h2>"<<name<<"</h2><img src='"<<name<<"/image.png'><img src='"<<name<<"/proposed-overlay.png'><p>原生 ROI 與模型提議；逐物件／line role仍unknown。</p></section>";
    }
    html<<"<p>0 background / 1 line appearance / 2 tap / 3 Hold head / 4 body / 5 tail / 6 Drag / 7 Flick / 8 effect；低信心或近似並列仍ignore。</p>";html.close();
    const json report={{"rows",rows},{"checkpoint_sha256",meta.at("checkpoint_sha256")},{"source_manifest_sha256",sha256_file(packet/"manifest.json")},
        {"roi_forward_softmax_ms",distribution(costs)},{"roi_geometry","native 256x256; no resize"},{"nonfinite_failures",failures},
        {"training_source",meta.at("training_source")},{"real_accuracy",nullptr},{"human_gold",0},{"runtime_feedback",false},
        {"real_input_backend_constructed",false},{"not_full_frame_latency",true},{"warmup_per_roi",3}};
    save(output/"predictions.json",report);return report;
}

json cpu_vision_self_test(const fs::path& output) {
    initialize();torch::manual_seed(1337);PartNet net;std::mt19937 rng(91);const auto e=synthetic(rng);const auto x=input(e.rgb),y=labels(e.semantic);
    const auto before=net->parameters().front().clone();torch::optim::SGD opt(net->parameters(),torch::optim::SGDOptions(.01));
    opt.zero_grad();auto value=loss(net->forward(x),y);value.backward();bool finite_gradients=true;
    for(const auto& p:net->parameters())finite_gradients=finite_gradients&&torch::isfinite(p.grad()).all().item<bool>();opt.step();
    const bool changed=!torch::equal(before,net->parameters().front());net->eval();torch::NoGradGuard no_grad;const auto expected=net->forward(x).clone();
    new_folder(output);torch::save(net,(output/"model.pt").string());PartNet loaded;torch::load(loaded,(output/"model.pt").string());loaded->eval();
    const auto actual=loaded->forward(x);auto truth_ignore=y.clone();truth_ignore.fill_(255);truth_ignore.index_put_({0,0,0},0);
    auto logits_a=expected.clone(),logits_b=expected.clone();logits_b.index_put_({0,torch::indexing::Slice(),1,1},100);
    bool oversized_rejected=false;try {net->forward(torch::zeros({1,3,257,64}));}catch(const std::invalid_argument&){oversized_rejected=true;}
    return {{"device_cpu",expected.device().type()==torch::kCPU},{"optimizer_changed_weights",changed},{"finite_gradient_and_loss",finite_gradients&&torch::isfinite(value).item<bool>()},
        {"checkpoint_max_abs_difference",(expected-actual).abs().max().item<double>()},{"ignore_pixel_loss_difference",std::abs(loss(logits_a,truth_ignore).item<double>()-loss(logits_b,truth_ignore).item<double>())},
        {"oversized_input_rejected",oversized_rejected},{"parameters",parameters(net)},{"torch_version",TORCH_VERSION}};
}
} // namespace pas
