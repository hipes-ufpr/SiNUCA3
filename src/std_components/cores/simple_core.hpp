#ifndef SINUCA3_SIMPLE_CORE_HPP_
#define SINUCA3_SIMPLE_CORE_HPP_

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
 * @file simple_core.hpp
 * @details Public API of the SimpleCore, a testing core that executes
 * everything in a single clock cycle.
 */

#include <sinuca3.hpp>

#include "engine/linkable.hpp"

/**
 * @details SimpleCore fetches an instruction from the required parameter
 * `fetching`, a Component<InstructionPacket> and optionally queries two
 * memories (without caring with the result at all) with the instruction. The
 * two memories are passed as the Component<MemoryPacket> parameters
 * `instructionMemory` and `dataMemory`.
 */
class SimpleCore : public Component<InstructionPacket> {
  private:
    Component<FetchPacket>* engine; /** @brief The engine component */
    Component<MemoryPacket>*
        instructionMemory;               /** @brief The instruction memory. */
    Component<MemoryPacket>* dataMemory; /** @brief The data memory. */
    Component<int>* fetcher;             /** @brief The fetcher. */
    Component<MemoryPacket*>* mmu;        /** @brief Memory management unit. */

    unsigned long numFetchedInstructions; /** @brief The number of fetched
                                             instructions. */
    int engineConnectionID;      /** @brief The connection ID of the engine. */
    int instructionConnectionID; /** @brief The connection ID of
                                    instructionMemory. */
    int dataConnectionID;        /** @brief The connection ID of dataMemory. */
    int fetcherConnectionID;     /** @brief The connection ID of fetcher. */
    int mmuConnectionID;         /** @brief The connection ID of mmu. */
    int coreId;                  /** @brief The ID of the core. */

    /** @brief Establish the core ID for the core. */
    int EstablishCoreID();
    /** @brief Establish the connections for the core. */
    int EstablishConnections(Config config);
    /** @brief Establish the child components for the core. */
    int EstablishChildComponents();
    /** @brief Establish the context for the core and propagate it to the child
     * components. */
    int EstablishContextAndPropagate();

    /** @brief Static counter to assign unique core IDs. */
    static int nextCoreID;
    /** @brief Increment the core ID counter. */
    static inline void StepCoreID() { nextCoreID++; }

  public:
    inline SimpleCore()
        : engine(NULL),
          instructionMemory(NULL),
          dataMemory(NULL),
          fetcher(NULL),
          mmu(NULL),
          numFetchedInstructions(0) {}
    virtual int Configure(Config config);
    virtual int PosConfigure();
    virtual void Clock();
    virtual void PrintStatistics();
    ~SimpleCore();
};

#endif  // SINUCA3_SIMPLE_CORE_HPP_
