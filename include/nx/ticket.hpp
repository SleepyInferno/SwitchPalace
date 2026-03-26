#pragma once

#include <cstdint>
#include <cstddef>

namespace switchpalace::nx {

class TicketManager {
public:
    /// Import ticket + certificate from container data
    bool importTicket(const uint8_t* tikData, size_t tikSize,
                      const uint8_t* certData, size_t certSize);
};

} // namespace switchpalace::nx
