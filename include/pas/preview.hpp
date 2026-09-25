#pragma once

#include "pas/core.hpp"
#include <memory>

namespace pas {

class PreviewWindow final {
public:
    PreviewWindow(int width, int height);
    ~PreviewWindow();
    PreviewWindow(const PreviewWindow&) = delete;
    PreviewWindow& operator=(const PreviewWindow&) = delete;
    bool pump();
    void draw(const Frame& frame);
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace pas
