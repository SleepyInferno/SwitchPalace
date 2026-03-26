#include "nx/ticket.hpp"

#ifdef __SWITCH__
#include <switch.h>

// libnx 4.x removed the public ES service API.
// Implement ticket import via direct IPC (command 1 = ImportTicket).
namespace {
    static Result esOpenService(Service* out) {
        return smGetService(out, "es");
    }

    static Result esImportTicketIpc(Service* srv,
                                    const void* tikBuf, size_t tikSize,
                                    const void* certBuf, size_t certSize) {
        return serviceDispatch(srv, 1,
            .buffer_attrs = {
                SfBufferAttr_HipcMapAlias | SfBufferAttr_In,
                SfBufferAttr_HipcMapAlias | SfBufferAttr_In,
            },
            .buffers = {
                { tikBuf, tikSize },
                { certBuf, certSize },
            },
        );
    }
} // anonymous namespace
#endif

namespace switchpalace::nx {

bool TicketManager::importTicket(const uint8_t* tikData, size_t tikSize,
                                 const uint8_t* certData, size_t certSize) {
    if (!tikData || tikSize == 0 || !certData || certSize == 0) {
        return false;
    }

#ifdef __SWITCH__
    Service esSrv = {};
    Result rc = esOpenService(&esSrv);
    if (R_FAILED(rc)) {
        return false;
    }

    rc = esImportTicketIpc(&esSrv, tikData, tikSize, certData, certSize);
    serviceClose(&esSrv);

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
