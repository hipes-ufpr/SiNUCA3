#ifndef SINUCA3_MMU_HPP_
#define SINUCA3_MMU_HPP_

//
// Copyright (C) 2026  HiPES - Universidade Federal do Paraná
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
 * @file mmu.hpp
 * @brief Translation of virtual addresses to physical addresses.
 * @details This component is responsible for the translation of virtual
 * addresses to physical addresses, which implies that all cores must be
 * connected to it.
 */

#include <sinuca3.hpp>

class MemoryManagementUnit : public Component<MemoryPacket> {
  private:
    std::vector<long> pageTableRoot;
    long lastPageTableRoot; /** @brief Used to assign new roots. */

    void RegisterPageTableRoot(int connId);
    unsigned long TranslateAddress(unsigned long vtAddress, int connId);
  public:
    MemoryManagementUnit() : lastPageTableRoot(0) {}
    virtual int Configure(Config config) { (void)config; return 0; }
    virtual void Clock();
    virtual void PrintStatistics() {}

    virtual ~MemoryManagementUnit() {}
};

#endif  // SINUCA3_MMU_HPP_