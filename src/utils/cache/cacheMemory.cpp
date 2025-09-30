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

#include "cacheMemory.hpp"

#include <cassert>
#include <cmath>
#include <cstddef>
#include <utils/logging.hpp>

#include "replacement_policy.hpp"
#include "utils/cache/replacement_policies/lru.hpp"
#include "utils/cache/replacement_policies/random.hpp"
#include "utils/cache/replacement_policies/roundRobin.hpp"

static double getLog2(double x) { return log(x) / log(2); }

static bool checkIfPowerOfTwo(unsigned long x) {
    if (x == 0) return false;  // 0 is not a power of 2
    // x is power of 2 if only one bit is set.
    return (x & (x - 1)) == 0;
}

CacheMemory::~CacheMemory() {
    if (this->entries) {
        delete[] this->entries[0];
        delete[] this->entries;
    }
    delete this->policy;
}

bool CacheMemory::Read(MemoryPacket addr) {
    bool exist;
    CacheLine *result;

    exist = this->GetEntry(addr, &result);
    if (exist){
        this->policy->Acess(result);
    }

    this->statAcess += 1;
    if(exist)
        this->statHit += 1;
    else
        this->statMiss += 1;

    return exist;
}

void CacheMemory::Write(MemoryPacket addr) {
    CacheLine *victim = NULL;
    unsigned long tag = GetTag(addr);
    unsigned long index = GetIndex(addr);

    if(!FindEmptyEntry(addr, &victim)){
        int set, way;
        this->policy->SelectVictim(tag, index, &set, &way);
        victim = &this->entries[set][way];
    }

    *victim = CacheLine(victim, tag, index);
    this->policy->Acess(victim);
    this->statEvaction += 1;
    return;
}

unsigned long CacheMemory::GetOffset(unsigned long addr) const {
    return addr & this->offsetMask;
}

unsigned long CacheMemory::GetIndex(unsigned long addr) const {
    return (addr & this->indexMask) >> this->offsetBits;
}

unsigned long CacheMemory::GetTag(unsigned long addr) const {
    return (addr & this->tagMask) >> (this->offsetBits + this->indexBits);
}

bool CacheMemory::GetEntry(unsigned long addr, CacheLine **result) const {
    unsigned long tag = this->GetTag(addr);
    unsigned long index = this->GetIndex(addr);
    for (int way = 0; way < this->numWays; ++way) {
        CacheLine *entry = &this->entries[index][way];

        if (entry->isValid && entry->tag == tag) {
            *result = entry;
            return true;
        }
    }
    return false;
}

bool CacheMemory::FindEmptyEntry(unsigned long addr, CacheLine **result) const {
    unsigned long index = this->GetIndex(addr);
    for (int way = 0; way < this->numWays; ++way) {
        CacheLine *entry = &this->entries[index][way];

        if (!entry->isValid) {
            *result = entry;
            return true;
        }
    }
    return false;
}

void CacheMemory::setAddrSizeBits(unsigned int addrSizeBits) {
    this->addrSizeBits = addrSizeBits;
}

int CacheMemory::FinishSetup() {
    if (this->cacheSize == 0) {
        SINUCA3_ERROR_PRINTF(
            "Cache didn't received obrigatory parameter \"cacheSize\"\n");
        return 1;
    }

    if (this->lineSize == 0) {
        SINUCA3_ERROR_PRINTF(
            "Cache didn't received obrigatory parameter \"lineSize\"\n");
        return 1;
    }

    if (this->numWays == -1) {
        SINUCA3_ERROR_PRINTF(
            "Cache didn't received obrigatory parameter \"associativity\"\n");
        return 1;
    }

    this->numSets = this->cacheSize / (this->lineSize * this->numWays);

    if (!checkIfPowerOfTwo(this->numSets) ||
        !checkIfPowerOfTwo(this->lineSize)) {
        SINUCA3_ERROR_PRINTF(
            "Trying to get a log2 of a non power of two number\n");
        return 1;
    }

    this->offsetBits = getLog2(this->lineSize);
    this->indexBits = getLog2(this->numSets);
    this->tagBits = this->addrSizeBits - (this->indexBits + this->offsetBits);

    if (this->tagBits < 0) {
        SINUCA3_ERROR_PRINTF(
            "Calculated tagBits is negative. Check addrSizeBits, lineSize and "
            "numSets.\n");
        return 1;
    }

    if (this->numWays != 0 && this->tagBits == 0) {
        SINUCA3_ERROR_PRINTF(
            "No bits left to be used as cache tag. Increase addrSizeBits or "
            "reduce lineSize/numSets.\n");
        return 1;
    }

    this->offsetMask = (1UL << offsetBits) - 1;
    this->indexMask = ((1UL << indexBits) - 1) << offsetBits;
    this->tagMask = ((1UL << tagBits) - 1) << (offsetBits + indexBits);

    size_t n = this->numSets * this->numWays;
    this->entries = new CacheLine *[this->numSets];
    this->entries[0] = new CacheLine[n];
    memset(this->entries[0], 0, n * sizeof(CacheLine));
    for (int i = 1; i < this->numSets; ++i) {
        this->entries[i] = this->entries[0] + (i * this->numWays);
    }

    for (int i = 0; i < this->numSets; ++i) {
        for (int j = 0; j < this->numWays; ++j) {
            this->entries[i][j].i = i;
            this->entries[i][j].j = j;
        }
    }

    if (this->policyID == Unset) {
        SINUCA3_ERROR_PRINTF(
            "Cache didn't received obrigatory parameter \"policy\"\n");
        return 1;
    }

    this->SetReplacementPolicy(this->policyID);

    return 0;
}

int CacheMemory::SetConfigParameter(const char *parameter, ConfigValue value) {
    bool isCacheSize = (strcmp(parameter, "cacheSize") == 0);
    bool isLineSize = (strcmp(parameter, "lineSize") == 0);
    bool isAssociativity = (strcmp(parameter, "associativity") == 0);

    bool isPolicy = (strcmp(parameter, "policy") == 0);

    if (!isCacheSize && !isLineSize && !isAssociativity && !isPolicy) {
        SINUCA3_ERROR_PRINTF("Cache received an unkown parameter: %s.\n",
                             parameter);
        return 1;
    }

    if (isCacheSize && value.type != ConfigValueTypeInteger) {
        SINUCA3_ERROR_PRINTF(
            "Cache parameter \"cacheSize\" is not an integer.\n");
        return 1;
    }

    if (isLineSize && value.type != ConfigValueTypeInteger) {
        SINUCA3_ERROR_PRINTF(
            "Cache parameter \"lineSize\" is not an integer.\n");
        return 1;
    }

    if (isAssociativity && value.type != ConfigValueTypeInteger) {
        SINUCA3_ERROR_PRINTF(
            "Cache parameter \"associativity\" is not an integer.\n");
        return 1;
    }

    if (isPolicy && value.type != ConfigValueTypeInteger) {
        SINUCA3_ERROR_PRINTF("Cache parameter \"policy\" is not an integer.\n");
        return 1;
    }

    if (isCacheSize) {
        const long v = value.value.integer;
        if (v <= 0) {
            SINUCA3_ERROR_PRINTF(
                "Invalid value for Cache parameter \"cacheSize\": should be > "
                "0.");
            return 1;
        }
        this->cacheSize = v;
    }

    if (isLineSize) {
        const long v = value.value.integer;
        if (v <= 0) {
            SINUCA3_ERROR_PRINTF(
                "Invalid value for Cache parameter \"lineSize\": should be > "
                "0.");
            return 1;
        }
        this->lineSize = v;
    }

    if (isAssociativity) {
        const long v = value.value.integer;
        if (v <= 0) {
            SINUCA3_ERROR_PRINTF(
                "Invalid value for Cache parameter \"associativity\": should "
                "be > 0.");
            return 1;
        }
        this->numWays = v;
    }

    if (isPolicy) {
        const ReplacementPoliciesID v =
            static_cast<ReplacementPoliciesID>(value.value.integer);
        this->policyID = v;
    }

    return 0;
}

bool CacheMemory::SetReplacementPolicy(ReplacementPoliciesID id) {
    switch (id) {
        case LruID:
            this->policy =
                new ReplacementPolicies::LRU(this->numSets, this->numWays);
            return 0;

        case RandomID:
            this->policy =
                new ReplacementPolicies::Random(this->numSets, this->numWays);
            return 0;

        case RoundRobinID:
            this->policy = new ReplacementPolicies::RoundRobin(this->numSets,
                                                               this->numWays);
            return 0;

        default:
            return 1;
    }
}

void CacheMemory::resetStatistics(){
    this->statMiss = 0;
    this->statHit = 0;
    this->statAcess = 0;
    this->statEvaction = 0;
}

unsigned long CacheMemory::getStatMiss() const{
    return this->statMiss;
}

unsigned long CacheMemory::getStatHit() const{
    return this->statHit;
}

unsigned long CacheMemory::getStatAcess() const{
    return this->statAcess;
}

unsigned long CacheMemory::getStatEvaction() const{
    return this->statEvaction;
}

float CacheMemory::getStatValidProp() const{
    int n = this->numSets * this->numWays;
    int count = 0;
    for(int i=0; i<n; ++i){
        if(this->entries[0][i].isValid)
            count += 1;
    }
    return (float)count/(float)n;
}
