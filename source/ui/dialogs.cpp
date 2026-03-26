#include "ui/dialogs.hpp"
#include <borealis.hpp>

namespace switchpalace::ui {

void showCancelDialog(std::function<void()> onConfirmCancel)
{
    auto* dialog = new brls::Dialog("The current install will be stopped and all progress rolled back. No partial files will remain on your system.");

    dialog->addButton("Cancel Install", [onConfirmCancel, dialog]() {
        dialog->close([onConfirmCancel]() {
            if (onConfirmCancel)
                onConfirmCancel();
        });
    });

    dialog->addButton("Continue Installing", [dialog]() {
        dialog->close();
    });

    dialog->setCancelable(true);
    dialog->open();
}

void showConflictDialog(const std::string& titleName)
{
    std::string body = titleName + " is already installed at this destination. Delete the existing version first, then retry.";
    auto* dialog = new brls::Dialog(body);

    dialog->addButton("Dismiss", [dialog]() {
        dialog->close();
    });

    dialog->setCancelable(true);
    dialog->open();
}

void showInsufficientSpaceDialog(const std::string& destination, const std::string& amount)
{
    std::string body = "Not enough space on " + destination + ". Free up " + amount + " and try again.";
    auto* dialog = new brls::Dialog(body);

    dialog->addButton("Dismiss", [dialog]() {
        dialog->close();
    });

    dialog->setCancelable(true);
    dialog->open();
}

} // namespace switchpalace::ui
