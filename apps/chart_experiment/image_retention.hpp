#pragma once
#include <stdexcept>
#include <string_view>

namespace rhythm::experimental {
enum class ImageRetention { none, result, diagnostic };
inline constexpr ImageRetention default_image_retention=ImageRetention::none;
inline ImageRetention parse_image_retention(std::string_view value) {
    if(value=="none")return ImageRetention::none;
    if(value=="result")return ImageRetention::result;
    if(value=="diagnostic")return ImageRetention::diagnostic;
    throw std::invalid_argument("image retention must be none, result or diagnostic");
}
inline const char* image_retention_name(ImageRetention policy) noexcept {
    switch(policy) {
    case ImageRetention::none:return "none";
    case ImageRetention::result:return "result";
    case ImageRetention::diagnostic:return "diagnostic";
    }
    return "none";
}
inline bool retain_diagnostic_images(ImageRetention policy) noexcept {return policy==ImageRetention::diagnostic;}
inline bool retain_result_image(ImageRetention policy) noexcept {return policy!=ImageRetention::none;}
inline ImageRetention image_retention_suffix(int& argc,const char* const* argv) {
    if(argc>=2&&std::string_view(argv[argc-1])=="--image-retention")
        throw std::invalid_argument("image retention value missing");
    if(argc>=3&&std::string_view(argv[argc-2])=="--image-retention") {
        const auto policy=parse_image_retention(argv[argc-1]);argc-=2;return policy;
    }
    return default_image_retention;
}
} // namespace rhythm::experimental
