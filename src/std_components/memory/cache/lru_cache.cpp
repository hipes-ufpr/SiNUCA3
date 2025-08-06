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

LRUCache::LRUCache() {
    this->WayUsageCounters = new unsigned int*[this->numSets];
    int n = this->numSets * this->numWays;
    this->WayUsageCounters[0] = new unsigned int[n];
    memset(this->WayUsageCounters[0], 0, n * sizeof(unsigned int));
    for (int i = 1; i < this->numSets; ++i) {
        this->WayUsageCounters[i] = this->WayUsageCounters[0] +
                                    (i * this->numWays * sizeof(unsigned int));
    }
}

LRUCache::~LRUCache() {
    delete[] this->WayUsageCounters[0];
    delete[] this->WayUsageCounters;
}

bool LRUCache::Read(unsigned long addr, CacheEntry** result) {
    bool exist = GetEntry(addr, result);
    this->WayUsageCounters[(*result)->i][(*result)->j] += 1;
    return exist;
}

void LRUCache::Write(unsigned long addr, unsigned long value) {
    CacheEntry* entry;
    CacheEntry* lru_entry = NULL;
    unsigned long oldest_time = ~0UL;  // valor máximo
    unsigned long tag = this->GetTag(addr);
    unsigned long index = this->GetIndex(addr);
    int i;
    int j;

    for_each_way(entry, index) {
        if (!entry->isValid) {
            lru_entry = entry;
            break;
        }
        i = entry->i;
        j = entry->j;
        if (this->WayUsageCounters[i][j] < oldest_time) {
            oldest_time = this->WayUsageCounters[i][j];
            lru_entry = entry;
        }
    }

    CacheEntry newEntry = {tag, index, true, i, j, value};
    *lru_entry = newEntry;
}
