#pragma once

#include <borealis.hpp>

namespace switchpalace::ui {

class ProgressBar : public brls::Box {
public:
    ProgressBar();

    void setProgress(float percent); // 0.0 to 100.0
    float getProgress() const;

    void draw(NVGcontext* vg, float x, float y, float width, float height, brls::Style style, brls::FrameContext* ctx) override;

private:
    float m_progress = 0.0f;
};

} // namespace switchpalace::ui
