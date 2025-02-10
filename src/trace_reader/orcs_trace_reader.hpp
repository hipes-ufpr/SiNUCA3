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

const int TRACE_LINE_SIZE = 512;

namespace sinuca {
namespace orcsTraceReader {

/** @brief Enumerates the INSTRUCTION (Opcode and Uop) operation type. */
enum InstructionOperation {
    // NOP
    INSTRUCTION_OPERATION_NOP,
    // INTEGERS
    INSTRUCTION_OPERATION_INT_ALU,
    INSTRUCTION_OPERATION_INT_MUL,
    INSTRUCTION_OPERATION_INT_DIV,
    // FLOAT POINT
    INSTRUCTION_OPERATION_FP_ALU,
    INSTRUCTION_OPERATION_FP_MUL,
    INSTRUCTION_OPERATION_FP_DIV,
    // BRANCHES
    INSTRUCTION_OPERATION_BRANCH,
    // MEMORY OPERATIONS
    INSTRUCTION_OPERATION_MEM_LOAD,
    INSTRUCTION_OPERATION_MEM_STORE,
    // NOT IDENTIFIED
    INSTRUCTION_OPERATION_OTHER,
    // SYNCHRONIZATION
    INSTRUCTION_OPERATION_BARRIER,
    // HMC
    INSTRUCTION_OPERATION_HMC_ROA,  // #12 READ+OP +Answer
    INSTRUCTION_OPERATION_HMC_ROWA  // #13 READ+OP+WRITE +Answer
};

/** @brief Enumerates the types of branches. */
enum Branch {
    BRANCH_SYSCALL,
    BRANCH_CALL,
    BRANCH_RETURN,
    BRANCH_UNCOND,
    BRANCH_COND
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
        this->branchType = BRANCH_UNCOND;
    }
};

/** @brief Port of the trace reader from OrCS (a.k.a. SiNUCA2). */
class OrCSTraceReader {
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

  public:
    int OpenTrace(const char *trace);

    inline ~OrCSTraceReader() {
        gzclose(this->gzStaticTraceFile);
        gzclose(this->gzDynamicTraceFile);
        gzclose(this->gzMemoryTraceFile);
    }

    void Statistics();

    // Generate the static dictionary.
    int GetTotalBBLs();
    int DefineBinaryBBLSize();
    int GenerateBinaryDict();

    int TraceStringToOpcode(char *input_string, OpcodePackage *opcode);
    int TraceNextDynamic(uint32_t *next_bbl);
    int TraceNextMemory(uint64_t *next_address, uint32_t *operation_size,
                        bool *is_read);
    int TraceFetch(OpcodePackage *m);
};

}  // namespace orcsTraceReader
}  // namespace sinuca

#endif  // SINUCA3_ORCS_TRACE_READER_HPP
