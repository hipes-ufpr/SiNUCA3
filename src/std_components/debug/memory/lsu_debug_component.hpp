#ifndef SINUCA3_LSU_DEBUG_COMPONENT_HPP
#define SINUCA3_LSU_DEBUG_COMPONENT_HPP

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
 * @file lsu_debug_component.hpp
 * @brief Implementation of a component to test the lsu. THIS FILE SHALL ONLY
 * BE INCLUDED BY CODE PATHS THAT ONLY COMPILE IN DEBUG MODE.
 * @details
 */

#include <sinuca3.hpp>
#include <std_components/memory/lsu.hpp>
#include "utils/pair.hpp"

enum InstructionType {
    InstructionTypeLoad,
    InstructionTypeStore,
    InstructionTypeOther
};

struct InstructionDecode {
    InstructionType type;
    int remainingCycles;
    long seqNum; /* Only for store requests */
    unsigned long address; /* Only used for loads and stores. */
};

struct DebugPacketLSU;

class LSUDebugComponent : public Component<DebugPacketLSU> {
  private:
    Component<FetchPacket>* fetch;
    Component<LSUPacket>* lsu;

    CircularBuffer instCommitQueue;
    std::vector<pair::Pair<unsigned long, long> > resolvedRequests;

    int lsuConnId;
    int fetchConnectionId;
    int fixedInstLatency;
    int loadsCounter;
    int storesCounter;

  public:
    inline LSUDebugComponent()
        : fetch(NULL),
          lsu(NULL),
          lsuConnId(-1),
          fetchConnectionId(-1),
          fixedInstLatency(2),
          loadsCounter(0),
          storesCounter(0) {
        this->instCommitQueue.Allocate(0, sizeof(InstructionDecode));
    }

    virtual int Configure(Config config);
    virtual void Clock();
    virtual void PrintStatistics();

    virtual ~LSUDebugComponent() { this->instCommitQueue.Deallocate(); }
};

#endif  // LSU_DEBUG_COMPONENT_HPP