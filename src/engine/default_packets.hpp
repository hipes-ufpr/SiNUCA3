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

const int MAX_REGISTERS = 16;
const int MAX_MEM_OPERATIONS = 16;
const int TRACE_LINE_SIZE = 256;
/**
 * @brief Intel Pin warns that any size < 23 may cause output to be truncated.
 * This value might increase in the future, so it is set to 25 for safety.
 */
const int INST_MNEMONIC_LEN = 25 + sizeof('\0');

/** @brief Enumerates the types of branches. */
enum Branch {
    BranchNone,
    BranchSyscall,
    BranchCall,
    BranchSysret,
    BranchRet,
    BranchUncond,
    BranchCond
};

/**
 * @brief Stores details of an instruction.
 * These details are static and wont change during program execution.
 */
struct StaticInstructionInfo {
    unsigned long instAddress;
    unsigned long instSize;
    unsigned int instPredicate;

    Branch branchType;

    unsigned short readRegsArray[MAX_REGISTERS];
    unsigned short writtenRegsArray[MAX_REGISTERS];

    unsigned char numberOfReadRegs;
    unsigned char numberOfWriteRegs;

    bool isPrefetchHintInst : 1;
    bool isPredicatedInst : 1;
    bool isIndirectControlFlowInst : 1;
    bool instCausesCacheLineFlush : 1;
    bool instPerformsAtomicUpdate : 1;
    bool instReadsMemory : 1;
    bool instWritesMemory : 1;

    char instMnemonic[INST_MNEMONIC_LEN];

    inline StaticInstructionInfo() {
        memset(this, 0, sizeof(*this));
        memcpy(this->instMnemonic, "N/A", 4);
        this->branchType = BranchNone;
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
    unsigned long readsAddr[MAX_MEM_OPERATIONS];
    unsigned long writesAddr[MAX_MEM_OPERATIONS];
    unsigned int readsSize[MAX_MEM_OPERATIONS];
    unsigned int writesSize[MAX_MEM_OPERATIONS];
    unsigned short numReadings;
    unsigned short numWritings;
};

/**
 * @brief Carries the information regarding an executed instruction.
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
    unsigned long nextInstruction; /** @brief The engine fills this as it
                             buffers the next instruction. */
};

/**
 * @brief Exchanged between the engine and components. It's never ambiguous
 * wether this is a request or response, so it does not need to be a tagged
 * union.
 *
 * @param request A request specifies an amount in bytes to fetch. The engine
 * will fetch up to this amount in instructions. Specifying an amount less than
 * the minimum instruction size may lead to deadlocks. Specifying 0 will fetch
 * a single instruction without any care over the size. Each instruction fetched
 * will be sended on it's own message. For this reason it's a good idea to
 * connect to the engine without a maximum buffer size.
 *
 * @param response A fetched instruction.
 */
union FetchPacket {
    long request; /** @brief Amount of bytes to fetch. 0 to fetch a single
                     instruction regardless of it's size. */
    InstructionPacket response; /** @brief The fetched instruction. */
};

/**
 * @brief Used by memory components.
 */
typedef unsigned long MemoryPacket;

/**
 * @brief Tag for the PredictorPacket union.
 */
enum PredictorPacketType {
    PredictorPacketTypeRequestQuery,
    PredictorPacketTypeRequestTargetUpdate,
    PredictorPacketTypeRequestDirectionUpdate,
    PredictorPacketTypeResponseUnknown,
    PredictorPacketTypeResponseTake,
    PredictorPacketTypeResponseTakeToAddress,
    PredictorPacketTypeResponseDontTake,
};

/**
 * @brief Message exchanged between components and branch predictors such as
 * BTBs, RASs, etc. Tagged union with PredictorPacketType.
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
        InstructionPacket
            requestQuery; /** @brief A request to predict a instruction. */

        struct {
            InstructionPacket instruction; /** @brief The instruction. */
            bool taken;                    /** @brief True if taken */
        } directionUpdate; /** @brief A request to update the direction of an
                                instruction. */

        struct {
            InstructionPacket instruction; /** @brief The instruction. */
            unsigned long target;          /** @brief Its target. */
        } targetUpdate; /** @brief A request to update the target of an
                            instruction. */

        struct {
            InstructionPacket instruction; /** @brief The instruction. */
            unsigned long target;          /** @brief Its target. */
        } targetResponse;                  /** @brief Data of response types. */
    } data;                                /** @brief The data. */
    PredictorPacketType type;              /** @brief The tag. */
};

#endif  // SINUCA3_DEFAULT_PACKETS_HPP_
