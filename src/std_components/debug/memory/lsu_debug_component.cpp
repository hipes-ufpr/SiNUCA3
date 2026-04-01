
//
// Copyright (C) 2026  HiPES - Universidade Federal do Paraná
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
 * @file lsu_debug_component.cpp
 * @brief Implementation of a component to test the lsu. THIS FILE SHALL ONLY
 * BE INCLUDED BY CODE PATHS THAT ONLY COMPILE IN DEBUG MODE.
 */

#include "lsu_debug_component.hpp"

#include "engine/default_packets.hpp"
#include "std_components/memory/lsu.hpp"

int LSUDebugComponent::Configure(Config config) {
    if (config.ComponentReference("fetch", &this->fetch, true))
        return config.Error("fetch", "missing required component reference");
    if (config.ComponentReference("lsu", &this->lsu, true))
        return config.Error("lsu", "missing required component reference");

    this->lsuConnId = this->lsu->Connect(0);
    this->fetchConnectionId = this->fetch->Connect(0);

    return 0;
}

void LSUDebugComponent::Clock() {
    InstructionDecode instDecode;
    FetchPacket instRequest;
    FetchPacket instResponse;
    LSUPacket lsuRequest;

    static InstructionDecode oldestInst;
    static bool hasOldestInst = false;

    while (this->fetch->ReceiveResponse(this->fetchConnectionId,
                                        &instResponse) == 0) {
        if (instResponse.response.staticInfo->instReadsMemory ||
            instResponse.response.staticInfo->instWritesMemory) {
            for (int i = 0; i < instResponse.response.dynamicInfo.numReadings;
                 i++) {
                lsuRequest.type = LSUPacketTypeLoadRequest;
                lsuRequest.operation.vtAddr =
                    instResponse.response.dynamicInfo.readsAddr[i];
                lsuRequest.operation.size =
                    instResponse.response.dynamicInfo.readsSize[i];
                this->lsu->SendRequest(this->lsuConnId, &lsuRequest);
                /* Enqueue instruction */
                instDecode.type = InstructionTypeLoad;
                instDecode.remainingCycles = this->fixedInstLatency;
                this->instCommitQueue.Enqueue(&instDecode);
            }
            for (int i = 0; i < instResponse.response.dynamicInfo.numWritings;
                 i++) {
                lsuRequest.type = LSUPacketTypeStoreRequest;
                lsuRequest.operation.vtAddr =
                    instResponse.response.dynamicInfo.writesAddr[i];
                lsuRequest.operation.size =
                    instResponse.response.dynamicInfo.writesSize[i];
                this->lsu->SendRequest(this->lsuConnId, &lsuRequest);
                /* Enqueue instruction */
                instDecode.type = InstructionTypeStore;
                instDecode.remainingCycles = this->fixedInstLatency;
                this->instCommitQueue.Enqueue(&instDecode);
            }
        } else {
            /* Enqueue instruction */
            instDecode.type = InstructionTypeOther;
            instDecode.remainingCycles = this->fixedInstLatency;
            this->instCommitQueue.Enqueue(&instDecode);
        }
    }

    instRequest.request = 0;
    this->fetch->SendRequest(this->fetchConnectionId, &instRequest);

    MemoryPacket memPacket;
    while (this->ReceiveRequestFromConnection(0, &memPacket) == 0) {
        this->resolvedStoreAddresses.push_back(memPacket);
    }

    if (!hasOldestInst && this->instCommitQueue.Dequeue(&oldestInst)) {
        return; /* No instructions to commit. */
    }

    hasOldestInst = true;
    if (oldestInst.remainingCycles > 0) {
        oldestInst.remainingCycles -= 1;
    } else {
        if (oldestInst.type == InstructionTypeStore) {
            /* Check if the store address has been resolved. */
            for (int i = 0; i < (int)this->resolvedStoreAddresses.size(); i++) {
                if (this->resolvedStoreAddresses[i] == memPacket) {
                    this->instCommitQueue.Dequeue(&oldestInst);
                    this->resolvedStoreAddresses[i] =
                        this->resolvedStoreAddresses.back();
                    this->resolvedStoreAddresses.pop_back();
                    hasOldestInst = false;
                    break;
                }
            }
        } else {
            this->instCommitQueue.Dequeue(&oldestInst);
            hasOldestInst = false;
        }
    }
}
