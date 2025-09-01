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
#include <cstddef>
#include "utils/cache/replacement_policies/lru.hpp"
#include "utils/cache/replacement_policies/random.hpp"
#include "utils/cache/replacement_policies/roundRobin.hpp"
unsigned long offsetBitsMask = 12;
unsigned long indexBitsMask = 6;
unsigned long tagBitsMask = 46;

#include <cassert>
#include <utils/logging.hpp>


#include "cache.hpp"
#include "replacement_policy.hpp"

Cache::~Cache() {
    if (this->entries) {
        delete[] this->entries[0];
        delete[] this->entries;
    }
    delete this->policy;
}

bool Cache::Read(MemoryPacket addr){
    bool exist;
    CacheEntry *result;

    exist = this->GetEntry(addr, &result);
    this->policy->Acess(result);

    return exist;
}

void Cache::Write(MemoryPacket addr){
    CacheEntry *victim;
    unsigned long tag = GetTag(addr);
    unsigned long index = GetIndex(addr);

    if(FindEmptyEntry(addr, &victim)){
        *victim = CacheEntry(victim, tag, index);
        this->policy->Acess(victim);
        return;
    }

    int set, way;
    this->policy->SelectVictim(tag, index, &set, &way);
    victim = &this->entries[set][way];

    *victim = CacheEntry(victim, tag, index);
    this->policy->Acess(victim);
    return;
}

unsigned long Cache::GetIndex(unsigned long addr) const {
    return (addr & indexBitsMask) >> offsetBitsMask;
}

unsigned long Cache::GetTag(unsigned long addr) const {
    return (addr & tagBitsMask) >> (offsetBitsMask + indexBitsMask);
}

bool Cache::GetEntry(unsigned long addr, CacheEntry **result) const {
    unsigned long tag = this->GetTag(addr);
    unsigned long index = this->GetIndex(addr);
    for (int way = 0; way < this->numWays; ++way) {
        CacheEntry *entry = &this->entries[index][way];

        if (entry->isValid && entry->tag == tag) {
            *result = entry;
            return true;
        }
    }
    return false;
}

bool Cache::FindEmptyEntry(unsigned long addr, CacheEntry **result) const {
    unsigned long index = this->GetIndex(addr);
    for (int way = 0; way < this->numWays; ++way) {
        CacheEntry *entry = &this->entries[index][way];

        if (!entry->isValid) {
            *result = entry;
            return true;
        }
    }
    return false;
}

int Cache::FinishSetup() {
    if (this->numSets == 0) {
        SINUCA3_ERROR_PRINTF(
            "Cache didn't received obrigatory parameter \"sets\"\n");
        return 1;
    }

    if (this->numWays == 0) {
        SINUCA3_ERROR_PRINTF(
            "Cache didn't received obrigatory parameter \"ways\"\n");
        return 1;
    }

    size_t n = this->numSets * this->numWays;
    this->entries = new CacheEntry *[this->numSets];
    this->entries[0] = new CacheEntry[n];
    memset(this->entries[0], 0, n * sizeof(CacheEntry));
    for (int i = 1; i < this->numSets; ++i) {
        this->entries[i] = this->entries[0] + (i * this->numWays);
    }

    for (int i = 0; i < this->numSets; ++i) {
        for (int j = 0; j < this->numWays; ++j) {
            this->entries[i][j].i = i;
            this->entries[i][j].j = j;
        }
    }

    return 0;
}

int Cache::SetConfigParameter(const char *parameter, ConfigValue value) {
    bool isSets = (strcmp(parameter, "sets") == 0);
    bool isWays = (strcmp(parameter, "ways") == 0);
    bool isPolicy = (strcmp(parameter, "policy") == 0);

    if (!isSets && !isWays && !isPolicy) {
        SINUCA3_ERROR_PRINTF("Cache received an unkown parameter: %s.\n",
                             parameter);
        return 1;
    }

    if(isSets && value.type != ConfigValueTypeInteger){
        SINUCA3_ERROR_PRINTF("Cache parameter \"sets\" is not an integer.\n");
        return 1;
    }

    if(isWays && value.type != ConfigValueTypeInteger){
        SINUCA3_ERROR_PRINTF("Cache parameter \"ways\" is not an integer.\n");
        return 1;
    }

    if(isPolicy && value.type != ConfigValueTypeInteger){
        SINUCA3_ERROR_PRINTF("Cache parameter \"policy\" is not an integer.\n");
        return 1;
    }

    if (isSets) {
        const long v = value.value.integer;
        if (v <= 0) {
            SINUCA3_ERROR_PRINTF(
                "Invalid value for Cache parameter \"sets\": should be > 0.");
            return 1;
        }
        this->numSets = v;
    }

    if (isWays) {
        const long v = value.value.integer;
        if (v <= 0) {
            SINUCA3_ERROR_PRINTF(
                "Invalid value for Cache parameter \"ways\": should be > 0.");
            return 1;
        }
        this->numWays = v;
    }

    if(isPolicy){
        const ReplacementPoliciesID v = static_cast<ReplacementPoliciesID>(value.value.integer);
        if(SetReplacementPolicy(v)){
            SINUCA3_ERROR_PRINTF(
                "Invalid value for Cache parameter \"policy\": should be a value from enum ReplacementPoliciesID.");
            return 1;
        }
    }

    return 0;
}

bool Cache::SetReplacementPolicy(ReplacementPoliciesID id){
    switch (id) {
        case LruID:
        this->policy = new ReplacementPolicies::LRU(this->numSets, this->numWays);
        return 0;

        case RandomID:
        this->policy = new ReplacementPolicies::Random(this->numSets, this->numWays);
        return 0;

        case RoundRobinID:
        this->policy = new ReplacementPolicies::RoundRobin(this->numSets, this->numWays);
        return 0;

        default:
        return 1;
    }
}
