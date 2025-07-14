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
#include "../../../sinuca3.hpp"

// @param entry Ouput: A (CacheEntry *) to store result
// @param index Input: A (unsigned long) containing the index of addr to be acessed
#define for_each_way(entry, index) \
    for (int __way = 0; \
            __way < this->numWays && ((entry) = &this->entries[index][__way], true); \
            ++__way)

struct CacheEntry {
  unsigned long tag;
  unsigned long index;
  bool isValid;
  int i, j;
  unsigned long value; // Value Storaged in this entry
};

class Cache : public sinuca::Component<sinuca::MemoryPacket> {
  public:
    inline Cache() : numSets(0), numWays(0), entries(NULL), numberOfRequests(0) {};
    virtual ~Cache();
    virtual int FinishSetup();
    virtual int SetConfigParameter(const char* parameter,
                                   sinuca::config::ConfigValue value);
    virtual void Clock();
    virtual void Flush();
    virtual void PrintStatistics();

  protected:
    int numSets;
    int numWays;

    CacheEntry **entries; // matrix [sets x ways]
    unsigned long numberOfRequests;

    unsigned long GetIndex(unsigned long addr) const;
    unsigned long GetTag(unsigned long addr) const;
    bool GetEntry(unsigned long addr, CacheEntry **result) const;

    virtual bool Read(unsigned long addr, CacheEntry **result) = 0;
    virtual void Write(unsigned long addr, unsigned long value) = 0;
};

#endif
