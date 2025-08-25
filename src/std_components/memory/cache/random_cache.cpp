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
 * @file random_cache.cpp
 * @brief Implementation of a cache using Random as replacement policy.
 */

#include "random_cache.hpp"

#include <cassert>
#include <cstdlib>
#include <ctime>
#include <utils/cache.hpp>
#include <utils/logging.hpp>

RandomCache::RandomCache() : isSeedSet(false), numberOfRequests(0) {}

RandomCache::~RandomCache() {}

bool RandomCache::Read(unsigned long addr, CacheEntry **result) {
    return this->cache.GetEntry(addr, result);
}

void RandomCache::Write(unsigned long addr, unsigned long value) {
    unsigned long tag = this->cache.GetTag(addr);
    unsigned long set = this->cache.GetIndex(addr);

    CacheEntry *entry;
    if (this->cache.FindEmptyEntry(addr, &entry)) {
        *entry = CacheEntry(entry, tag, set, value);
        return;
    }

    int random = rand() % this->cache.numWays;
    entry = &this->cache.entries[set][random];
    *entry = CacheEntry(entry, tag, set, value);
    return;
}

void RandomCache::Clock() {
    SINUCA3_DEBUG_PRINTF("%p: RandomCache Clock!\n", this);
    long numberOfConnections = this->GetNumberOfConnections();
    MemoryPacket packet;
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
                // there is no penalty, so we still need to decide what happens
                // in this case.
                //
                // Then, call Write() to insert a new addr in cache
                // according to the replacement policy.
            }
        }
    }
}

void RandomCache::Flush() {}

void RandomCache::PrintStatistics() {}

int RandomCache::FinishSetup() {
    if (this->cache.FinishSetup()) return 1;

    if (this->isSeedSet)
        srand(this->seed);
    else
        srand(time(NULL));

    return 0;
}

int RandomCache::SetConfigParameter(const char *parameter, ConfigValue value) {
    if (this->cache.SetConfigParameter(parameter, value)) return 1;

    bool isSeed = (strcmp(parameter, "seed") == 0);
    if (!isSeed) {
        SINUCA3_ERROR_PRINTF("Random Cache received an unkown parameter: %s.\n",
                             parameter);
        return 1;
    }

    if (isSeed && value.type != ConfigValueTypeInteger) {
        SINUCA3_ERROR_PRINTF(
            "Random Cache parameter \"seed\" is not an integer.\n");
        return 1;
    }

    const long v = value.value.integer;
    this->seed = v;
    this->isSeedSet = true;

    return 0;
}
