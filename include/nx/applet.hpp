#pragma once

namespace switchpalace::nx {

/// Detect whether the application is running in applet mode.
/// Returns true when the applet type is NOT Application or SystemApplication.
bool isAppletMode();

/// Returns a human-readable string for the current mode: "APPLET" or "APPLICATION"
const char* getAppletModeString();

} // namespace switchpalace::nx
