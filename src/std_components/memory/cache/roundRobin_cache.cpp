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
 * @file roundRobin_cache.cpp
 * @brief Implementation of a cache using RoundRobin as replacement policy.
 */

#include "roundRobin_cache.hpp"

#include <cassert>
#include <cstring>
#include <utils/cache.hpp>
#include <utils/logging.hpp>

RoundRobinCache::RoundRobinCache() : numberOfRequests(0) {}

RoundRobinCache::~RoundRobinCache() {}

bool RoundRobinCache::Read(unsigned long addr, CacheEntry **result) {
    return this->cache.GetEntry(addr, result);
}

void RoundRobinCache::Write(unsigned long addr, unsigned long value) {
    unsigned long tag = this->cache.GetTag(addr);
    unsigned long set = this->cache.GetIndex(addr);

    CacheEntry *entry;
    if (this->cache.FindEmptyEntry(addr, &entry)) {
        *entry = CacheEntry(entry, tag, set, value);
        return;
    }

    int rr = this->rrIndex[set];
    this->rrIndex[set] = (this->rrIndex[set] + 1) % this->cache.numWays;
    entry = &this->cache.entries[set][rr];
    *entry = CacheEntry(entry, tag, set, value);
    return;
}

void RoundRobinCache::Clock() {
    SINUCA3_DEBUG_PRINTF("%p: RoundRobinCache Clock!\n", this);
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

void RoundRobinCache::Flush() {}

void RoundRobinCache::PrintStatistics() {}

int RoundRobinCache::FinishSetup() {
    if (this->cache.FinishSetup()) return 1;

    this->rrIndex = new int[this->cache.numSets];
    memset(this->rrIndex, 0, sizeof(int) * this->cache.numSets);

    return 0;
}

int RoundRobinCache::SetConfigParameter(const char *parameter,
                                        ConfigValue value) {
    return this->cache.SetConfigParameter(parameter, value);
}
