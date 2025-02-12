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
 * @file orcs_trace_reader.cpp
 * @brief Port of the OrCS trace reader. https://github.com/mazalves/OrCS
 */

#include "orcs_trace_reader.hpp"

#include <cassert>
#include <cstdio>   // NULL, SEEK_SET, snprintf
#include <cstdlib>  // strtoul
#include <cstring>  // strcmp, strcpy

#include "../utils/logging.hpp"
#include "trace_reader.hpp"

int sinuca::traceReader::orcsTraceReader::OrCSTraceReader::OpenTrace(
    const char *traceFileName) {
    char fileName[TRACE_LINE_SIZE];

    fileName[0] = '\0';
    snprintf(fileName, sizeof(fileName), "%s.tid%d.stat.out.gz", traceFileName,
             0);
    SINUCA3_DEBUG_PRINTF("Static File = %s => READY !\n", fileName);

    // Open the .gz file.
    this->gzStaticTraceFile = gzopen(fileName, "ro");
    if (this->gzStaticTraceFile == NULL) {
        SINUCA3_ERROR_PRINTF("Could not open the static file.\n%s\n", fileName);
        return 1;
    }

    SINUCA3_DEBUG_PRINTF("Static File = %s => READY !\n", fileName);

    // Open the Dynamic Trace File.
    fileName[0] = '\0';
    snprintf(fileName, sizeof(fileName), "%s.tid%d.dyn.out.gz", traceFileName,
             0);

    // Open the .gz group.
    this->gzDynamicTraceFile = gzopen(fileName, "ro");
    if (this->gzDynamicTraceFile == NULL) {
        SINUCA3_ERROR_PRINTF("Could not open the dynamic file.\n%s\n",
                             fileName);
        return 1;
    }
    SINUCA3_DEBUG_PRINTF("Dynamic File = %s => READY !\n", fileName);

    // Open the Memory Trace File.
    fileName[0] = '\0';
    snprintf(fileName, sizeof(fileName), "%s.tid%d.mem.out.gz", traceFileName,
             0);

    // Open the .gz group.
    this->gzMemoryTraceFile = gzopen(fileName, "ro");
    if (this->gzMemoryTraceFile == NULL) {
        SINUCA3_ERROR_PRINTF("Could not open the memory file.\n%s\n", fileName);
        return 1;
    }
    SINUCA3_DEBUG_PRINTF("Memory File = %s => READY !\n", fileName);

    // Set the trace reader controls.
    this->isInsideBBL = false;
    this->currectBBL = 0;
    this->currectOpcode = 0;

    // Obtain the number of BBLs.
    if (this->GetTotalBBLs()) return 1;

    this->binaryBBLSize = new uint32_t[this->binaryTotalBBLs];
    memset(this->binaryBBLSize, 0,
           this->binaryTotalBBLs * sizeof(*this->binaryBBLSize));

    // Define the size of each specific BBL.
    if (this->DefineBinaryBBLSize()) return 1;

    // Create the opcode for each BBL.
    this->binaryDict = new OpcodePackage *[this->binaryTotalBBLs];
    for (uint32_t bbl = 1; bbl < this->binaryTotalBBLs; bbl++) {
        this->binaryDict[bbl] = new OpcodePackage[this->binaryBBLSize[bbl]];
    }

    if (this->GenerateBinaryDict()) return 1;

    return 0;
}

int sinuca::traceReader::orcsTraceReader::OrCSTraceReader::GetTotalBBLs() {
    char file_line[TRACE_LINE_SIZE] = "";
    bool file_eof = false;
    uint32_t bbl = 0;
    this->binaryTotalBBLs = 0;

    // Go to the Begin of the File and Check is it's not EOF.
    gzclearerr(this->gzStaticTraceFile);
    gzseek(this->gzStaticTraceFile, 0, SEEK_SET);
    file_eof = gzeof(this->gzStaticTraceFile);

    if (file_eof) {
        SINUCA3_ERROR_PRINTF("Static File Unexpected EOF.\n");
        return 1;
    }

    while (!file_eof) {
        gzgets(this->gzStaticTraceFile, file_line, TRACE_LINE_SIZE);
        file_eof = gzeof(this->gzStaticTraceFile);

        if (file_line[0] == '@') {
            bbl = (uint32_t)strtoul(file_line + 1, NULL, 10);
            this->binaryTotalBBLs++;
            if (bbl != this->binaryTotalBBLs) {
                SINUCA3_ERROR_PRINTF("Expected sequenced bbls.\n");
                return 1;
            }
        }
    }

    this->binaryTotalBBLs++;

    return 0;
}

int sinuca::traceReader::orcsTraceReader::OrCSTraceReader::
    DefineBinaryBBLSize() {
    char file_line[TRACE_LINE_SIZE] = "";
    bool file_eof = false;
    uint32_t bbl = 0;

    // Go to the Begin of the File and Check is it's not EOF.
    gzclearerr(this->gzStaticTraceFile);
    gzseek(this->gzStaticTraceFile, 0, SEEK_SET);
    file_eof = gzeof(this->gzStaticTraceFile);

    if (file_eof) {
        SINUCA3_ERROR_PRINTF("Static File Unexpected EOF.\n");
        return 1;
    }

    while (!file_eof) {
        gzgets(this->gzStaticTraceFile, file_line, TRACE_LINE_SIZE);
        file_eof = gzeof(this->gzStaticTraceFile);

        if (file_line[0] == '\0' || file_line[0] == '#') {
            // If Comment, then ignore
            continue;
        } else if (file_line[0] == '@') {
            bbl++;
            binaryBBLSize[bbl] = 0;
        } else {
            binaryBBLSize[bbl]++;
        }
    }

    return 0;
}

int sinuca::traceReader::orcsTraceReader::OrCSTraceReader::
    GenerateBinaryDict() {
    char file_line[TRACE_LINE_SIZE] = "";
    bool file_eof = false;
    uint32_t bbl = 0;  // Actual BBL (Index of the Vector).
    uint32_t opcode = 0;
    OpcodePackage NewOpcode;  // Actual Opcode.

    // Go to the Begin of the File and Check is it's not EOF.
    gzclearerr(this->gzStaticTraceFile);
    gzseek(this->gzStaticTraceFile, 0, SEEK_SET);
    file_eof = gzeof(this->gzStaticTraceFile);

    if (file_eof) {
        SINUCA3_ERROR_PRINTF("Static File Unexpected EOF.\n");
        return 1;
    }

    while (!file_eof) {
        gzgets(this->gzStaticTraceFile, file_line, TRACE_LINE_SIZE);
        file_eof = gzeof(this->gzStaticTraceFile);

        SINUCA3_DEBUG_PRINTF("Read: %s\n", file_line);
        if (file_line[0] == '\0' || file_line[0] == '#') {
            // If Comment, then ignore.
            continue;
        } else if (file_line[0] == '@') {
            // If New BBL.
            SINUCA3_DEBUG_PRINTF("BBL %u with %u instructions.\n", bbl,
                                 opcode);  // Debug from previous BBL.
            opcode = 0;

            bbl = (uint32_t)strtoul(file_line + 1, NULL, 10);
            if (bbl >= this->binaryTotalBBLs) {
                SINUCA3_ERROR_PRINTF(
                    "Static has more BBLs than previous analyzed static "
                    "file.\n");
                return 1;
            }
        } else {
            // If Inside BBL.
            SINUCA3_DEBUG_PRINTF("Opcode %u = %s", opcode, file_line);
            if (this->TraceStringToOpcode(file_line,
                                          &this->binaryDict[bbl][opcode])) {
                return 1;
            }
            if (this->binaryDict[bbl][opcode].opcodeAddress == 0) {
                SINUCA3_ERROR_PRINTF(
                    "Static trace file generating opcode address equal to "
                    "zero.\n");
                return 1;
            }
            ++opcode;
        }
    }

    return 0;
}

/**
 * clang-format off
 * @details
 * Convert Static Trace line into Instruction
 * Field #:   01 |   02   |   03  |  04   |   05  |  06  |  07    |  08   |  09
 * |  10   |  11  |  12   |  13   |  14   |  15      |  16        | 17 Type: Asm
 * | Opcode | Inst. | Inst. | #Read | Read | #Write | Write | Base | Index | Is
 * | Is    | Is    | Cond. | Is       | Is         | Is Cmd | Number | Addr. |
 * Size  | Regs  | Regs | Regs   | Regs  | Reg. | Reg.  | Read | Read2 | Write |
 * Type  | Indirect | Predicated | Pfetch
 *
 * Static File Example:
 *
 * #
 * # Compressed Trace Generated By Pin to SiNUCA
 * #
 * @1
 * MOV 8 4345024 3 1 12 1 19 12 0 1 3 0 0 0 0 0
 * ADD 1 4345027 4 1 12 2 12 34 0 0 3 0 0 0 0 0 0
 * TEST 1 4345031 3 2 19 19 1 34 0 0 3 0 0 0 0 0 0
 * JNZ 7 4345034 2 2 35 34 1 35 0 0 4 0 0 0 1 0 0
 * @2
 * CALL_NEAR 9 4345036 5 2 35 15 2 35 15 15 0 1 0 0 1 0 0 0
 * clang-format on
 */
int sinuca::traceReader::orcsTraceReader::OrCSTraceReader::TraceStringToOpcode(
    char *input_string, OpcodePackage *opcode) {
    char *sub_string = NULL;
    char *tmp_ptr = NULL;
    uint32_t sub_fields, count, i;
    count = 0;

    for (i = 0; input_string[i] != '\0'; i++) count += (input_string[i] == ' ');

    if (count < 13) {
        SINUCA3_ERROR_PRINTF(
            "Error converting Text to Instruction (Wrong  number "
            "of fields %d), input_string = %s\n",
            count, input_string);
        return 1;
    }

    sub_string = strtok_r(input_string, " ", &tmp_ptr);
    strcpy(opcode->opcodeAssembly, sub_string);

    sub_string = strtok_r(NULL, " ", &tmp_ptr);
    opcode->opcodeOperation =
        InstructionOperation(strtoul(sub_string, NULL, 10));

    sub_string = strtok_r(NULL, " ", &tmp_ptr);
    opcode->opcodeAddress = strtoull(sub_string, NULL, 10);

    sub_string = strtok_r(NULL, " ", &tmp_ptr);
    opcode->opcodeSize = strtoul(sub_string, NULL, 10);

    // Number of Read Registers.
    sub_string = strtok_r(NULL, " ", &tmp_ptr);
    sub_fields = strtoul(sub_string, NULL, 10);

    for (i = 0; i < sub_fields; i++) {
        sub_string = strtok_r(NULL, " ", &tmp_ptr);
        opcode->readRegs[i] = strtoul(sub_string, NULL, 10);
    }

    // Number of Write Registers.
    sub_string = strtok_r(NULL, " ", &tmp_ptr);
    sub_fields = strtoul(sub_string, NULL, 10);

    for (i = 0; i < sub_fields; i++) {
        sub_string = strtok_r(NULL, " ", &tmp_ptr);
        opcode->writeRegs[i] = strtoul(sub_string, NULL, 10);
    }

    sub_string = strtok_r(NULL, " ", &tmp_ptr);
    opcode->baseReg = strtoull(sub_string, NULL, 10);

    sub_string = strtok_r(NULL, " ", &tmp_ptr);
    opcode->indexReg = strtoull(sub_string, NULL, 10);

    sub_string = strtok_r(NULL, " ", &tmp_ptr);
    opcode->isRead = (sub_string[0] == '1');

    sub_string = strtok_r(NULL, " ", &tmp_ptr);
    opcode->isRead2 = (sub_string[0] == '1');

    sub_string = strtok_r(NULL, " ", &tmp_ptr);
    opcode->isWrite = (sub_string[0] == '1');

    sub_string = strtok_r(NULL, " ", &tmp_ptr);
    opcode->branchType = Branch(strtoull(sub_string, NULL, 10));

    sub_string = strtok_r(NULL, " ", &tmp_ptr);
    opcode->isIndirect = (sub_string[0] == '1');

    sub_string = strtok_r(NULL, " ", &tmp_ptr);
    opcode->isPredicated = (sub_string[0] == '1');

    sub_string = strtok_r(NULL, " ", &tmp_ptr);
    opcode->isPrefetch = (sub_string[0] == '1');

    return 0;
}

// =============================================================================
int sinuca::traceReader::orcsTraceReader::OrCSTraceReader::TraceNextDynamic(
    uint32_t *next_bbl) {
    static char file_line[TRACE_LINE_SIZE];
    file_line[0] = '\0';

    bool valid_dynamic = false;
    *next_bbl = 0;

    while (!valid_dynamic) {
        // Obtain the next trace line.
        if (gzeof(this->gzDynamicTraceFile)) {
            return 1;
        }
        char *buffer =
            gzgets(this->gzDynamicTraceFile, file_line, TRACE_LINE_SIZE);
        if (buffer == NULL) {
            return 1;
        }

        // Analyze the trace line.
        if (file_line[0] == '\0' || file_line[0] == '#') {
            SINUCA3_DEBUG_PRINTF("Dynamic trace line (empty/comment): %s\n",
                                 file_line);
            continue;
        } else if (file_line[0] == '$') {
            SINUCA3_DEBUG_PRINTF("Dynamic trace line (synchronization): %s\n",
                                 file_line);
            continue;
        } else {
            // BBL is always greater than 0.
            // If strtoul==0 the line could not be converted.
            SINUCA3_DEBUG_PRINTF("Dynamic trace line: %s\n", file_line);

            *next_bbl = strtoul(file_line, NULL, 10);
            if (*next_bbl == 0) {
                SINUCA3_ERROR_PRINTF(
                    "The BBL from the dynamic trace file should "
                    "not be zero. Dynamic line %s\n",
                    file_line);
                return 1;
            }

            valid_dynamic = true;
        }
    }

    return 0;
}

/**
 * @details
 * clang-format off
 * Convert Dynamic Memory Trace line into Instruction Memory Operands
 * Field #:    1  |  2   |    3    |  4
 * Type:      R/W | R/W  | Memory  | BBL
 *            Op. | Size | Address | Number
 *
 * Memory File Example:
 *
 * #
 * # Compressed Trace Generated By Pin to SiNUCA
 * #
 * W 8 140735291283448 1238
 * W 8 140735291283440 1238
 * W 8 140735291283432 1238
 * clang-format on
 */
int sinuca::traceReader::orcsTraceReader::OrCSTraceReader::TraceNextMemory(
    uint64_t *mem_address, uint32_t *mem_size, bool *mem_is_read) {
    static char file_line[TRACE_LINE_SIZE];
    file_line[0] = '\0';

    bool valid_memory = false;

    while (!valid_memory) {
        // Obtain the next trace line.
        if (gzeof(this->gzMemoryTraceFile)) {
            return 1;
        }
        char *buffer =
            gzgets(this->gzMemoryTraceFile, file_line, TRACE_LINE_SIZE);
        if (buffer == NULL) {
            return 1;
        }

        // Analyze the trace line.
        if (file_line[0] == '\0' || file_line[0] == '#') {
            SINUCA3_DEBUG_PRINTF("Memory trace line (empty/comment): %s\n",
                                 file_line);
            continue;
        } else {
            char *sub_string = NULL;
            char *tmp_ptr = NULL;
            uint32_t count = 0, i = 0;
            while (file_line[i] != '\0') {
                count += (file_line[i] == ' ');
                i++;
            }
            if (count != 3) {
                SINUCA3_ERROR_PRINTF(
                    "Error converting Text to Memory (Wrong  "
                    "number of fields %d)\n",
                    count);
                return 1;
            }
            SINUCA3_DEBUG_PRINTF("Memory trace line: %s\n", file_line);

            sub_string = strtok_r(file_line, " ", &tmp_ptr);
            *mem_is_read = strcmp(sub_string, "R") == 0;

            sub_string = strtok_r(NULL, " ", &tmp_ptr);
            *mem_size = strtoull(sub_string, NULL, 10);

            sub_string = strtok_r(NULL, " ", &tmp_ptr);
            *mem_address = strtoull(sub_string, NULL, 10);

            sub_string = strtok_r(NULL, " ", &tmp_ptr);
            valid_memory = true;
        }
    }
    return 0;
}

sinuca::traceReader::FetchResult
sinuca::traceReader::orcsTraceReader::OrCSTraceReader::TraceFetch(
    OpcodePackage *m) {
    OpcodePackage newOpcode;
    uint32_t new_BBL;

    // Fetch new BBL inside the dynamic file.
    if (!this->isInsideBBL) {
        if (this->TraceNextDynamic(&new_BBL)) {
            this->currectBBL = new_BBL;
            this->currectOpcode = 0;
            this->isInsideBBL = true;
        } else {
            SINUCA3_LOG_PRINTF("End of dynamic simulation trace\n");
            return FetchResultEnd;
        }
    }

    // Fetch new INSTRUCTION inside the static file.
    newOpcode = this->binaryDict[this->currectBBL][this->currectOpcode];
    SINUCA3_DEBUG_PRINTF("BBL:%u  OPCODE:%u = %s\n", this->currectBBL,
                         this->currectOpcode, newOpcode.opcodeAssembly);

    this->currectOpcode++;
    if (this->currectOpcode >= this->binaryBBLSize[this->currectBBL]) {
        this->isInsideBBL = false;
        this->currectOpcode = 0;
    }

    // Add SiNUCA information.
    *m = newOpcode;

    // If it is LOAD/STORE then fetch new MEMORY inside the memory file.
    bool mem_is_read;
    if (m->isRead) {
        if (TraceNextMemory(&m->readAddress, &m->readSize, &mem_is_read))
            return FetchResultError;
        if (!mem_is_read) {
            SINUCA3_ERROR_PRINTF("Expecting a read from memory trace\n");
            return FetchResultError;
        }
    }

    if (m->isRead2) {
        if (TraceNextMemory(&m->readAddress, &m->readSize, &mem_is_read))
            return FetchResultError;
        if (!mem_is_read) {
            SINUCA3_ERROR_PRINTF("Expecting a read2 from memory trace\n");
            return FetchResultError;
        }
    }

    if (m->isWrite) {
        if (TraceNextMemory(&m->readAddress, &m->readSize, &mem_is_read))
            return FetchResultError;
        if (!mem_is_read) {
            SINUCA3_ERROR_PRINTF("Expecting a write from memory trace\n");
            return FetchResultError;
        }
    }

    this->fetchInstructions++;
    return FetchResultOk;
}

sinuca::traceReader::FetchResult
sinuca::traceReader::orcsTraceReader::OrCSTraceReader::Fetch(
    sinuca::InstructionPacket *ret) {
    OpcodePackage orcsOpcode;
    FetchResult result = this->TraceFetch(&orcsOpcode);
    if (result != FetchResultOk) return result;

    ret->address = orcsOpcode.opcodeAddress;
    ret->size = static_cast<unsigned char>(orcsOpcode.opcodeSize);
    ret->opcode = NULL;

    return FetchResultOk;
}

void sinuca::traceReader::orcsTraceReader::OrCSTraceReader::PrintStatistics() {
    SINUCA3_LOG_PRINTF(
        "######################################################\n");
    SINUCA3_LOG_PRINTF("trace_reader_t\n");
    SINUCA3_LOG_PRINTF("fetch_instructions:%lu\n", this->fetchInstructions);
}
