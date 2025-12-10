#ifndef SINUCA3_REPLACEMENT_POLICY_HPP_
#define SINUCA3_REPLACEMENT_POLICY_HPP_

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

#include <sinuca3.hpp>

// Foward Declaration
struct CacheLine;
template <typename ValueType>
class CacheMemory;

class ReplacementPolicy {
  public:
    ReplacementPolicy(int numSets, int numWays)
        : numSets(numSets), numWays(numWays) {};
    virtual ~ReplacementPolicy() {};

    virtual void Acess(CacheLine* entry) = 0;
    virtual void SelectVictim(unsigned long tag, unsigned long index,
                              int* resultSet, int* resultWay) = 0;

  protected:
    int numSets;
    int numWays;
};

#endif
