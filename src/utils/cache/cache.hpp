#ifndef SINUCA3_CACHE_NWAY_HPP_
#define SINUCA3_CACHE_NWAY_HPP_

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
 * @file cache_nway.hpp
 * @details WP! Abstract
 */

#include <cstddef>
#include <sinuca3.hpp>

#include "replacement_policy.hpp"

enum ReplacementPoliciesID {
  LruID = 0,
  RandomID = 1,
  RoundRobinID = 2
};

struct CacheEntry {
    unsigned long tag;
    unsigned long index;
    bool isValid;

    // This is the position of this entry in the entries matrix.
    // It can be useful for organizing other matrices.
    int i, j;

    CacheEntry() {};

    inline CacheEntry(int i, int j, unsigned long tag, unsigned long index)
        : tag(tag), index(index), isValid(false), i(i), j(j) {};

    inline CacheEntry(CacheEntry *entry, unsigned long tag, unsigned long index)
        : tag(tag),
          index(index),
          isValid(true),
          i(entry->i),
          j(entry->j){};
};

class Cache {
  public:
    inline Cache() : numSets(0), numWays(0), entries(NULL), policy(NULL) {};
    virtual ~Cache();

    int FinishSetup();
    int SetConfigParameter(const char *parameter, ConfigValue value);

    bool SetReplacementPolicy(ReplacementPoliciesID id);

    /**
     * @brief Reads a cache.
     * @param addr Address to look for.
     * @return True if is HIT, false if is MISS.
     */
    bool Read(MemoryPacket addr);

    /**
     * @brief Write a cache.
     * @detail This just simulate the behavior. So, no actual data need to be stored.
     * If no empty (isValid == false) is found, a replacement algorithm must choose which
     * items to discard.
     * @param addr Address to write to.
     */
    void Write(MemoryPacket addr);

    unsigned long GetIndex(unsigned long addr) const;
    unsigned long GetTag(unsigned long addr) const;

    // Find the entry for a addr.
    // If it returns true,

    /**
     * @brief Find the entry for a addr.
     * @param addr Address to look for.
     * @param result Pointer to store search result.
     * @return True if found, false otherwise.
     */
    bool GetEntry(unsigned long addr, CacheEntry **result) const;

    /**
     * @brief Can be used to find a entry that is not valid.
     * @param addr Address to look for.
     * @param result Pointer to store result.
     * @return True if victim is found, false otherwise.
     */
    bool FindEmptyEntry(unsigned long addr, CacheEntry **result) const;

    int numSets;
    int numWays;
    CacheEntry **entries;  // matrix [sets x ways]

    private:
        ReplacementPolicy *policy;
};

#endif
