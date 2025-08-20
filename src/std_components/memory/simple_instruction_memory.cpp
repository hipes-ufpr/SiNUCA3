//
// Copyright (C) 2025  HiPES - Universidade Federal do Paraná
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
 * @file simple_instruction_memory.cpp
 * @brief Implementation of the SimpleInstructionMemory.
 */

#include "simple_instruction_memory.hpp"

#include <sinuca3.hpp>

int SimpleInstructionMemory::SetConfigParameter(const char* parameter,
                                                ConfigValue value) {
    if (strcmp(parameter, "sendTo") == 0) {
        if (value.type == ConfigValueTypeComponentReference) {
            this->sendTo = dynamic_cast<Component<InstructionPacket>*>(
                value.value.componentReference);
            if (this->sendTo != NULL) return 0;
        }

        SINUCA3_ERROR_PRINTF(
            "SimpleInstructionMemory parameter `sendTo` is not a "
            "Component<InstructionPacket>.\n");
        return 1;
    }

    SINUCA3_ERROR_PRINTF(
        "SimpleInstructionMemory received unknown parameter %s.\n", parameter);
    return 1;
}

int SimpleInstructionMemory::FinishSetup() {
    if (this->sendTo != NULL) {
        this->sendToID = this->sendTo->Connect(0);
    }
    return 0;
}

void SimpleInstructionMemory::Clock() {
    long numberOfConnections = this->GetNumberOfConnections();
    InstructionPacket packet;
    for (long i = 0; i < numberOfConnections; ++i) {
        while (this->ReceiveRequestFromConnection(i, &packet) == 0) {
            ++this->numberOfRequests;
            if (this->sendTo != NULL) {
                this->sendTo->SendRequest(this->sendToID, &packet);
            } else {
                this->SendResponseToConnection(i, &packet);
            }
        }
    }
}

void SimpleInstructionMemory::PrintStatistics() {
    SINUCA3_LOG_PRINTF("SimpleInstructionMemory %p: %lu requests made\n", this,
                       this->numberOfRequests);
}

SimpleInstructionMemory::~SimpleInstructionMemory() {}
