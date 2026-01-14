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
 * @file simple_core.cpp
 * @details Public API of the SimpleCore, a testing core that executes
 * everything in a single clock cycle.
 */

#include "simple_core.hpp"

#include <cassert>
#include <cstring>
#include <sinuca3.hpp>

int SimpleCore::Configure(Config config) {
    if (config.ComponentReference("instructionMemory",
                                  &this->instructionMemory))
        return 1;
    if (config.ComponentReference("dataMemory", &this->dataMemory)) return 1;
    if (config.ComponentReference("fetching", &this->fetching, true)) return 1;

    if (this->instructionMemory != NULL)
        this->instructionConnectionID = this->instructionMemory->Connect(0);
    if (this->dataMemory != NULL)
        this->dataConnectionID = this->dataMemory->Connect(0);
    this->fetchingConnectionID = this->fetching->Connect(0);

    return 0;
}

void SimpleCore::Clock() {
    FetchPacket fetch;
    fetch.request = 0;
    this->fetching->SendRequest(this->fetchingConnectionID, &fetch);
    if (this->fetching->ReceiveResponse(this->fetchingConnectionID, &fetch) ==
        0) {
        ++this->numFetchedInstructions;
        if (this->instructionMemory != NULL) {
            MemoryPacket fetchPacket = fetch.response.staticInfo->instAddress;
            this->instructionMemory->SendRequest(this->instructionConnectionID,
                                                 &fetchPacket);
            if (this->dataMemory != NULL) {
                for (long i = 0; i < fetch.response.dynamicInfo.numReadings;
                     ++i) {
                    this->dataMemory->SendRequest(
                        this->dataConnectionID,
                        (unsigned long*)&(
                            fetch.response.dynamicInfo.readsAddr[i]));
                }

                for (long i = 0; i < fetch.response.dynamicInfo.numWritings;
                     ++i) {
                    this->dataMemory->SendRequest(
                        this->dataConnectionID,
                        (unsigned long*)&(
                            fetch.response.dynamicInfo.writesAddr[i]));
                }
            }
        }
    }
}

void SimpleCore::PrintStatistics() {
    SINUCA3_LOG_PRINTF("SimpleCore %p: %lu instructions fetched\n", this,
                       this->numFetchedInstructions);
}

SimpleCore::~SimpleCore() {}
