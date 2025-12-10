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
 * @brief Implementation of LRU as replacement policy.
 */

#include "lru.hpp"

#include <cassert>
#include <cstring>
#include <utils/cache/cacheMemory.hpp>
#include <utils/logging.hpp>

namespace ReplacementPolicies {

LRU::LRU(int numSets, int numWays) : ReplacementPolicy(numSets, numWays) {
    this->WayUsageCounters = new unsigned int*[this->numSets];
    int n = this->numSets * this->numWays;
    this->WayUsageCounters[0] = new unsigned int[n];
    memset(this->WayUsageCounters[0], 0, n * sizeof(unsigned int));
    for (int i = 1; i < this->numSets; i++) {
        this->WayUsageCounters[i] =
            this->WayUsageCounters[0] + (i * this->numWays);
    }
};

LRU::~LRU() {
    delete[] this->WayUsageCounters[0];
    delete[] this->WayUsageCounters;
}

void LRU::Acess(CacheLine* entry) {
    for (int way = 0; way < this->numWays; ++way) {
        this->WayUsageCounters[entry->i][way] += 1;
    }
    this->WayUsageCounters[entry->i][entry->j] = 0;
}

void LRU::SelectVictim(unsigned long tag, unsigned long index, int* resultSet,
                       int* resultWay) {
    (void)tag;
    long lruMaxCount = -1;  // Max value
    *resultSet = index;
    for (int way = 0; way < this->numWays; ++way) {
        if (this->WayUsageCounters[index][way] > lruMaxCount) {
            lruMaxCount = this->WayUsageCounters[index][way];
            *resultWay = way;
        }
    }
}

}  // namespace ReplacementPolicies
