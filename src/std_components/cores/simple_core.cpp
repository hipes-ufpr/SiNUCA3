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

int SimpleCore::SetConfigParameter(const char* parameter, ConfigValue value) {
    Component<MemoryPacket>** ptrToParameter;
    int* connectionIDPtr;
    if (strcmp(parameter, "instructionMemory") == 0) {
        ptrToParameter = &this->instructionMemory;
        connectionIDPtr = &this->instructionConnectionID;
    } else if (strcmp(parameter, "dataMemory") == 0) {
        ptrToParameter = &this->dataMemory;
        connectionIDPtr = &this->dataConnectionID;
    } else if (strcmp(parameter, "fetching") == 0) {
        if (value.type != ConfigValueTypeComponentReference) {
            SINUCA3_ERROR_PRINTF(
                "Component SimpleCore received a parameter that's not a "
                "component "
                "reference.\n");
            return 1;
        }

        this->fetching = dynamic_cast<Component<FetchPacket>*>(
            value.value.componentReference);
        if (this->fetching == NULL) {
            SINUCA3_ERROR_PRINTF(
                "Component SimpleCore received a parameter fetching that's not"
                "a reference to a Component<MemoryPacket>.\n");
            return 1;
        }
        this->fetchingConnectionID = this->fetching->Connect(0);
        return 0;
    } else {
        SINUCA3_ERROR_PRINTF(
            "Component SimpleCore received unknown parameter %s.\n", parameter);
        return 1;
    }

    if (value.type != ConfigValueTypeComponentReference) {
        SINUCA3_ERROR_PRINTF(
            "Component SimpleCore received a parameter that's not a component "
            "reference.\n");
        return 1;
    }

    *ptrToParameter =
        dynamic_cast<Component<MemoryPacket>*>(value.value.componentReference);
    if (*ptrToParameter == NULL) {
        SINUCA3_ERROR_PRINTF(
            "Component SimpleCore received a parameter %s that's not a "
            "reference "
            "to a Component<MemoryPacket>.\n",
            parameter);
        return 1;
    }

    *connectionIDPtr = (*ptrToParameter)->Connect(0);

    return 0;
}

int SimpleCore::FinishSetup() {
    if (this->fetching == NULL) {
        SINUCA3_ERROR_PRINTF(
            "SimpleCore didn't received required parameter fetching.\n");
        return 1;
    }

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
            MemoryPacket fetchPacket = fetch.response.staticInfo->opcodeAddress;
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
