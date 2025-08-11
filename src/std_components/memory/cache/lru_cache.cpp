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
 * @file lru_cache.cpp
 * @brief Implementation of a cache using LRU as replacement policy.
 */

#include "lru_cache.hpp"

#include <cassert>
#include <cstring>

#include "../../../utils/logging.hpp"
#include "cache.hpp"

LRUCache::LRUCache() : numberOfRequests(0) {
    this->WayUsageCounters = new unsigned int*[this->c.numSets];
    int n = this->c.numSets * this->c.numWays;
    this->WayUsageCounters[0] = new unsigned int[n];
    memset(this->WayUsageCounters[0], 0, n * sizeof(unsigned int));
    for (int i = 1; i < this->c.numSets; ++i) {
        this->WayUsageCounters[i] = this->WayUsageCounters[0] +
                                    (i * this->c.numWays * sizeof(unsigned int));
    }
}

LRUCache::~LRUCache() {
    delete[] this->WayUsageCounters[0];
    delete[] this->WayUsageCounters;
}

bool LRUCache::Read(unsigned long addr, CacheEntry** result) {
    bool exist = this->c.GetEntry(addr, result);
    this->WayUsageCounters[(*result)->i][(*result)->j] += 1;
    return exist;
}

void LRUCache::Write(unsigned long addr, unsigned long value) {
    CacheEntry* lruEntry = NULL;
    unsigned long oldestTime = ~0UL;  // Max value
    unsigned long tag = this->c.GetTag(addr);
    unsigned long index = this->c.GetIndex(addr);
    int i;
    int j;

    for (int way = 0; way < this->c.numWays; ++way) {
        CacheEntry* entry = &this->c.entries[index][way];

        if (!entry->isValid) {
            lruEntry = entry;
            break;
        }
        i = entry->i;
        j = entry->j;
        if (this->WayUsageCounters[i][j] < oldestTime) {
            oldestTime = this->WayUsageCounters[i][j];
            lruEntry = entry;
        }
    }

    CacheEntry newEntry = {tag, index, true, i, j, value};
    *lruEntry = newEntry;
}

void LRUCache::Clock() {
    SINUCA3_DEBUG_PRINTF("%p: LRUCache Clock!\n", this);
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
                // there is no penalty, so we still need to decide what happens
                // in this case.
                //
                // Then, call Write() to insert a new addr in cache
                // according to the replacement policy.
            }
        }
    }
}

void LRUCache::Flush(){}

void LRUCache::PrintStatistics(){}

int LRUCache::FinishSetup(){
    return this->c.FinishSetup();
}

int LRUCache::SetConfigParameter(const char *parameter,
                               sinuca::config::ConfigValue value){
                                   return this->c.SetConfigParameter(parameter, value);
                               }
