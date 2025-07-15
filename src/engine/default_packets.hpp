#ifndef SINUCA3_DEFAULT_PACKETS_HPP_
#define SINUCA3_DEFAULT_PACKETS_HPP_

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
 * @file default_packets.hpp
 * @brief Standard message types.
 */

#include <cstring>

namespace sinuca {

const unsigned int MAX_REGISTERS = 32;
const unsigned int MAX_MEM_OPERATIONS = 16;
const unsigned int TRACE_LINE_SIZE = 256;

/** @brief Enumerates the types of branches. */
enum Branch {
    BranchSyscall,
    BranchCall,
    BranchReturn,
    BranchUncond,
    BranchCond
};

/**
 * @brief Stores details of an instruction.
 * These details are static and cannot be changed during program execution.
 */
struct StaticInstructionInfo {
    char opcodeAssembly[TRACE_LINE_SIZE];

    long opcodeAddress;
    unsigned char opcodeSize;
    unsigned short int baseReg;
    unsigned short int indexReg;

    unsigned short readRegs[MAX_REGISTERS];
    unsigned char numReadRegs;
    unsigned short writeRegs[MAX_REGISTERS];
    unsigned char numWriteRegs;

    Branch branchType;
    bool isNonStdMemOp;
    bool isControlFlow;
    bool isIndirect;
    bool isPredicated;
    bool isPrefetch;

    inline StaticInstructionInfo() {
        memset(this, 0, sizeof(*this));
        memcpy(this->opcodeAssembly, "N/A", 4);
        this->branchType = BranchUncond;
    }
};

/**
 * @brief Stores details of an instruction.
 * These details are dynamic and will vary during program execution.
 *
 * An example of instructions that can change this value are non-standard memory
 * instructions, such as vgather and vscatter.
 */
struct DynamicInstructionInfo {
    long readsAddr[MAX_MEM_OPERATIONS];
    long writesAddr[MAX_MEM_OPERATIONS];
    int readsSize[MAX_MEM_OPERATIONS];
    int writesSize[MAX_MEM_OPERATIONS];
    unsigned short numReadings;
    unsigned short numWritings;

    inline DynamicInstructionInfo() { memset(this, 0, sizeof(*this)); }
};

/**
 * @brief Exchanged between the engine and components.
 *
 * @param staticInfo Stores details that are static and cannot vary during
 * program execution. It is a constant pointer to avoid unnecessary copying.
 *
 * @param DynamicInstructionInfo Stores details that are dynamic and will vary
 * during program execution. The idea is that it is allocated on the simulator
 * stack to avoid malloc for each instruction.
 */
struct InstructionPacket {
    const StaticInstructionInfo* staticInfo;
    DynamicInstructionInfo dynamicInfo;
};

/**
 * @brief Used by memory components.
 */
typedef unsigned long MemoryPacket;

enum PredictorPacketType {
    RequestQuery,
    RequestUpdate,
    ResponseUnknown,
    ResponseTake,
    ResponseTakeToAddress,
    ResponseDontTake,
};

/**
 * @brief Message exchanged between components and branch predictors such as
 * BTBs, RASs, etc.
 *
 * @details When a component wishes to query the predictor about a newly-arrived
 * instruction, it sends a RequestQuery message with the address of the
 * instruction, it's branch type (if known) and wether the branch type is known
 * or not (some fetchers may not know the type at the prediction stage). When
 * the query is performed, the predictor will anwser with a ResponseUnknown
 * message if has no prediction, a ResponseTake if the prediction is to take but
 * the target is not known, a ResponseTakeToAddress if the prediction is to take
 * and the address is known (data is filled with responseAddress), and a
 * ResponseDontTake if the prediction is to not take the branch.
 */
struct PredictorPacket {
    union {
        struct {
            unsigned long address;
            Branch branchType;
            bool isBranchTypeKnown;
        } requestQuery;

        struct {
            unsigned long address;
            unsigned long targetAddress;
            Branch branchType;
            bool wasTaken;
        } requestUpdate;

        unsigned long responseAddress;
    } data;
    PredictorPacketType type;
};

}  // namespace sinuca

#endif  // SINUCA3_DEFAULT_PACKETS_HPP_
