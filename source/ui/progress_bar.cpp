#include "ui/progress_bar.hpp"

namespace switchpalace::ui {

ProgressBar::ProgressBar()
{
    setHeight(8.0f);
    setGrow(1.0f);
}

void ProgressBar::setProgress(float percent)
{
    if (percent < 0.0f) percent = 0.0f;
    if (percent > 100.0f) percent = 100.0f;
    m_progress = percent;
}

float ProgressBar::getProgress() const
{
    return m_progress;
}

void ProgressBar::draw(NVGcontext* vg, float x, float y, float width, float height,
                       brls::Style style, brls::FrameContext* ctx)
{
    // Draw unfilled track: #3A3A3A, 8px height, 4px corner radius
    float barHeight = 8.0f;
    float barY = y + (height - barHeight) / 2.0f;
    float radius = 4.0f;

    nvgBeginPath(vg);
    nvgRoundedRect(vg, x, barY, width, barHeight, radius);
    nvgFillColor(vg, nvgRGB(0x3A, 0x3A, 0x3A));
    nvgFill(vg);

    // Draw filled portion: #3D9CF0 (Royal Sapphire)
    float filledWidth = (m_progress / 100.0f) * width;
    if (filledWidth > 0.0f)
    {
        nvgBeginPath(vg);
        nvgRoundedRect(vg, x, barY, filledWidth, barHeight, radius);
        nvgFillColor(vg, nvgRGB(0x3D, 0x9C, 0xF0));
        nvgFill(vg);
    }
}

} // namespace switchpalace::ui
