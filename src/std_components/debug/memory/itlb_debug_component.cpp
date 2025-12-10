#include "config/config.hpp"
#include "std_components/memory/itlb.hpp"
#include "utils/logging.hpp"
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
 * @file itlb_debug_component.cpp
 * @brief Implementation of a component to test the itlb. THIS FILE SHALL ONLY
 * BE INCLUDED BY CODE PATHS THAT ONLY COMPILE IN DEBUG MODE.
 */

#include <cstdio>
#include <sinuca3.hpp>

#include "itlb_debug_component.hpp"

int iTLBDebugComponent::Configure(Config config) {
    if (config.ComponentReference("fetch", &this->fetch, true)) return 1;
    if (config.ComponentReference("itlb", &this->itlb, true)) return 1;

    this->fetchConnectionID = this->fetch->Connect(0);
    this->itlbID = this->itlb->Connect(0);
    SINUCA3_DEBUG_PRINTF("%p: connected to itlb: %d\n", this, this->itlbID);

    return 0;
}

void iTLBDebugComponent::F0() {
    FetchPacket packet;

    if (!this->fetch->ReceiveResponse(this->fetchConnectionID, &packet)) {
        this->fetchBuffer.Enqueue(&packet);
        this->waitingFor -= 1;
        SINUCA3_DEBUG_PRINTF("%p: F0: Fetched instruction %s\n", this,
                             packet.response.staticInfo->opcodeAssembly);
    }

    if (this->waitingFor < this->fetchBuffer.GetSize() &&
        !this->fetchBuffer.IsFull()) {
        packet.request = 0;
        this->fetch->SendRequest(this->fetchConnectionID, &packet);
        this->waitingFor += 1;
        SINUCA3_DEBUG_PRINTF("%p: F0: Sending new fetcher request. (%d/%d)\n",
                             this, this->fetchBuffer.GetOccupation(),
                             this->fetchBuffer.GetSize());
    } else {
        SINUCA3_DEBUG_PRINTF(
            "%p: F0: NOT sending new fetcher request. (%d/%d)\n", this,
            this->fetchBuffer.GetOccupation(), this->fetchBuffer.GetSize());
    }
}

void iTLBDebugComponent::F1() {
    FetchPacket packet;
    Address fakePhysicalAddress;

    if (this->itlb->ReceiveResponse(this->itlbID, &fakePhysicalAddress)) {
        SINUCA3_DEBUG_PRINTF("%p: F1: Waiting response from iTLB.\n", this);
    } else {
        this->tlbRequestBuffer.Dequeue(&fakePhysicalAddress);
        SINUCA3_DEBUG_PRINTF("%p: F1: Response from itlb received!\n", this);
    }

    if (!this->tlbRequestBuffer.IsFull() &&
        !this->fetchBuffer.Dequeue(&packet)) {
        Address virtualAddress =
            static_cast<Address>(packet.response.staticInfo->opcodeAddress);
        this->itlb->SendRequest(this->itlbID, &virtualAddress);
        this->tlbRequestBuffer.Enqueue(&virtualAddress);
        SINUCA3_DEBUG_PRINTF("%p: F1: Sending request %p to itlb.\n", this,
                             (void*)virtualAddress);
    } else {
        if (this->tlbRequestBuffer.IsFull()) {
            SINUCA3_DEBUG_PRINTF(
                "%p: F1: NOT sending request to itlb. Waiting response.\n",
                this);
        } else {
            SINUCA3_DEBUG_PRINTF(
                "%p: F1: NOT sending request to itlb. Cant dequeue new "
                "instruction.\n",
                this);
        }
    }
}

void iTLBDebugComponent::Clock() {
    SINUCA3_DEBUG_PRINTF("%p: iTLBDebugComponent Clock\n", this);

    this->F0();
    this->F1();

    return;
}

void iTLBDebugComponent::PrintStatistics() {
    SINUCA3_LOG_PRINTF("EngineDebugComponent %p: printing statistics\n", this);
}

iTLBDebugComponent::~iTLBDebugComponent() {}

#endif  // NDEBUG
