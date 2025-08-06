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
 * @file cache_nway.cpp
 * @brief Implementation of a abstract nway cache.
 */

 // TODO
 // We need to know how many bits are going to be used as offset... in other
 // words... how large is one page in memory. Idealy, we import this information
 // from elsewhere. But I will leave as global constants for now.
unsigned long offsetBitsMask = 12;
unsigned long indexBitsMask = 6;
unsigned long tagBitsMask = 46;

#include "cache.hpp"

#include <cassert>

#include "../../../utils/logging.hpp"

unsigned long Cache::GetIndex(unsigned long addr) const {
    return (addr & indexBitsMask) >> offsetBitsMask;
}

unsigned long Cache::GetTag(unsigned long addr) const {
    return (addr & tagBitsMask) >> (offsetBitsMask + indexBitsMask);
}

bool Cache::GetEntry(unsigned long addr, CacheEntry **result) const {
    unsigned long tag = this->GetTag(addr);
    unsigned long index = this->GetIndex(addr);
    CacheEntry *entry;
    for_each_way(entry, index) {
        if (entry->isValid && entry->tag == tag) {
            *result = entry;
            return true;
        }
    }
    return false;
}

Cache::~Cache() {
    if (this->entries) {
        delete[] this->entries[0];
        delete[] this->entries;
    }
}

int Cache::FinishSetup() {
    if (this->numSets == 0) {
        SINUCA3_ERROR_PRINTF(
            "TLB didn't received obrigatory parameter \"sets\"\n");
        return 1;
    }

    if (this->numWays == 0) {
        SINUCA3_ERROR_PRINTF(
            "TLB didn't received obrigatory parameter \"ways\"\n");
        return 1;
    }

    this->entries = new CacheEntry *[this->numSets];
    int n = this->numSets * this->numWays;
    assert(n > 0 && "Sanity check failed: TLB buffer size must overflowed.\n");
    this->entries[0] = new CacheEntry[n];
    memset(this->entries[0], 0, n * sizeof(CacheEntry));
    for (int i = 1; i < this->numSets; ++i) {
        this->entries[i] =
            this->entries[0] + (i * this->numWays * sizeof(CacheEntry));
    }

    for (int i = 0; i < this->numSets; ++i) {
        for (int j = 0; j < this->numWays; ++j) {
            this->entries[i][j].i = i;
            this->entries[i][j].j = j;
        }
    }

    return 0;
}

int Cache::SetConfigParameter(const char *parameter,
                              sinuca::config::ConfigValue value) {
    bool isSets = (strcmp(parameter, "sets") == 0);
    bool isWays = (strcmp(parameter, "ways") == 0);

    if (!isSets && !isWays) {
        SINUCA3_ERROR_PRINTF("TLB received an unkown parameter: %s.\n",
                             parameter);
        return 1;
    }

    if ((isSets || isWays) &&
        value.type != sinuca::config::ConfigValueTypeInteger) {
        SINUCA3_ERROR_PRINTF("TLB parameter \"%s\" is not an integer.\n",
                             isSets ? "sets" : "ways");
        return 1;
    }

    if (isSets) {
        const long v = value.value.integer;
        if (v <= 0) {
            SINUCA3_ERROR_PRINTF(
                "Invalid value for TLB parameter \"sets\": should be > 0.");
            return 1;
        }
        this->numSets = v;
    }

    if (isWays) {
        const long v = value.value.integer;
        if (v <= 0) {
            SINUCA3_ERROR_PRINTF(
                "Invalid value for TLB parameter \"ways\": should be > 0.");
            return 1;
        }
        this->numWays = v;
    }

    return 0;
}

void Cache::Clock() {
    SINUCA3_DEBUG_PRINTF("%p: CacheNWay Clock!\n", this);
    long numberOfConnections = this->GetNumberOfConnections();
    sinuca::MemoryPacket packet;
    for (long i = 0; i < numberOfConnections; ++i) {
        if (this->ReceiveRequestFromConnection(i, &packet) == 0) {
            ++this->numberOfRequests;

            CacheEntry *result;

            // We dont have (and dont need) data to send back, so a
            // MemoryPacket is send back to to signal
            // that the cache's operation has been completed.

            // Read() returns true if it was hit.
            if (this->Read(packet, &result)) {
                this->SendResponseToConnection(i, &packet);
            } else {
                // TODO
                // Call the page-table walker.
                // This is a memory access, if the memory is perfect,
                // there is no penalty, so we still need to decide what happens in this case.
                //
                // Then, call Write() to insert a new addr in cache
                // according to the replacement policy.
            }
        }
    }
}

void Cache::Flush() {}

void Cache::PrintStatistics() {}
