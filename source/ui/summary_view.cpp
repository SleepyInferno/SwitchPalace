#include "ui/summary_view.hpp"

#include <string>

namespace switchpalace::ui {

SummaryView::SummaryView(const std::vector<install::InstallResult>& results)
{
    setAxis(brls::Axis::COLUMN);
    setWidth(brls::View::AUTO);
    setHeight(brls::View::AUTO);
    setGrow(1.0f);
    setBackgroundColor(nvgRGB(0x1A, 0x1A, 0x1A));
    setPadding(32.0f, 32.0f, 32.0f, 32.0f);

    // Count passed/failed
    int passed = 0;
    int total = static_cast<int>(results.size());
    for (const auto& r : results)
    {
        if (r.success)
            passed++;
    }

    // Title
    auto* title = new brls::Label();
    if (passed == total)
        title->setText("Installation Complete");
    else
        title->setText("Installation Finished");
    title->setFontSize(28.0f);
    title->setTextColor(nvgRGB(0xFF, 0xFF, 0xFF));
    title->setMargins(0.0f, 0.0f, 24.0f, 0.0f);
    addView(title);

    // Result list in scrolling frame
    auto* scrollFrame = new brls::ScrollingFrame();
    scrollFrame->setGrow(1.0f);

    auto* listBox = new brls::Box(brls::Axis::COLUMN);
    listBox->setWidth(brls::View::AUTO);

    for (const auto& result : results)
    {
        auto* row = new brls::Box(brls::Axis::ROW);
        row->setWidth(brls::View::AUTO);
        row->setPadding(8.0f, 0.0f, 8.0f, 0.0f);
        row->setAlignItems(brls::AlignItems::CENTER);

        auto* icon = new brls::Label();
        icon->setFontSize(20.0f);

        auto* nameLabel = new brls::Label();
        nameLabel->setFontSize(20.0f);
        nameLabel->setTextColor(nvgRGB(0xFF, 0xFF, 0xFF));
        nameLabel->setText(result.filename);
        nameLabel->setMargins(0.0f, 0.0f, 0.0f, 8.0f);

        if (result.success)
        {
            icon->setText("\u2713"); // Checkmark
            icon->setTextColor(nvgRGB(0x30, 0xD1, 0x58));

            row->addView(icon);
            row->addView(nameLabel);

            auto* verifiedLabel = new brls::Label();
            verifiedLabel->setText("Verified");
            verifiedLabel->setFontSize(18.0f);
            verifiedLabel->setTextColor(nvgRGB(0x30, 0xD1, 0x58));
            verifiedLabel->setMargins(0.0f, 0.0f, 0.0f, 8.0f);
            row->addView(verifiedLabel);
        }
        else
        {
            icon->setText("\u2717"); // X mark
            icon->setTextColor(nvgRGB(0xFF, 0x45, 0x3A));

            row->addView(icon);
            row->addView(nameLabel);

            if (!result.errorMsg.empty())
            {
                auto* errLabel = new brls::Label();
                errLabel->setText(result.errorMsg);
                errLabel->setFontSize(18.0f);
                errLabel->setTextColor(nvgRGB(0xFF, 0x45, 0x3A));
                errLabel->setMargins(0.0f, 0.0f, 0.0f, 8.0f);
                row->addView(errLabel);
            }
        }

        listBox->addView(row);
    }

    scrollFrame->addView(listBox);
    addView(scrollFrame);

    // Overall summary line
    auto* summaryLine = new brls::Label();
    summaryLine->setText(std::to_string(passed) + " of " + std::to_string(total) + " installed successfully");
    summaryLine->setFontSize(20.0f);
    summaryLine->setTextColor(nvgRGB(0xFF, 0xFF, 0xFF));
    summaryLine->setMargins(16.0f, 0.0f, 8.0f, 0.0f);
    addView(summaryLine);

    // Dismiss instruction
    auto* dismissLabel = new brls::Label();
    dismissLabel->setText("Press B to return to file browser");
    dismissLabel->setFontSize(18.0f);
    dismissLabel->setTextColor(nvgRGB(0xAD, 0xAD, 0xAD));
    dismissLabel->setHorizontalAlign(brls::HorizontalAlign::CENTER);
    addView(dismissLabel);

    // Register B button to pop back to file browser
    // Pop SummaryView first, then pop InstallProgressView in the callback once
    // the first animation + stack removal has completed (avoids double-free bug
    // from two synchronous pops on a deferred-deletion activity stack).
    registerAction("Back", brls::ControllerButton::BUTTON_B, [](brls::View* view) {
        brls::Application::popActivity(brls::TransitionAnimation::FADE, []() {
            brls::Application::popActivity();
        });
        return true;
    });
}

} // namespace switchpalace::ui
