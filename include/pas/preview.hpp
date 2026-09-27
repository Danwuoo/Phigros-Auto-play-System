#pragma once

#include "pas/core.hpp"
#include <memory>

namespace pas {

class PreviewWindow final {
public:
    PreviewWindow(int width, int height, bool benchmark_placement = false);
    ~PreviewWindow();
    PreviewWindow(const PreviewWindow&) = delete;
    PreviewWindow& operator=(const PreviewWindow&) = delete;
    bool pump();
    void draw(const Frame& frame);
    void title(const std::string& text);
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace pas
