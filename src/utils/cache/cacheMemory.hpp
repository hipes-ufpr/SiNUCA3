#ifndef SINUCA3_CACHE_MEMORY_HPP_
#define SINUCA3_CACHE_MEMORY_HPP_

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
 * @file cacheMemory.hpp
 * @brief Templated n-way cache memory.
 **/

#include <cmath>
#include <cstddef>
#include <cstring>
#include <sinuca3.hpp>
#include <utils/logging.hpp>

#include "replacement_policy.hpp"
#include "utils/cache/replacement_policies/lru.hpp"
#include "utils/cache/replacement_policies/random.hpp"
#include "utils/cache/replacement_policies/roundRobin.hpp"

/**
 * @brief Number of bits in a address.
 * Default is 64 bits.
 * Used to calculate how many bits are used for tag.
 * If you want to change this, may be better to clone this file
 * and create another CacheMemory class for this.
 */
static const int addrSizeBits = 64;

struct CacheLine {
    unsigned long tag;
    unsigned long index;
    bool isValid;

    // This is the position of this entry in the entries matrix.
    // It can be useful for organizing other matrices.
    int i, j;

    CacheLine() {};

    inline CacheLine(int i, int j, unsigned long tag, unsigned long index)
        : tag(tag), index(index), isValid(false), i(i), j(j) {};

    inline CacheLine(CacheLine* entry, unsigned long tag, unsigned long index)
        : tag(tag), index(index), isValid(true), i(entry->i), j(entry->j) {};
};

/**
 * @breaf CacheMemory is a class that facilitates the creation of components
 * that have memory that behaves like a cache (apart from the coherence
 * protocol).
 *
 * @details
 * The CacheMemory class provides a generic implementation of an N-way
 * set-associative cache.
 *
 * This structure stores data by using part of the address as the set index,
 * allowing fast access to cached entries. Each set can contain multiple ways
 * (lines), as defined by the associativity. In case multiple addresses map to
 * the same set (a collision), a replacement policy is used to decide which
 * entry to evict.
 *
 * It is templated on the type of value stored (ValueType) and supports
 * operations such as:
 *   - Read: Lookup for a entry using a address.
 *   - Write: Store a value in the cache, using a replacement policy if
 * necessary.
 *
 * The class provides multiple static creation methods (fromCacheSize,
 * fromNumSets, fromBits) and any of them can be used. The different
 * constructors exist to make it easier to create a CacheMemory instance
 * depending on the context and the information you have available (total size,
 * number of sets, or number of index/offset bits).
 *
 * @usage
 * Include the header and instantiate with the desired type:
 * @code
 * CacheMemory<int> *myCache;
 * myCache = CacheMemory<int>::fromCacheSize(16384, 64, 4,
 * CacheMemoryNS::LruID); const int* value; if (myCache->Lookup(addr, &value)) {
 *     // cache hit
 * }
 * myCache->Write(addr, &newValue);
 * @endcode
 */
template <typename ValueType>
class CacheMemory {
  public:
    /**
     * @brief Creates a cache based on the total cache size and line size.
     *
     * @param cacheSize Total size of the cache in bytes.
     * @param lineSize Size of a single cache line in bytes. Determines how many
     * bits of the address are used as the offset within a line. Can be 0 if not
     * relevant.
     * @param associativity Number of ways per set (N in N-way set-associative
     * cache). Cannot be 0.
     * @param policy Replacement policy to use when inserting or evicting cache
     * lines.
     *
     * @return Pointer to a newly created CacheMemory instance.
     *
     * @detail
     * Both numSets and lineSize must be powers of two.
     * This ensures that address bits can be cleanly divided
     * into index and offset fields.
     *
     * @note
     * numSets = (cacheSize / (lineSize * associativity))
     */
    static CacheMemory* fromCacheSize(unsigned int cacheSize,
                                      unsigned int lineSize,
                                      unsigned int associativity,
                                      const char* policy);

    /**
     * @brief Creates a cache specifying the number of sets and line size.
     *
     * @param numSets Number of sets in the cache.
     * @param lineSize Size of a single cache line in bytes. Determines how many
     * bits of the address are used as the offset within a line. Can be 0 if not
     * relevant.
     * @param associativity Number of ways per set (N in N-way set-associative
     * cache). Cannot be 0.
     * @param policy Replacement policy to use when inserting or evicting cache
     * lines.
     *
     * @return Pointer to a newly created CacheMemory instance.
     *
     * @detail
     * Both numSets and lineSize must be powers of two.
     * This ensures that address bits can be cleanly divided
     * into index and offset fields.
     */
    static CacheMemory* fromNumSets(unsigned int numSets, unsigned int lineSize,
                                    unsigned int associativity,
                                    const char* policy);

    /**
     * @brief Creates a cache specifying the number of index and offset bits
     * directly.
     *
     * @param numIndexBits Number of bits used for the set index.
     * @param numOffsetBits Number of bits used for the offset within a cache
     * line.  Can be 0 if not relevant.
     * @param associativity Number of ways per set (N in N-way set-associative
     * cache). Cannot be 0.
     * @param policy Replacement policy to use when inserting or evicting cache
     * lines.
     *
     * @return Pointer to a newly created CacheMemory instance.
     *
     * @details
     * The number of sets is determined by the number of bits used for the
     * index. The remaining bits of the address not used for index or offset
     * will serve as the tag.
     */
    static CacheMemory* fromBits(unsigned int numIndexBits,
                                 unsigned int numOffsetBits,
                                 unsigned int associativity,
                                 const char* policy);

    virtual ~CacheMemory();

    /**
     * @brief Looks up a cached value for the specified memory address.
     *
     * This method attempts to locate the cache line corresponding to the
     * provided memory address. If the address is found (cache HIT), it returns
     * a pointer directly to the cached value.
     *
     * The returned pointer refers to a constant value stored internally within
     * the cache. This design avoids mandatory data copies during lookup
     * operations, while ensuring that the caller cannot modify the cached
     * content directly.
     *
     * @tparam ValueType Type of the values stored in the cache.
     * @param addr Memory address to look up in the cache.
     * @return Pointer to the cached value if found (HIT), or `NULL` if not
     * found (MISS).
     */
    const ValueType* Read(unsigned long addr);

    /**
     * @brief Writes a value into the cache for the specified memory address.
     *
     * The data pointed to by @p result is copied into the internal cache
     * storage.
     *
     * If there is no invalid (empty) cache line available in the corresponding
     * set, a replacement policy must be applied to select which existing entry
     * will be evicted.
     *
     * @tparam ValueType Type of the values stored in the cache.
     * @param addr Memory address to write to.
     * @param[in] result Pointer to the value to be written into the cache.
     */
    void Write(unsigned long addr, const ValueType* data);

    unsigned long GetOffset(unsigned long addr) const;
    unsigned long GetIndex(unsigned long addr) const;
    unsigned long GetTag(unsigned long addr) const;

    /**
     * @brief Find the entry for a addr.
     * @param addr Address to look for.
     * @param result Pointer to store search result.
     * @return True if found, false otherwise.
     */
    bool GetEntry(unsigned long addr, CacheLine** result) const;

    /**
     * @brief Can be used to find a entry that is not valid.
     * @param addr Address to look for.
     * @param result Pointer to store result.
     * @return True if victim is found, false otherwise.
     */
    bool FindEmptyEntry(unsigned long addr, CacheLine** result) const;

    void resetStatistics();
    unsigned long getStatMiss() const;
    unsigned long getStatHit() const;
    unsigned long getStatAcess() const;
    unsigned long getStatEvaction() const;
    float getStatValidProp() const;

  protected:
    int numWays;
    int numSets;

    unsigned int offsetBits;
    unsigned int indexBits;
    unsigned int tagBits;
    unsigned long offsetMask;
    unsigned long indexMask;
    unsigned long tagMask;

    CacheLine** entries;  // matrix [sets x ways]

    ValueType** data;  // matrix [sets x ways]

    ReplacementPolicy* policy;

    // Statistics
    unsigned long statMiss;
    unsigned long statHit;
    unsigned long statAcess;
    unsigned long statEvaction;

    inline CacheMemory()
        : statMiss(0), statHit(0), statAcess(0), statEvaction(0) {};

    static CacheMemory* Alocate(unsigned int numIndexBits,
                                unsigned int numOffsetBits,
                                unsigned int associativity, const char* policy);
    int SetReplacementPolicy(const char* policyName);
};

// ===================================================================================
//  Implementation
// ===================================================================================

static inline double getLog2(double x) { return log(x) / log(2); }

static inline bool checkIfPowerOfTwo(unsigned long x) {
    if (x == 0) return false;  // 0 is not a power of 2
    // x is power of 2 if only one bit is set.
    return (x & (x - 1)) == 0;
}

template <typename ValueType>
CacheMemory<ValueType>* CacheMemory<ValueType>::fromCacheSize(
    unsigned int cacheSize, unsigned int lineSize, unsigned int associativity,
    const char* policy) {
    unsigned int numSets = cacheSize / (lineSize * associativity);
    return CacheMemory<ValueType>::fromNumSets(numSets, lineSize, associativity,
                                               policy);
}

template <typename ValueType>
CacheMemory<ValueType>* CacheMemory<ValueType>::fromNumSets(
    unsigned int numSets, unsigned int lineSize, unsigned int associativity,
    const char* policy) {
    if (associativity == 0) return NULL;
    if (!checkIfPowerOfTwo(numSets)) {
        SINUCA3_ERROR_PRINTF(
            "CacheMemory: numSets cannot be %u because it is not power of "
            "two.\n",
            numSets);
    }
    if (!checkIfPowerOfTwo(lineSize)) {
        SINUCA3_ERROR_PRINTF(
            "CacheMemory: lineSize cannot be %u because it is not power of "
            "two.\n",
            lineSize);
        return NULL;
    }

    unsigned int indexBits = getLog2(numSets);
    unsigned int offsetBits = getLog2(lineSize);
    return CacheMemory<ValueType>::Alocate(indexBits, offsetBits, associativity,
                                           policy);
}

template <typename ValueType>
CacheMemory<ValueType>* CacheMemory<ValueType>::fromBits(
    unsigned int numIndexBits, unsigned int numOffsetBits,
    unsigned int associativity, const char* policy) {
    return CacheMemory<ValueType>::Alocate(numIndexBits, numOffsetBits,
                                           associativity, policy);
}

template <typename ValueType>
CacheMemory<ValueType>* CacheMemory<ValueType>::Alocate(
    unsigned int numIndexBits, unsigned int numOffsetBits,
    unsigned int associativity, const char* policy) {
    if (associativity == 0) {
        SINUCA3_ERROR_PRINTF(
            "CacheMemory: associativity cannot be equal to 0.\n");
        return NULL;
    }

    unsigned int numSets = 1u << numIndexBits;

    CacheMemory<ValueType>* cm = new CacheMemory<ValueType>();
    cm->numWays = associativity;
    cm->numSets = numSets;

    cm->offsetBits = numOffsetBits;
    cm->indexBits = numIndexBits;
    cm->tagBits = addrSizeBits - (cm->indexBits + cm->offsetBits);

    cm->offsetMask = (1UL << cm->offsetBits) - 1;
    cm->indexMask = ((1UL << cm->indexBits) - 1) << cm->offsetBits;
    cm->tagMask = ((1UL << cm->tagBits) - 1)
                  << (cm->offsetBits + cm->indexBits);

    if (cm->SetReplacementPolicy(policy)) {
        delete cm;
        SINUCA3_ERROR_PRINTF("CacheMemory: Invalid replacement policy.\n");
        return NULL;
    }

    size_t n = cm->numSets * cm->numWays;
    cm->entries = new CacheLine*[cm->numSets];
    cm->entries[0] = new CacheLine[n];
    memset(cm->entries[0], 0, n * sizeof(CacheLine));
    for (int i = 1; i < cm->numSets; ++i) {
        cm->entries[i] = cm->entries[0] + (i * cm->numWays);
    }

    for (int i = 0; i < cm->numSets; ++i) {
        for (int j = 0; j < cm->numWays; ++j) {
            cm->entries[i][j].i = i;
            cm->entries[i][j].j = j;
        }
    }

    cm->data = new ValueType*[cm->numSets];
    cm->data[0] = new ValueType[n];
    memset(cm->data[0], 0, n * sizeof(ValueType));
    for (int i = 1; i < cm->numSets; ++i) {
        cm->data[i] = cm->data[0] + (i * cm->numWays);
    }

    return cm;
}

template <typename ValueType>
CacheMemory<ValueType>::~CacheMemory() {
    if (this->entries) {
        delete[] this->entries[0];
        delete[] this->entries;
    }

    if (this->data) {
        delete[] this->data[0];
        delete[] this->data;
    }

    delete this->policy;
}

template <typename ValueType>
const ValueType* CacheMemory<ValueType>::Read(unsigned long addr) {
    bool exist;
    CacheLine* line;
    const ValueType* result = NULL;

    exist = this->GetEntry(addr, &line);
    if (exist) {
        this->policy->Acess(line);
        result = static_cast<const ValueType*>(&this->data[line->i][line->j]);
    }

    this->statAcess += 1;
    if (exist)
        this->statHit += 1;
    else
        this->statMiss += 1;

    return result;
}

template <typename ValueType>
void CacheMemory<ValueType>::Write(unsigned long addr, const ValueType* data) {
    CacheLine* victim = NULL;
    unsigned long tag = GetTag(addr);
    unsigned long index = GetIndex(addr);

    if (!FindEmptyEntry(addr, &victim)) {
        int set, way;
        this->policy->SelectVictim(tag, index, &set, &way);
        victim = &this->entries[set][way];
    }

    *victim = CacheLine(victim, tag, index);
    this->policy->Acess(victim);

    this->data[victim->i][victim->j] = *data;

    this->statEvaction += 1;
    return;
}

template <typename ValueType>
unsigned long CacheMemory<ValueType>::GetOffset(unsigned long addr) const {
    return addr & this->offsetMask;
}

template <typename ValueType>
unsigned long CacheMemory<ValueType>::GetIndex(unsigned long addr) const {
    return (addr & this->indexMask) >> this->offsetBits;
}

template <typename ValueType>
unsigned long CacheMemory<ValueType>::GetTag(unsigned long addr) const {
    return (addr & this->tagMask) >> (this->offsetBits + this->indexBits);
}

template <typename ValueType>
bool CacheMemory<ValueType>::GetEntry(unsigned long addr,
                                      CacheLine** result) const {
    unsigned long tag = this->GetTag(addr);
    unsigned long index = this->GetIndex(addr);
    for (int way = 0; way < this->numWays; ++way) {
        CacheLine* entry = &this->entries[index][way];

        if (entry->isValid && entry->tag == tag) {
            *result = entry;
            return true;
        }
    }
    return false;
}

template <typename ValueType>
bool CacheMemory<ValueType>::FindEmptyEntry(unsigned long addr,
                                            CacheLine** result) const {
    unsigned long index = this->GetIndex(addr);
    for (int way = 0; way < this->numWays; ++way) {
        CacheLine* entry = &this->entries[index][way];

        if (!entry->isValid) {
            *result = entry;
            return true;
        }
    }
    return false;
}

template <typename ValueType>
int CacheMemory<ValueType>::SetReplacementPolicy(const char* policyName) {
    if (strcmp(policyName, "lru") == 0) {
        this->policy =
            new ReplacementPolicies::LRU(this->numSets, this->numWays);
        return 0;
    }

    if (strcmp(policyName, "random") == 0) {
        this->policy =
            new ReplacementPolicies::Random(this->numSets, this->numWays);
        return 0;
    }

    if (strcmp(policyName, "roundrobin") == 0) {
        this->policy =
            new ReplacementPolicies::RoundRobin(this->numSets, this->numWays);
        return 0;
    }

    return 1;
}

template <typename ValueType>
void CacheMemory<ValueType>::resetStatistics() {
    this->statMiss = 0;
    this->statHit = 0;
    this->statAcess = 0;
    this->statEvaction = 0;
}

template <typename ValueType>
unsigned long CacheMemory<ValueType>::getStatMiss() const {
    return this->statMiss;
}

template <typename ValueType>
unsigned long CacheMemory<ValueType>::getStatHit() const {
    return this->statHit;
}

template <typename ValueType>
unsigned long CacheMemory<ValueType>::getStatAcess() const {
    return this->statAcess;
}

template <typename ValueType>
unsigned long CacheMemory<ValueType>::getStatEvaction() const {
    return this->statEvaction;
}

template <typename ValueType>
float CacheMemory<ValueType>::getStatValidProp() const {
    int n = this->numSets * this->numWays;
    int count = 0;
    for (int i = 0; i < n; ++i) {
        if (this->entries[0][i].isValid) count += 1;
    }
    return (float)count / (float)n;
}

#endif
