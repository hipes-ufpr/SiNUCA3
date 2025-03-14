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

#include <stdint.h>  // uint32_t
#include <zlib.h>    // gzFile

#include <cstring>

#include "trace_reader.hpp"

const int TRACE_LINE_SIZE = 512;

namespace sinuca {
namespace traceReader {
namespace orcsTraceReader {

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
    InstructionOperation opcodeOperation;
    uint64_t opcodeAddress;
    uint32_t opcodeSize;

    uint32_t readRegs[16];
    uint32_t writeRegs[16];

    uint32_t baseReg;
    uint32_t indexReg;

    bool isRead;
    uint64_t readAddress;
    uint32_t readSize;

    bool isRead2;
    uint64_t read2Address;
    uint32_t read2Size;

    bool isWrite;
    uint64_t writeAddress;
    uint32_t writeSize;

    Branch branchType;
    bool isIndirect;

    bool isPredicated;
    bool isPrefetch;

    inline OpcodePackage() {
        memset(this, 0, sizeof(*this));

        memcpy(this->opcodeAssembly, "N/A", 4);
        this->branchType = BranchUncond;
    }
};

/** @brief Port of the trace reader from OrCS (a.k.a. SiNUCA2). */
class OrCSTraceReader : public TraceReader {
  private:
    gzFile gzStaticTraceFile;
    gzFile gzDynamicTraceFile;
    gzFile gzMemoryTraceFile;

    // Controls the trace reading.
    bool isInsideBBL;
    uint32_t currectBBL;
    uint32_t currectOpcode;

    // Controls the static dictionary.
    // Total of BBLs for the static file.
    // Total of instructions for each BBL.
    uint32_t binaryTotalBBLs;
    uint32_t *binaryBBLSize;
    // Complete dictionary of BBLs and instructions.
    OpcodePackage **binaryDict;

    uint64_t fetchInstructions;

    // Generate the static dictionary.
    int GetTotalBBLs();
    int DefineBinaryBBLSize();
    int GenerateBinaryDict();

    int TraceStringToOpcode(char *input_string, OpcodePackage *opcode);
    int TraceNextDynamic(uint32_t *next_bbl);
    int TraceNextMemory(uint64_t *next_address, uint32_t *operation_size,
                        bool *is_read);

    FetchResult TraceFetch(OpcodePackage *m);

  public:
    virtual int OpenTrace(const char *traceFileName);
    virtual unsigned long GetTraceSize();
    virtual unsigned long GetNumberOfFetchedInstructions();
    virtual void PrintStatistics();
    virtual FetchResult Fetch(InstructionPacket *ret);

    inline ~OrCSTraceReader() {
        gzclose(this->gzStaticTraceFile);
        gzclose(this->gzDynamicTraceFile);
        gzclose(this->gzMemoryTraceFile);

        for (uint32_t bbl = 1; bbl < this->binaryTotalBBLs; bbl++)
            delete[] this->binaryDict[bbl];

        delete[] this->binaryDict;
        delete[] this->binaryBBLSize;
    }
};

}  // namespace orcsTraceReader
}  // namespace traceReader
}  // namespace sinuca

#endif  // SINUCA3_ORCS_TRACE_READER_HPP
