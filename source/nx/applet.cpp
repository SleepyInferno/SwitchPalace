#include "nx/applet.hpp"

#ifdef __SWITCH__
#include <switch.h>
#endif

namespace switchpalace::nx {

bool isAppletMode()
{
#ifdef __SWITCH__
    AppletType type = appletGetAppletType();
    return (type != AppletType_Application &&
            type != AppletType_SystemApplication);
#else
    // On host builds, default to application mode
    return false;
#endif
}

const char* getAppletModeString()
{
    return isAppletMode() ? "APPLET" : "APPLICATION";
}

} // namespace switchpalace::nx
