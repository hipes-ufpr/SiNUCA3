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

#include "roundRobin.hpp"

#include <cassert>
#include <cstring>
#include <utils/cache/cacheMemory.hpp>
#include <utils/logging.hpp>

namespace ReplacementPolicies {

RoundRobin::RoundRobin(int numSets, int numWays)
    : ReplacementPolicy(numSets, numWays) {
    this->rrIndex = new int[this->numSets];
    memset(this->rrIndex, 0, sizeof(int) * this->numSets);
};

RoundRobin::~RoundRobin() { delete this->rrIndex; }

void RoundRobin::Acess(CacheLine* entry) { (void)entry; }

void RoundRobin::SelectVictim(unsigned long tag, unsigned long index,
                              int* resultSet, int* resultWay) {
    (void)tag;
    int rr = this->rrIndex[index];
    this->rrIndex[index] = (this->rrIndex[index] + 1) % this->numWays;
    *resultSet = index;
    *resultWay = rr;
}

}  // namespace ReplacementPolicies
