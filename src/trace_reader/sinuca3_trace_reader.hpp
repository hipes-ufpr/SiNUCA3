#ifndef SINUCA3_ORCS_TRACE_READER_HPP
#define SINUCA3_ORCS_TRACE_READER_HPP

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
 * @file orcs_trace_reader.hpp
 * @brief Port of the OrCS trace reader. https://github.com/mazalves/OrCS
 */

#include <cstdio>
#include <cstring>
#include "trace_reader.hpp"

#define MAX_REGISTERS 32
#define MAX_MEM_OPERATIONS 16

const int TRACE_LINE_SIZE = 512;

namespace sinuca {
namespace traceReader {
namespace sinuca3TraceReader {

/** @brief Enumerates the INSTRUCTION (Opcode and Uop) operation type. */
enum InstructionOperation {
    // NOP
    InstructionOperationNop,
    // INTEGERS
    InstructionOperationIntAlu,
    InstructionOperationIntMul,
    InstructionOperationIntDiv,
    // FLOAT POINT
    InstructionOperationFPAlu,
    InstructionOperationFPMul,
    InstructionOperationFPDiv,
    // BRANCHES
    InstructionOperationBranch,
    // MEMORY OPERATIONS
    InstructionOperationMemLoad,
    InstructionOperationMemStore,
    // NOT IDENTIFIED
    InstructionOperationOther,
    // SYNCHRONIZATION
    InstructionOperationBarrier,
    // HMC
    InstructionOperationHMCROA,  // #12 READ+OP +Answer
    InstructionOperationHMCROWA  // #13 READ+OP+WRITE +Answer
};

/** @brief Enumerates the types of branches. */
enum Branch {
    BranchSyscall,
    BranchCall,
    BranchReturn,
    BranchUncond,
    BranchCond
};

struct OpcodePackage {
    char opcodeAssembly[TRACE_LINE_SIZE];
    // instruction_operation_t opcode_operation; //
    // uint32_t instruction_id; //
    
    long opcodeAddress;
    unsigned char opcodeSize;
    unsigned short int baseReg;
    unsigned short int indexReg;

    unsigned short int readRegs[MAX_REGISTERS];
    unsigned short int writeRegs[MAX_REGISTERS];

    // long readsAddr[MAX_MEM_OPERATIONS]; //
    unsigned int readsSize[MAX_MEM_OPERATIONS];
    unsigned int numReads;

    // long writesAddr[MAX_MEM_OPERATIONS]; //
    unsigned int writesSize[MAX_MEM_OPERATIONS];
    unsigned int numWrites;

    Branch branchType;
    bool isControlFlow;
    bool isIndirect;
    bool isPredicated;
    bool isPrefetch;
    bool isHive;
    int hive_read1 = -1;
    int hive_read2 = -1;
    int hive_write = -1;
    bool isVima;

    inline OpcodePackage() {
        memset(this, 0, sizeof(*this));
        memcpy(this->opcodeAssembly, "N/A", 4);
        this->branchType = BranchUncond;
    }
};

/** @brief  */
class SinucaTraceReader : public TraceReader {
  private:
    FILE *StaticTraceFile;
    FILE *DynamicTraceFile;
    FILE *MemoryTraceFile;

    // Controls the trace reading.
    bool isInsideBBL;
    unsigned int currentBBL;
    unsigned int currentOpcode;

    // Controls the static dictionary.
    // Total of BBLs for the static file.
    // Total of instructions for each BBL.
    unsigned int binaryTotalBBLs;
    unsigned short *binaryBBLsSize;
    // Complete dictionary of BBLs and instructions.
    OpcodePackage **binaryDict;

    unsigned long fetchInstructions;

    // Generate the static dictionary.
    int GetTotalBBLs();
    int GenerateBinaryDict();

    int TraceNextDynamic(unsigned int*);
    int TraceNextMemory(long*, unsigned int*, bool*);

    FetchResult TraceFetch(const OpcodePackage**);

  public:
    virtual int OpenTrace(const char*);
    virtual void PrintStatistics();
    virtual FetchResult Fetch(InstructionPacket*);

    inline ~SinucaTraceReader() {
        fclose(this->StaticTraceFile);
        fclose(this->DynamicTraceFile);
        fclose(this->MemoryTraceFile);
    }
};

}  // namespace orcsTraceReader
}  // namespace traceReader
}  // namespace sinuca

#endif  // SINUCA3_ORCS_TRACE_READER_HPP