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

#include "sinuca3_trace_reader.hpp"
#include <unistd.h>

#include <cassert>
#include <cstddef>
#include <cstdio>   // NULL, SEEK_SET, snprintf
#include <cstring>  // strcmp, strcpy

#include "../utils/logging.hpp"
#include "trace_reader.hpp"

#include <iostream> // TMP

int sinuca::traceReader::sinuca3TraceReader::SinucaTraceReader::OpenTrace(
    const char *traceFileName) {
    char fileName[TRACE_LINE_SIZE];

    // Open the Static Trace file.
    fileName[0] = '\0';
    snprintf(fileName, sizeof(fileName), "../trace_generator/static_%s.trace", traceFileName);
    this->StaticTraceFile = fopen(fileName, "rb");
    if (this->StaticTraceFile == NULL) {
        SINUCA3_ERROR_PRINTF("Could not open the static file.\n%s\n", 
                             fileName);
        return 1;
    }
    SINUCA3_DEBUG_PRINTF("Static File = %s => READY !\n", fileName);

    // Open the Dynamic Trace File.
    fileName[0] = '\0';
    snprintf(fileName, sizeof(fileName), "../trace_generator/dynamic_%s.trace", traceFileName);
    this->DynamicTraceFile = fopen(fileName, "rb");
    if (this->DynamicTraceFile == NULL) {
        SINUCA3_ERROR_PRINTF("Could not open the dynamic file.\n%s\n",
                             fileName);
        return 1;
    }
    SINUCA3_DEBUG_PRINTF("Dynamic File = %s => READY !\n", fileName);

    // Open the Memory Trace File.
    fileName[0] = '\0';
    snprintf(fileName, sizeof(fileName), "../trace_generator/memory_%s.trace", traceFileName);
    this->MemoryTraceFile = fopen(fileName, "rb");
    if (this->MemoryTraceFile == NULL) {
        SINUCA3_ERROR_PRINTF("Could not open the memory file.\n%s\n", 
                             fileName);
        return 1;
    }
    SINUCA3_DEBUG_PRINTF("Memory File = %s => READY !\n", fileName);

    // Set the trace reader controls.
    this->isInsideBBL = false;
    this->currentBBL = 0;
    this->currentOpcode = 0;
    this->binaryTotalBBLs = 0;

    // Obtain the number of BBLs.
    if (this->GetTotalBBLs()) return 1;

    this->binaryBBLsSize = new uint32_t[this->binaryTotalBBLs];
    memset(this->binaryBBLsSize, 0,
           this->binaryTotalBBLs * sizeof(*this->binaryBBLsSize));

    // Create the opcode for each BBL.
    this->binaryDict = new OpcodePackage* [this->binaryTotalBBLs];
    if (this->GenerateBinaryDict()) return 1;

    return 0;
}

int sinuca::traceReader::sinuca3TraceReader::SinucaTraceReader::GetTotalBBLs() {
    char file_line[TRACE_LINE_SIZE] = "";
    bool file_eof = false;
    uint32_t bbl = 0;
    unsigned int *count = &this->binaryTotalBBLs;

    // Go to the Begin of the File
    rewind(this->StaticTraceFile);
    size_t read = fread((void*)count, 1, sizeof(*count), StaticTraceFile);
    if (read == 0) {return 1;}
    std::cout << *count << std::endl;

    return 0;
}

#define BUFFER_SIZE 1 << 21

int sinuca::traceReader::sinuca3TraceReader::SinucaTraceReader::
    GenerateBinaryDict() {
    //------------------//
    unsigned int *count = &this->binaryTotalBBLs;
    char line[BUFFER_SIZE], byte;
    size_t read, strSize, cpySize, bufSize, bufPointer;
    int numInstBbl, savedNumInst, bbl = 0;
    OpcodePackage* package;
    
    fseek(this->StaticTraceFile, sizeof(*count), SEEK_SET);
    read = fread(&bufSize, 1, sizeof(bufSize), this->StaticTraceFile);
    while(read > 0) {
        SINUCA3_DEBUG_PRINTF("BUFFER SIZE => %lu\n", bufSize);
        fread(line, 1, bufSize, this->StaticTraceFile);
        bufPointer = 0;
        do {
            memcpy(&numInstBbl, line+bufPointer, sizeof(numInstBbl));
            SINUCA3_DEBUG_PRINTF("BBL SIZE => %d\n", numInstBbl);
            this->binaryDict[bbl] = new OpcodePackage[numInstBbl];
            savedNumInst = numInstBbl;

            while (numInstBbl > 0) {
                package = &this->binaryDict[bbl][savedNumInst-numInstBbl];
                
                strSize = snprintf(package->opcodeAssembly, 512, "%s", line+bufPointer);
                bufPointer += strSize;
                
                cpySize = sizeof(package->opcodeAddress)+
                    sizeof(package->opcodeSize)+
                    sizeof(package->baseReg)+
                    sizeof(package->indexReg);
                memcpy(&package->opcodeAddress, line+bufPointer, cpySize);
                bufPointer += cpySize;
                
                memcpy(&byte, line+bufPointer, 1);
                if ((byte & 1) == 1) {package->isPrefetch = true;}
                if ((byte & (1 << 1)) == (1 << 1)) {package->isPredicated = true;}
                if ((byte & (1 << 2)) == (1 << 2)) {
                    package->isControlFlow = true;
                    if ((byte & (1 << 3)) == (1 << 3)) {package->isIndirect = true;}
                    memcpy(&package->branchType, line+bufPointer, 1);
                }

                numInstBbl--;
            }

            bbl++;
        } while (bufPointer < bufSize);

        read = fread(&bufSize, 1, sizeof(bufSize), this->StaticTraceFile);
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
int sinuca::traceReader::sinuca3TraceReader::SinucaTraceReader::TraceStringToOpcode(
    char *input_string, OpcodePackage *opcode) {
    
    return 0;
}

// =============================================================================
int sinuca::traceReader::sinuca3TraceReader::SinucaTraceReader::TraceNextDynamic(
    uint32_t *next_bbl) {

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
int sinuca::traceReader::sinuca3TraceReader::SinucaTraceReader::TraceNextMemory(
    uint64_t *mem_address, uint32_t *mem_size, bool *mem_is_read) {
    
    return 0;
}

// sinuca::traceReader::FetchResult
// sinuca::traceReader::sinuca3TraceReader::SinucaTraceReader::TraceFetch(
//     OpcodePackage *m) {
//     OpcodePackage newOpcode;
//     uint32_t new_BBL;

//     // Fetch new BBL inside the dynamic file.
//     if (!this->isInsideBBL) {
//         if (this->TraceNextDynamic(&new_BBL)) {
//             this->currectBBL = new_BBL;
//             this->currectOpcode = 0;
//             this->isInsideBBL = true;
//         } else {
//             SINUCA3_LOG_PRINTF("End of dynamic simulation trace\n");
//             return FetchResultEnd;
//         }
//     }

//     // Fetch new INSTRUCTION inside the static file.
//     newOpcode = this->binaryDict[this->currectBBL][this->currectOpcode];
//     SINUCA3_DEBUG_PRINTF("BBL:%u  OPCODE:%u = %s\n", this->currectBBL,
//                          this->currectOpcode, newOpcode.opcodeAssembly);

//     this->currectOpcode++;
//     if (this->currectOpcode >= this->binaryBBLSize[this->currectBBL]) {
//         this->isInsideBBL = false;
//         this->currectOpcode = 0;
//     }

//     // Add SiNUCA information.
//     *m = newOpcode;

//     // If it is LOAD/STORE then fetch new MEMORY inside the memory file.
//     bool mem_is_read;
//     if (m->isRead) {
//         if (TraceNextMemory(&m->readAddress, &m->readSize, &mem_is_read))
//             return FetchResultError;
//         if (!mem_is_read) {
//             SINUCA3_ERROR_PRINTF("Expecting a read from memory trace\n");
//             return FetchResultError;
//         }
//     }

//     if (m->isRead2) {
//         if (TraceNextMemory(&m->readAddress, &m->readSize, &mem_is_read))
//             return FetchResultError;
//         if (!mem_is_read) {
//             SINUCA3_ERROR_PRINTF("Expecting a read2 from memory trace\n");
//             return FetchResultError;
//         }
//     }

//     if (m->isWrite) {
//         if (TraceNextMemory(&m->readAddress, &m->readSize, &mem_is_read))
//             return FetchResultError;
//         if (!mem_is_read) {
//             SINUCA3_ERROR_PRINTF("Expecting a write from memory trace\n");
//             return FetchResultError;
//         }
//     }

//     this->fetchInstructions++;
//     return FetchResultOk;
// }

// sinuca::traceReader::FetchResult
// sinuca::traceReader::sinuca3TraceReader::SinucaTraceReader::Fetch(
//     sinuca::InstructionPacket *ret) {
//     OpcodePackage orcsOpcode;
//     FetchResult result = this->TraceFetch(&orcsOpcode);
//     if (result != FetchResultOk) return result;

//     ret->address = orcsOpcode.opcodeAddress;
//     ret->size = static_cast<unsigned char>(orcsOpcode.opcodeSize);
//     ret->opcode = NULL;

//     return FetchResultOk;
// }

// void sinuca::traceReader::sinuca3TraceReader::SinucaTraceReader::PrintStatistics() {
//     SINUCA3_LOG_PRINTF(
//         "######################################################\n");
//     SINUCA3_LOG_PRINTF("trace_reader_t\n");
//     SINUCA3_LOG_PRINTF("fetch_instructions:%lu\n", this->fetchInstructions);
// }

int main() {
    sinuca::traceReader::TraceReader* reader = new (sinuca::traceReader::sinuca3TraceReader::SinucaTraceReader);

    reader->OpenTrace("teste");
}
