#include "nx/ticket.hpp"

#ifdef __SWITCH__
#include <switch.h>
#endif

namespace switchpalace::nx {

bool TicketManager::importTicket(const uint8_t* tikData, size_t tikSize,
                                 const uint8_t* certData, size_t certSize) {
    if (!tikData || tikSize == 0 || !certData || certSize == 0) {
        return false;
    }

#ifdef __SWITCH__
    Result rc = esInitialize();
    if (R_FAILED(rc)) {
        return false;
    }

    rc = esImportTicket(tikData, tikSize, certData, certSize);
    esExit();

    if (R_FAILED(rc)) {
        return false;
    }
#else
    (void)tikData;
    (void)tikSize;
    (void)certData;
    (void)certSize;
#endif

    return true;
}

} // namespace switchpalace::nx
