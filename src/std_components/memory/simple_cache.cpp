//
// Copyright (C) 2024  HiPES - Universidade Federal do Paraná
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <https://www.gnu.org/licenses/>.
//

/**
 * @file simple_cache.cpp
 * @brief Implementation of the SimpleCache.
 */

#include "simple_cache.hpp"

#include <sinuca3.hpp>
#include "utils/logging.hpp"

int SimpleCache::FinishSetup() {
    if (this->cache.FinishSetup()) return 1;

    return 0;
}

int SimpleCache::SetConfigParameter(const char* parameter, ConfigValue value) {
    if (this->cache.SetConfigParameter(parameter, value)) return 1;

    return 0;
}

void SimpleCache::Clock() {
    SINUCA3_DEBUG_PRINTF("%p: SimpleCache Clock!\n", this);
    long numberOfConnections = this->GetNumberOfConnections();
    MemoryPacket packet;
    for (long i = 0; i < numberOfConnections; ++i) {
        if (this->ReceiveRequestFromConnection(i, &packet) == 0) {
            ++this->numberOfRequests;

            // We dont have (and dont need) data to send back, so a
            // MemoryPacket is send back to to signal
            // that the cache's operation has been completed.

            SINUCA3_DEBUG_PRINTF("%p: SimpleCache Message (%lu) Received!\n",
                                 this, packet);

            // Read() returns true if it was hit.
            if (this->cache.Read(packet)) {
                SINUCA3_DEBUG_PRINTF("%p: SimpleCache HIT!\n", this);
            } else {
                SINUCA3_DEBUG_PRINTF("%p: SimpleCache MISS!\n", this);
                this->cache.Write(packet);
            }


            this->SendResponseToConnection(i, &packet);
        }
    }
}

void SimpleCache::PrintStatistics() {
            SINUCA3_DEBUG_PRINTF("%p: SimpleCache Stats:\n\tMiss: %lu\n\tHit: %lu\n\tAcces: %lu\n\tEvaction: %lu\n\tValidProp: %.3f\n",
                this, this->cache.getStatMiss(), this->cache.getStatHit(), this->cache.getStatAcess(), this->cache.getStatEvaction(), this->cache.getStatValidProp());
}
