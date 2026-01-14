#ifndef NDEBUG

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
 * @file engine_debug_component.cpp
 * @brief Implementation of the EngineDebugComponent. THIS FILE SHALL ONLY BE
 * CALLED BY CODE PATHS THAT ONLY COMPILE IN DEBUG MODE.
 */

#include "engine_debug_component.hpp"

#include <sinuca3.hpp>

int EngineDebugComponent::Configure(Config config) {
    bool failNow = false;
    if (config.Bool("failNow", &failNow)) return 1;
    if (failNow) return config.Error("failNow", "it's true");
    if (config.ComponentReference("pointerOther", &this->other)) return 1;
    if (config.ComponentReference("fetch", &this->fetch)) return 1;

    if (this->other != NULL) this->otherConnectionID = this->other->Connect(0);
    if (this->fetch != NULL) this->fetchConnectionID = this->fetch->Connect(0);

    return 0;
}

void EngineDebugComponent::Clock() {
    SINUCA3_DEBUG_PRINTF("%p: Clock!\n", this);

    if (this->fetch != NULL) {
        FetchPacket packet;
        packet.request = 0;
        SINUCA3_DEBUG_PRINTF("%p: Fetching instruction!\n", this);
        this->fetch->SendRequest(this->fetchConnectionID, &packet);
        if (this->fetch->ReceiveResponse(this->fetchConnectionID, &packet) ==
            0) {
            SINUCA3_DEBUG_PRINTF("%p: Received instruction %s\n", this,
                                 packet.response.staticInfo->instMnemonic);
        }
    }

    InstructionPacket messageInput = {NULL, DynamicInstructionInfo(), 0};
    InstructionPacket messageOutput = {NULL, DynamicInstructionInfo(), 0};

    if (this->other) {
        if (!(this->send)) {
            messageInput.staticInfo = (const StaticInstructionInfo*)0xcafebabe;
            SINUCA3_DEBUG_PRINTF("%p: Sending message (%p) to %p.\n", this,
                                 messageInput.staticInfo, this->other);
            this->other->SendRequest(this->otherConnectionID, &messageInput);
            this->send = true;
        } else {
            if (this->other->ReceiveResponse(this->otherConnectionID,
                                             &messageOutput) == 0) {
                SINUCA3_DEBUG_PRINTF("%p: Received response (%p) from %p.\n",
                                     this, messageOutput.staticInfo,
                                     this->other);
                this->send = false;
            } else {
                SINUCA3_DEBUG_PRINTF("%p: No response from %p.\n", this,
                                     this->other);
            }
        }
    } else {
        for (long i = 0; i < this->GetNumberOfConnections(); ++i) {
            if (this->ReceiveRequestFromConnection(i, &messageOutput) == 0) {
                SINUCA3_DEBUG_PRINTF("%p: Received message (%p)\n", this,
                                     messageOutput.staticInfo);
                messageInput.staticInfo = messageOutput.staticInfo + 1;
                SINUCA3_DEBUG_PRINTF("%p: Sending response (%p)\n", this,
                                     messageInput.staticInfo);
                this->SendResponseToConnection(i, &messageInput);
            }
        }
    }
}

void EngineDebugComponent::PrintStatistics() {
    SINUCA3_LOG_PRINTF("EngineDebugComponent %p: printing statistics\n", this);
}

EngineDebugComponent::~EngineDebugComponent() {}

#endif  // NDEBUG
