#include <cstddef>
#include <cstring>
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

#include "../utils/logging.hpp"
#include "engine_debug_component.hpp"

void EngineDebugComponent::PrintConfigValue(const char* parameter,
                                            sinuca::config::ConfigValue value,
                                            unsigned char indent) {
    // This is not good code. Please don't use this kind of stuff outside this
    // very specific niche where we want to avoid allocations and have a very
    // specific reason to use SINUCA3_DEBUG_PRINTF to print stuff and etc.
    char indentChars[sizeof(unsigned char) << 8];
    memset(indentChars, ' ', sizeof(unsigned char) << 8);
    indentChars[indent] = '\0';

    switch (value.type) {
        case sinuca::config::ConfigValueTypeArray:
            SINUCA3_DEBUG_PRINTF("%p: %s %s%s%s\n", this, parameter,
                                 indentChars, indentChars, "array: ");
            for (unsigned long i = 0; i < value.value.array->size(); ++i)
                this->PrintConfigValue(parameter, (*value.value.array)[i],
                                       indent + 1);
            return;
        case sinuca::config::ConfigValueTypeBoolean:
            SINUCA3_DEBUG_PRINTF("%p: %s %s%sbool: %s\n", this, parameter,
                                 indentChars, indentChars,
                                 value.value.boolean ? "true" : "false");
            return;
        case sinuca::config::ConfigValueTypeNumber:
            SINUCA3_DEBUG_PRINTF("%p: %s %s%snumber: %f\n", this, parameter,
                                 indentChars, indentChars, value.value.number);
            return;
        case sinuca::config::ConfigValueTypeInteger:
            SINUCA3_DEBUG_PRINTF("%p: %s %s%sinteger: %ld\n", this, parameter,
                                 indentChars, indentChars, value.value.integer);
            return;
        case sinuca::config::ConfigValueTypeComponentReference:
            SINUCA3_DEBUG_PRINTF("%p: %s %s%sreference: %p\n", this, parameter,
                                 indentChars, indentChars,
                                 value.value.componentReference);
            return;
    }
}

int EngineDebugComponent::SetConfigParameter(
    const char* parameter, sinuca::config::ConfigValue value) {
    this->PrintConfigValue(parameter, value);
    if (strcmp(parameter, "failNow") == 0) {
        SINUCA3_DEBUG_PRINTF("%p: SetConfigParameter returning failure.\n",
                             this);
        return 1;
    }
    if (strcmp(parameter, "failOnFinish") == 0) {
        SINUCA3_DEBUG_PRINTF("%p: Will fail on finish.\n", this);
        this->shallFailOnFinish = true;
    }

    if (strcmp(parameter, "pointerOther") == 0) {
        this->other =
            dynamic_cast<EngineDebugComponent*>(value.value.componentReference);
        if (this->other == NULL) {
            SINUCA3_DEBUG_PRINTF(
                "%p: Failed to cast pointerOther to EngineDebugComponent.\n",
                this);
            return 1;
        }
        connectionID = this->other->Connect(4);
    }

    return 0;
}

int EngineDebugComponent::FinishSetup() {
    SINUCA3_DEBUG_PRINTF("%p: Finishing setup with %s.\n", this,
                         this->shallFailOnFinish ? "FAILURE" : "SUCCESS");
    return this->shallFailOnFinish;
}

void EngineDebugComponent::Clock() {
    sinuca::InstructionPacket messageInput = {0, 0, 0};
    sinuca::InstructionPacket messageOutput = {0, 0, 0};
    SINUCA3_DEBUG_PRINTF("%p: Clock!\n", this);
    if (this->other) {
        if (!(this->send)) {
            messageInput.address = 0xcafebabe;
            SINUCA3_DEBUG_PRINTF("%p: Sending message (%lx) to %p.\n", this,
                                 messageInput.address, this->other);
            this->other->SendRequest(this->connectionID, &messageInput);
            this->send = true;
        } else {
            if (this->other->ReceiveResponse(this->connectionID,
                                             &messageOutput)) {
                SINUCA3_DEBUG_PRINTF("%p: Received response (%lx) from %p.\n",
                                     this, messageOutput.address, this->other);
            } else {
                SINUCA3_DEBUG_PRINTF("%p: No response from %p.\n", this,
                                     this->other);
            }
        }
    } else {
        std::vector<sinuca::engine::Connection*> connections =
            this->GetConnections();
        for (unsigned int i = 0; i < connections.size(); ++i) {
            if (this->ReceiveRequestFromConnection(i, &messageOutput)) {
                SINUCA3_DEBUG_PRINTF("%p: Received message (%lx)\n", this,
                                     messageOutput.address);
                messageInput.address = messageOutput.address + 1;
                SINUCA3_DEBUG_PRINTF("%p: Sending response (%lx)\n", this,
                                     messageInput.address);
                this->SendResponseToConnection(i, &messageInput);
            }
        }
    }
}

EngineDebugComponent::~EngineDebugComponent() {}

#endif  // NDEBUG
