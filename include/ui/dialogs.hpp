#pragma once

#include <functional>
#include <string>

namespace switchpalace::ui {

// Show cancel confirmation dialog
// onConfirmCancel called if user confirms cancellation
void showCancelDialog(std::function<void()> onConfirmCancel);

// Show conflict error dialog
// titleName: the conflicting title
void showConflictDialog(const std::string& titleName);

// Show insufficient space dialog
void showInsufficientSpaceDialog(const std::string& destination, const std::string& amount);

} // namespace switchpalace::ui
