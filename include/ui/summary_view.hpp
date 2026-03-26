#pragma once

#include <borealis.hpp>
#include <vector>
#include "install/install.hpp"

namespace switchpalace::ui {

class SummaryView : public brls::Box {
public:
    explicit SummaryView(const std::vector<install::InstallResult>& results);
};

} // namespace switchpalace::ui
