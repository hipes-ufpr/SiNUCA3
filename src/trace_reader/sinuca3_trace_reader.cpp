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

#include "../trace_generator/sinuca3_pintool.hpp"
#include "../utils/logging.hpp"
#include "trace_reader.hpp"

int sinuca::traceReader::sinuca3TraceReader::SinucaTraceReader::OpenTrace(
    const char *traceFileName) {
    //------------------------//
    char fileName[TRACE_LINE_SIZE];

    // Open the Static Trace file
    fileName[0] = '\0';
    snprintf(fileName, sizeof(fileName), "../../trace/static_%s.trace",
             traceFileName);
    this->StaticTraceFile = fopen(fileName, "rb");
    if (this->StaticTraceFile == NULL) {
        SINUCA3_ERROR_PRINTF("Could not open the static file.\n%s\n", fileName);
        return 1;
    }
    SINUCA3_DEBUG_PRINTF("Static File = %s => READY !\n", fileName);

    // Open the Dynamic Trace File
    fileName[0] = '\0';
    snprintf(fileName, sizeof(fileName), "../../trace/dynamic_%s.trace",
             traceFileName);
    this->DynamicTraceFile = fopen(fileName, "rb");
    if (this->DynamicTraceFile == NULL) {
        SINUCA3_ERROR_PRINTF("Could not open the dynamic file.\n%s\n",
                             fileName);
        return 1;
    }
    SINUCA3_DEBUG_PRINTF("Dynamic File = %s => READY !\n", fileName);

    // Open the Memory Trace File
    fileName[0] = '\0';
    snprintf(fileName, sizeof(fileName), "../../trace/memory_%s.trace",
             traceFileName);
    this->MemoryTraceFile = fopen(fileName, "rb");
    if (this->MemoryTraceFile == NULL) {
        SINUCA3_ERROR_PRINTF("Could not open the memory file.\n%s\n", fileName);
        return 1;
    }
    SINUCA3_DEBUG_PRINTF("Memory File = %s => READY !\n", fileName);

    // Set the trace reader controls
    this->isInsideBBL = false;
    this->currentBBL = 0;
    this->binaryTotalBBLs = 0;

    // Obtain the number of BBLs
    if (this->GetTotalBBLs()) return 1;

    // Initialize the binary dictionary containing all BBLs and their instructions.
    this->binaryBBLsSize = new unsigned short[this->binaryTotalBBLs];
    this->binaryDict = new InstructionPacket *[this->binaryTotalBBLs];
    if (this->GenerateBinaryDict()) return 1;

    return 0;
}

int sinuca::traceReader::sinuca3TraceReader::SinucaTraceReader::GetTotalBBLs() {
    // Go to the Begin of the File
    rewind(this->StaticTraceFile);

    size_t read = fread(&this->binaryTotalBBLs , 1, sizeof(this->binaryTotalBBLs), StaticTraceFile);
    if (read <= 0) {
        return 1;
    }
    SINUCA3_DEBUG_PRINTF("NUMBER OF BBLs => %u\n", this->binaryTotalBBLs);

    return 0;
}

void readRegs(const char *buf, size_t *offset, unsigned short *vet,
              unsigned short numRegs) {
    //---------------------//
    memcpy(vet, buf + *offset, sizeof(*vet) * numRegs);
    increaseOffset(offset, sizeof(*vet) * numRegs);
}

int readBuffer(char *buf, size_t *offset, size_t bufSize, FILE *file) {
    if (bufSize > BUFFER_SIZE) {
        SINUCA3_ERROR_PRINTF("INCOMPATIBLE BUFFER SIZE (BINARY DICT)");
        return 1;
    }
    fread(buf, 1, bufSize, file);
    *offset = 0;

    return 0;
}

void readDataINSBytes(char *buf, size_t *offset,
                      sinuca::traceReader::InstructionPacket *package) {
    //----------------------------------------------//
    DataINS *data;

    data = (DataINS *)(buf + *offset);
    package->opcodeAddress = data->addr;
    package->opcodeSize = data->size;
    package->baseReg = data->baseReg;
    package->indexReg = data->indexReg;
    package->numReadRegs = data->numReadRegs;
    package->numWriteRegs = data->numWriteRegs;

    package->isPrefetch = ((data->booleanValues & (1 << 0)) != 0);
    package->isPredicated = ((data->booleanValues & (1 << 1)) != 0);
    package->isControlFlow = ((data->booleanValues & (1 << 2)) != 0);
    package->isNonStdMemOp = ((data->booleanValues & (1 << 4)) != 0);
    if (package->isControlFlow) {
        package->isIndirect = ((data->booleanValues & (1 << 3)) != 0);
    }
    if (!package->isNonStdMemOp) {
        if ((data->booleanValues & (1 << 5)) != 0) {
            package->numReadings++;
        }
        if ((data->booleanValues & (1 << 6)) != 0) {
            package->numReadings++;
        }
        if ((data->booleanValues & (1 << 7)) != 0) {
            package->numWritings++;
        }
    }
    increaseOffset(offset, sizeof(*data));

    SINUCA3_DEBUG_PRINTF("INS ADDR => %p ", (void *)package->opcodeAddress);
    SINUCA3_DEBUG_PRINTF("INS SIZE => %d ", package->opcodeSize);
    SINUCA3_DEBUG_PRINTF("INS NUM R REGS => %d ", package->numReadRegs);
    SINUCA3_DEBUG_PRINTF("INS NUM W REGS => %d ", package->numWriteRegs);
}

int readMnemonic(char *str, char *buf, size_t *offset) {
    size_t strSize = strlen(buf + *offset) + 1;
    if (strSize > TRACE_LINE_SIZE) {
        SINUCA3_ERROR_PRINTF("INCOMPATIBLE STRING SIZE (BINARY DICT)\n");
        return 1;
    }
    memcpy(str, buf + *offset, strSize);
    increaseOffset(offset, strSize);

    return 0;
}

int readBufSizeFromFile(size_t *size, FILE *file) {
    size_t read = fread(size, 1, sizeof(*size), file);
    if (read <= 0) {
        SINUCA3_ERROR_PRINTF("INCOMPATIBLE FILE SIZE (BINARY DICT)\n");
        return 1;
    }

    return 0;
}

int sinuca::traceReader::sinuca3TraceReader::SinucaTraceReader::
    GenerateBinaryDict() {
    //------------------//
    char buf[BUFFER_SIZE];
    size_t offset, bufSize;
    unsigned short bblSize;
    InstructionPacket *package;
    DataINS *data;

    unsigned int bblCounter = 0;
    unsigned short instCounter;

    fseek(this->StaticTraceFile, sizeof(this->binaryTotalBBLs), SEEK_SET);
    if (readBufSizeFromFile(&bufSize, this->StaticTraceFile)) {
        return 1;
    }
    if (readBuffer(buf, &offset, bufSize, this->StaticTraceFile)) {
        return 1;
    }

    while (bblCounter < this->binaryTotalBBLs) {
        bblSize = *(unsigned short *)(buf + offset);
        increaseOffset(&offset, sizeof(bblSize));
        SINUCA3_DEBUG_PRINTF("BBL SIZE => %d\n", bblSize);

        this->binaryBBLsSize[bblCounter] = bblSize;
        this->binaryDict[bblCounter] = new InstructionPacket[bblSize];

        instCounter = 0;
        while (instCounter < bblSize) {
            if (offset == bufSize) {
                if (readBufSizeFromFile(&bufSize, this->StaticTraceFile))
                    return 1;

                if (readBuffer(buf, &offset, bufSize, this->StaticTraceFile))
                    return 1;
            }

            package = &this->binaryDict[bblCounter][instCounter];
            readDataINSBytes(buf, &offset, package);
            readRegs(buf, &offset, package->readRegs, package->numReadRegs);
            readRegs(buf, &offset, package->writeRegs, package->numWriteRegs);
            readMnemonic(package->opcodeAssembly, buf, &offset);
            if (package->isControlFlow) {
                package->branchType = *(Branch *)(buf + offset);
                increaseOffset(&offset, sizeof(package->branchType));
            }

            SINUCA3_DEBUG_PRINTF("INS MNEMONIC => %s\n",
                                 package->opcodeAssembly);
            SINUCA3_DEBUG_PRINTF("BBL => %d ", bblCounter);
            SINUCA3_DEBUG_PRINTF("INS => %d\n", instCounter);

            instCounter++;
        }

        bblCounter++;
    }

    SINUCA3_DEBUG_PRINTF("READ BYTES => %lu BUF SIZE => %lu\n", offset,
                         bufSize);

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

// TODO
int sinuca::traceReader::sinuca3TraceReader::SinucaTraceReader::
    TraceNextDynamic(unsigned int *nextBbl) {
    //--------------------//
    static size_t bufSize, readBuf = 0;
    static char buf[BUFFER_SIZE];
    size_t read = fread(nextBbl, 1, sizeof(*nextBbl), this->DynamicTraceFile);
    if (read <= 0) {
        SINUCA3_ERROR_PRINTF("INCOMPATIBLE SIZE OF DYNAMIC TRACE FILE\n");
        return 1;
    }

    return 0;
}

/**
 * @details
 * clang-format off
 *
 * clang-format on
 */
int sinuca::traceReader::sinuca3TraceReader::SinucaTraceReader::TraceNextMemory(
    InstructionPacket *package) {
    //-------------------------//
    unsigned short numMemOps, readIt, writeIt;
    DataMEM data;
    memOpType type;

    readIt = writeIt = 0;
    if (package->isNonStdMemOp) {
        fread(&numMemOps, sizeof(numMemOps), 1, this->MemoryTraceFile);
        while (numMemOps--) {
            fread(&data, sizeof(data), 1, this->MemoryTraceFile);
            fread(&type, sizeof(type), 1, this->MemoryTraceFile);
            switch (type) {
                case LOAD:
                    package->readsAddr[readIt] = data.addr;
                    package->readsSize[readIt] = data.size;
                    readIt++;
                    break;
                case STORE:
                    package->writeRegs[writeIt] = data.addr;
                    package->writesSize[writeIt] = data.size;
                    writeIt++;
                    break;
                default:
                    break;
            }
        }
        package->numReadings = readIt;
        package->numWritings = writeIt;
    } else {
        for (; readIt < package->numReadings; readIt++) {
            fread(&data, sizeof(data), 1, this->MemoryTraceFile);
            package->readsAddr[readIt] = data.addr;
            package->readsSize[readIt] = data.size;
            readIt++;
        }
        for (; writeIt < package->numWritings; writeIt++) {
            fread(&data, sizeof(data), 1, this->MemoryTraceFile);
            package->writeRegs[writeIt] = data.addr;
            package->writesSize[writeIt] = data.size;
            writeIt++;
        }
    }

    return 0;
}

sinuca::traceReader::FetchResult
sinuca::traceReader::sinuca3TraceReader::SinucaTraceReader::TraceFetch(
    InstructionPacket **package) {
    //--------------------------//
    if (!this->isInsideBBL) {
        if (this->TraceNextDynamic(&this->currentBBL)) {
            return FetchResultEnd;
        }
        this->isInsideBBL = true;
        this->currentOpcode = 0;

        SINUCA3_DEBUG_PRINTF("CURRENT BBL => %d\n", this->currentBBL);
    }

    *package = &this->binaryDict[this->currentBBL][this->currentOpcode];
    this->currentOpcode++;
    if (this->currentOpcode >= this->binaryBBLsSize[this->currentBBL]) {
        this->isInsideBBL = false;
    }
    this->TraceNextMemory(*package);
    this->fetchInstructions++;

    return FetchResultOk;
}

sinuca::traceReader::FetchResult
sinuca::traceReader::sinuca3TraceReader::SinucaTraceReader::Fetch(
    InstructionPacket **ret) {
    //----------------------//
    FetchResult result = this->TraceFetch(ret);
    if (result != FetchResultOk) return result;

    return FetchResultOk;
}

void sinuca::traceReader::sinuca3TraceReader::SinucaTraceReader::
    PrintStatistics() {
    SINUCA3_LOG_PRINTF(
        "######################################################\n");
    SINUCA3_LOG_PRINTF("trace_reader_t\n");
    SINUCA3_LOG_PRINTF("fetch_instructions:%lu\n", this->fetchInstructions);
}

int main() {
    namespace Sinuca3Reader = sinuca::traceReader::sinuca3TraceReader;
    namespace Reader = sinuca::traceReader;

    Reader::TraceReader *reader = new (Sinuca3Reader::SinucaTraceReader);

    sinuca::traceReader::InstructionPacket *package;
    reader->OpenTrace("teste");
    while (reader->Fetch(&package) != sinuca::traceReader::FetchResultEnd) {
        SINUCA3_DEBUG_PRINTF("INS MNEMONIC => %s ", package->opcodeAssembly);
        SINUCA3_DEBUG_PRINTF("INS NUM MEM L => %d ", package->numReadings);
        SINUCA3_DEBUG_PRINTF("INS NUM MEM W => %d ", package->numWritings);
    }
}
