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
 * @file simple_memory.cpp
 * @brief Implementation of the SimpleMemory.
 */

#include "simple_memory.hpp"

#include <sinuca3.hpp>

int SimpleMemory::Configure(Config config) {
    (void)config;
    return 0;
}

void SimpleMemory::Clock() {
    long numberOfConnections = this->GetNumberOfConnections();
    MemoryPacket packet;
    for (long i = 0; i < numberOfConnections; ++i) {
        if (this->ReceiveRequestFromConnection(i, &packet) == 0) {
            ++this->numberOfRequests;
            this->SendResponseToConnection(i, &packet);
        }
    }
}

void SimpleMemory::PrintStatistics() {
    SINUCA3_LOG_PRINTF("SimpleMemory %p: %lu requests made\n", this,
                       this->numberOfRequests);
}

SimpleMemory::~SimpleMemory() {}
