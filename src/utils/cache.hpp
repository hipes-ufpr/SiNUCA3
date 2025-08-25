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

struct CacheEntry {
    unsigned long tag;
    unsigned long index;
    bool isValid;
    int i, j;
    unsigned long value;  // Value Storaged in this entry

    CacheEntry() {};

    inline CacheEntry(int i, int j, unsigned long tag, unsigned long index, unsigned long value)
    : tag(tag), index(index), isValid(false), i(i), j(j), value(value) {};

    inline CacheEntry(CacheEntry *entry, unsigned long tag, unsigned long index, unsigned long value)
    : tag(tag), index(index), isValid(true), i(entry->i), j(entry->j), value(value) {};
};

class Cache {
  public:
    inline Cache()
        : numSets(0), numWays(0), entries(NULL) {};
    virtual ~Cache();

    int FinishSetup();
    int SetConfigParameter(const char *parameter,
                                   ConfigValue value);

    int numSets;
    int numWays;

    CacheEntry **entries;  // matrix [sets x ways]

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
     * @brief Can be used to find a entry that is not valid yet.
     * @detail  If no victim is found, a replacement algorithm must choose which items to discard to make room for new data.
     * @param addr Address to look for.
     * @param result Pointer to store result.
     * @return True if victim is found, false otherwise.
     */
    bool FindEmptyEntry(unsigned long addr, CacheEntry **result) const;
};

#endif
