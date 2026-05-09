
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
#include "utils/logger.hpp"

int LSUDebugComponent::Configure(Config config) {
    if (config.ComponentReference("fetch", &this->fetch, true))
        return config.Error("fetch", "missing required component reference");
    if (config.ComponentReference("lsu", &this->lsu, true))
        return config.Error("lsu", "missing required component reference");

    this->lsuConnId = this->lsu->Connect(0);
    this->fetchConnectionId = this->fetch->Connect(0);

    return 0;
}

void LSUDebugComponent::PrintStatistics() {
    SINUCA3_LOG_PRINTF("=== LSU DEBUG COMPONENT STATISTICS ===\n");
    SINUCA3_LOG_PRINTF("Total loads: %d\n", this->loadsCounter);
    SINUCA3_LOG_PRINTF("Total stores: %d\n", this->storesCounter);
}

void LSUDebugComponent::Clock() {
    InstructionDecode instDecode;
    FetchPacket instRequest;
    FetchPacket instResponse;
    LSUPacket lsuRequest;

    static InstructionDecode oldestInst;
    static bool hasOldestInst = false;
    static bool requestSentToFetch = false;

    if (!this->fetch->ReceiveResponse(this->fetchConnectionId, &instResponse)) {
        instDecode.remainingCycles = this->fixedInstLatency;
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
                instDecode.address = lsuRequest.operation.vtAddr;
                this->instCommitQueue.Enqueue(&instDecode);
                ++this->loadsCounter;
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
                instDecode.address = lsuRequest.operation.vtAddr;
                this->instCommitQueue.Enqueue(&instDecode);
                ++this->storesCounter;
            }
        } else {
            /* Enqueue instruction */
            instDecode.type = InstructionTypeOther;
            instDecode.address = 0;
            this->instCommitQueue.Enqueue(&instDecode);
        }
        requestSentToFetch = false;
    }

    if (!requestSentToFetch) {
        instRequest.request = 0;
        this->fetch->SendRequest(this->fetchConnectionId, &instRequest);
        requestSentToFetch = true;
    }

    DebugPacketLSU pkt;
    while (this->ReceiveRequestFromConnection(0, &pkt) == 0) {
        this->resolvedRequests.push_back(pair::Pair<unsigned long, long>(pkt.address, pkt.seqNum));
    }

    DebugPacketLSU responsePkt;
    if (!hasOldestInst && this->instCommitQueue.Dequeue(&oldestInst)) {
        return; /* No instructions to commit. */
    }

    hasOldestInst = true;

    if (oldestInst.remainingCycles > 0) {
        oldestInst.remainingCycles -= 1;
    } else {
        hasOldestInst = false;
        if (oldestInst.type == InstructionTypeStore) {
            /* Check if the store address has been resolved. */
            for (unsigned long i = 0; i < this->resolvedRequests.size(); i++) {
                if (this->resolvedRequests[i].first == oldestInst.address) {
                    responsePkt.address = oldestInst.address;
                    responsePkt.seqNum = this->resolvedRequests[i].second;
                    this->SendResponseToConnection(0, &responsePkt);
                    this->resolvedRequests[i] = this->resolvedRequests.back();
                    this->resolvedRequests.pop_back();
                    return;
                }
            }
            hasOldestInst = true;
        }
    }
}
