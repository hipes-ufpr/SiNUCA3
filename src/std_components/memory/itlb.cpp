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
 * @file itlb.cpp
 * @brief Implementation of a instruction TLB.
 */

#include "itlb.hpp"

#include <sinuca3.hpp>

#include "config/config.hpp"
#include "utils/cache/cacheMemory.hpp"
#include "utils/logging.hpp"

int iTLB::Configure(Config config) {
    const char* tempStr_Policy = "lru";  // lru is default

    if (config.Integer("entries", (long*)&this->entries, true)) return 1;
    if (config.Integer("associativity", (long*)&this->numWays, true)) return 1;
    if (config.Integer("missPenalty", (long*)&this->missPenalty, true))
        return 1;

    // Optional
    if (config.String("policy", &tempStr_Policy)) return 1;
    if (config.Integer("pageSize", (long*)&this->pageSize)) return 1;

    if (this->entries == 0) {
        SINUCA3_ERROR_PRINTF(
            "Invalid value for iTLB parameter \"entries\": should be > "
            "0.");
        return 1;
    }

    if (this->numWays == 0) {
        SINUCA3_ERROR_PRINTF(
            "Invalid value for iTLB parameter \"associativity\": should "
            "be > 0.");
        return 1;
    }

    unsigned int numSets = this->entries / this->numWays;
    this->cache = CacheMemory<unsigned long>::fromNumSets(
        numSets, this->pageSize, this->numWays, tempStr_Policy);
    if (this->cache == NULL) {
        SINUCA3_ERROR_PRINTF("iTLB: Failed to alocate CacheMemory\n");
        return 1;
    }

    this->pendingRequests.Allocate(0, sizeof(struct TLBRequest));

    return 0;
}

void iTLB::Clock() {
    SINUCA3_DEBUG_PRINTF("%p: iTLB Clock!\n", this);

    long numberOfConnections = this->GetNumberOfConnections();
    for (int id = 0; id < numberOfConnections; ++id) {
        struct TLBRequest newRequest;
        if (this->ReceiveRequestFromConnection(id, &newRequest.addr) == 0) {
            ++this->numberOfRequests;
            newRequest.id = id;
            this->pendingRequests.Enqueue(&newRequest);
            SINUCA3_DEBUG_PRINTF("%p: iTLB Message (%p) Received!\n", this,
                                 (void*)newRequest.addr);
        }
    }

    /* If paying a miss penalty */
    if (this->currentPenalty > 0) {
        --this->currentPenalty;

        if (this->currentPenalty == 0) {
            this->currentPenalty = NO_PENALTY;
            SINUCA3_DEBUG_PRINTF("%p: iTLB Waiting ended! Sending response\n",
                                 this);
            this->SendResponseToConnection(this->curRequest.id,
                                           &this->curRequest.addr);
        }

        return;
    }

    struct TLBRequest request;
    if (this->pendingRequests.Dequeue(&request)) {
        SINUCA3_DEBUG_PRINTF("%p: iTLB No work avaliable: Stall.\n", this);
        return;
    }
    this->curRequest = request;

    // We dont have (and dont need) data to send back, so
    // the same address is send back to to signal
    // that the iTLB's operation has been completed.

    // Read() returns NULL if it was a miss.
    if (this->cache->Read(this->curRequest.addr)) {
        SINUCA3_DEBUG_PRINTF("%p: iTLB (%p) HIT Sending response!\n", this,
                             (void*)this->curRequest.addr);
        this->SendResponseToConnection(this->curRequest.id,
                                       &this->curRequest.addr);
    } else {
        SINUCA3_DEBUG_PRINTF("%p: iTLB (%p) MISS Waiting %lu cycles!\n", this,
                             (void*)this->curRequest.addr, this->missPenalty);
        this->currentPenalty = this->missPenalty;
        this->cache->Write(this->curRequest.addr, &this->curRequest.addr);
    }

    return;
}

void iTLB::PrintStatistics() {
    SINUCA3_DEBUG_PRINTF(
        "%p: iTLB Stats:\n\tMiss: %lu\n\tHit: %lu\n\tAcces: "
        "%lu\n\tEvaction: %lu\n\tValidProp: %.3f\n",
        this, this->cache->getStatMiss(), this->cache->getStatHit(),
        this->cache->getStatAcess(), this->cache->getStatEvaction(),
        this->cache->getStatValidProp());
}
