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

#include <cstring>

#include "../sinuca3.hpp"
#include "../utils/logging.hpp"

int SimpleCore::SetConfigParameter(const char* parameter,
                                   sinuca::config::ConfigValue value) {
    sinuca::Component<sinuca::MemoryPacket>** ptrToParameter;
    int* connectionIDPtr;
    if (strcmp(parameter, "instructionMemory") == 0) {
        ptrToParameter = &this->instructionMemory;
        connectionIDPtr = &this->instructionConnectionID;
    } else if (strcmp(parameter, "dataMemory") == 0) {
        ptrToParameter = &this->dataMemory;
        connectionIDPtr = &this->dataConnectionID;
    } else {
        SINUCA3_ERROR_PRINTF(
            "Component SimpleCore received unknown parameter %s.\n", parameter);
        return 1;
    }

    if (value.type != sinuca::config::ConfigValueTypeComponentReference) {
        SINUCA3_ERROR_PRINTF(
            "Component SimpleCore received a parameter that's not a component "
            "reference.\n");
        return 1;
    }

    *ptrToParameter = dynamic_cast<sinuca::Component<sinuca::MemoryPacket>*>(
        value.value.componentReference);
    if (*ptrToParameter == NULL) {
        SINUCA3_ERROR_PRINTF(
            "Component SimpleCore received a parameter that's not a reference "
            "to a Component<MemoryPacket>.\n");
        return 1;
    }

    *connectionIDPtr = (*ptrToParameter)->Connect(0);

    return 0;
}

int SimpleCore::FinishSetup() { return 0; }

void SimpleCore::Clock() {
    long numberOfConnections = this->GetNumberOfConnections();
    sinuca::InstructionPacket instruction;

    for (long i = 0; i < numberOfConnections; ++i) {
        if (this->ReceiveRequestFromConnection(i, &instruction)) {
            ++this->numFetchedInstructions;

            sinuca::MemoryPacket fetchPacket =
                instruction.staticInfo->opcodeAddress;
            this->instructionMemory->SendRequest(this->instructionConnectionID,
                                                 &fetchPacket);
            for (long i = 0; i < instruction.dynamicInfo.numReadings; ++i) {
                this->dataMemory->SendRequest(
                    this->dataConnectionID,
                    (unsigned long*)&(instruction.dynamicInfo.readsAddr[i]));
            }

            for (long i = 0; i < instruction.dynamicInfo.numWritings; ++i) {
                this->dataMemory->SendRequest(
                    this->dataConnectionID,
                    (unsigned long*)&(instruction.dynamicInfo.writesAddr[i]));
            }
        }
    }
}

void SimpleCore::Flush() {}

void SimpleCore::PrintStatistics() {
    SINUCA3_LOG_PRINTF("SimpleCore %p: %lu instructions fetched\n", this,
                       this->numFetchedInstructions);
}

SimpleCore::~SimpleCore() {}
