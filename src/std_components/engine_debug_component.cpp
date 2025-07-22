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

#include "../sinuca3.hpp"
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
        return 0;
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
        this->otherConnectionID = this->other->Connect(4);
        return 0;
    }
    if (strcmp(parameter, "flush") == 0) {
        this->flush = value.value.integer;
        return 0;
    }
    if (strcmp(parameter, "fetch") == 0) {
        this->fetch = dynamic_cast<Component<sinuca::InstructionPacket>*>(
            value.value.componentReference);
        if (this->fetch == NULL) {
            SINUCA3_DEBUG_PRINTF(
                "%p: Failed to cast fetch to Component<InstructionPacket>.\n",
                this);
            return 1;
        }
        this->fetchConnectionID = this->fetch->Connect(0);
    }

    return 0;
}

int EngineDebugComponent::FinishSetup() {
    SINUCA3_DEBUG_PRINTF("%p: Finishing setup with %s.\n", this,
                         this->shallFailOnFinish ? "FAILURE" : "SUCCESS");
    return this->shallFailOnFinish;
}

void EngineDebugComponent::Clock() {
    SINUCA3_DEBUG_PRINTF("%p: Clock!\n", this);

    if (this->fetch != NULL) {
        sinuca::InstructionPacket packet;
        SINUCA3_DEBUG_PRINTF("%p: Fetching instruction!\n", this);
        this->fetch->SendRequest(this->fetchConnectionID, &packet);
        if (this->fetch->ReceiveResponse(this->fetchConnectionID, &packet) ==
            0) {
            SINUCA3_DEBUG_PRINTF("%p: Received instruction %s\n", this,
                                 packet.staticInfo->opcodeAssembly);
        }
    }

    if (this->flush > 0) {
        --this->flush;
        if (this->flush == 0) sinuca::ENGINE->Flush();
    }

    sinuca::InstructionPacket messageInput = {NULL,
                                              sinuca::DynamicInstructionInfo()};
    sinuca::InstructionPacket messageOutput = {
        NULL, sinuca::DynamicInstructionInfo()};

    if (this->other) {
        if (!(this->send)) {
            messageInput.staticInfo =
                (const sinuca::StaticInstructionInfo*)0xcafebabe;
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

void EngineDebugComponent::Flush() {
    SINUCA3_DEBUG_PRINTF("%p: A flush happened!\n", this);
}

void EngineDebugComponent::PrintStatistics() {
    SINUCA3_LOG_PRINTF("EngineDebugComponent %p: printing statistics\n", this);
}

EngineDebugComponent::~EngineDebugComponent() {}

#endif  // NDEBUG
