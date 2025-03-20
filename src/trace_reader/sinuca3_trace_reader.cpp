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
 * @file sinuca3_trace_reader.cpp
 */

#include "sinuca3_trace_reader.hpp"

#include <cassert>
#include <cstddef>
#include <cstdio>  // FILE*

#include "../trace_generator/sinuca3_pintool.hpp"
#include "../utils/logging.hpp"
#include "trace_reader.hpp"

inline void increaseOffset(size_t *offset, size_t size) { *offset += size; }

int sinuca::traceReader::sinuca3TraceReader::SinucaTraceReader::OpenTrace(
    const char *traceFileName) {
    //------------------------//
    char fileName[TRACE_LINE_SIZE];

    /* Open static trace */
    fileName[0] = '\0';
    snprintf(fileName, sizeof(fileName), "./trace/static_%s.trace",
             traceFileName);
    this->StaticTraceFile = fopen(fileName, "rb");
    if (this->StaticTraceFile == NULL) {
        SINUCA3_ERROR_PRINTF("Could not open the static file.\n%s\n", fileName);
        return 1;
    }
    SINUCA3_DEBUG_PRINTF("Static File = %s => READY !\n", fileName);

    /* Open dynamic trace */
    fileName[0] = '\0';
    snprintf(fileName, sizeof(fileName), "./trace/dynamic_%s.trace",
             traceFileName);
    this->DynamicTraceFile = fopen(fileName, "rb");
    if (this->DynamicTraceFile == NULL) {
        SINUCA3_ERROR_PRINTF("Could not open the dynamic file.\n%s\n",
                             fileName);
        return 1;
    }
    SINUCA3_DEBUG_PRINTF("Dynamic File = %s => READY !\n", fileName);

    /* Open memory trace */
    fileName[0] = '\0';
    snprintf(fileName, sizeof(fileName), "./trace/memory_%s.trace",
             traceFileName);
    this->MemoryTraceFile = fopen(fileName, "rb");
    if (this->MemoryTraceFile == NULL) {
        SINUCA3_ERROR_PRINTF("Could not open the memory file.\n%s\n", fileName);
        return 1;
    }
    SINUCA3_DEBUG_PRINTF("Memory File = %s => READY !\n", fileName);

    this->isInsideBBL = false;
    this->currentBBL = 0;
    this->binaryTotalBBLs = 0;

    if (this->GetTotalBBLs()) return 1;
    this->binaryBBLsSize = new unsigned short[this->binaryTotalBBLs];
    this->binaryDict = new InstructionPacket *[this->binaryTotalBBLs];
    if (this->GenerateBinaryDict()) return 1;

    return 0;
}

unsigned long
sinuca::traceReader::sinuca3TraceReader::SinucaTraceReader::GetTraceSize() {
    return this->binaryTotalBBLs;
}

unsigned long sinuca::traceReader::sinuca3TraceReader::SinucaTraceReader::
    GetNumberOfFetchedInstructions() {
    //------------------------------//
    return this->fetchInstructions;
}

int sinuca::traceReader::sinuca3TraceReader::SinucaTraceReader::GetTotalBBLs() {
    /* The pintool writes to the beginning of the static
     * trace the total number of basic blocks */
    rewind(this->StaticTraceFile);
    unsigned int *num = &this->binaryTotalBBLs;
    size_t read = fread(num, 1, sizeof(*num), StaticTraceFile);
    if (read <= 0) {
        return 1;
    }

    SINUCA3_DEBUG_PRINTF("NUMBER OF BBLs => %u\n", this->binaryTotalBBLs);
    return 0;
}

/* Helper functions */
void readRegs(const char *buf, size_t *offset, unsigned short *vet,
              unsigned short numRegs);
void readDataINSBytes(char *buf, size_t *offset,
                      sinuca::InstructionPacket *package);
int readBuffer(char *buf, size_t *offset, size_t bufSize, FILE *file);
int readMnemonic(char *str, char *buf, size_t *offset);
int readBufSizeFromFile(size_t *size, FILE *file);

int sinuca::traceReader::sinuca3TraceReader::SinucaTraceReader::
    GenerateBinaryDict() {
    //------------------//
    char buf[BUFFER_SIZE];
    size_t offset, bufSize;
    unsigned short bblSize;
    unsigned int totalIns;
    InstructionPacket *package, *pool, *poolPointer;
    sinuca::traceGenerator::DataINS *data;

    unsigned int bblCounter = 0;
    unsigned short instCounter;

    fseek(this->StaticTraceFile, sizeof(this->binaryTotalBBLs), SEEK_SET);
    size_t read = fread(&totalIns, 1, sizeof(totalIns), this->StaticTraceFile);
    if (read <= 0) {
        SINUCA3_ERROR_PRINTF("INCOMPATIBLE FILE SIZE (BINARY DICT)\n");
        return 1;
    }
    pool = new InstructionPacket[totalIns];
    poolPointer = pool;

    if (readBufSizeFromFile(&bufSize, this->StaticTraceFile)) {
        SINUCA3_ERROR_PRINTF("INCOMPATIBLE FILE SIZE (BINARY DICT)\n");
        return 1;
    }
    if (readBuffer(buf, &offset, bufSize, this->StaticTraceFile)) return 1;

    while (bblCounter < this->binaryTotalBBLs) {
        /* Total of instructions of current basic block */
        bblSize = *(unsigned short *)(buf + offset);
        increaseOffset(&offset, sizeof(bblSize));
        SINUCA3_DEBUG_PRINTF("BBL SIZE => %d\n", bblSize);

        this->binaryBBLsSize[bblCounter] = bblSize;
        this->binaryDict[bblCounter] = poolPointer;
        poolPointer += bblSize;

        instCounter = 0;
        while (instCounter < bblSize) {
            if (offset == bufSize) {
                if (readBufSizeFromFile(&bufSize, this->StaticTraceFile)) {
                    SINUCA3_ERROR_PRINTF(
                        "INCOMPATIBLE FILE SIZE (BINARY DICT)\n");
                    return 1;
                }

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

            SINUCA3_DEBUG_PRINTF(
                "INS MNEMONIC => %s\n"
                "INS ADDR => %p\n"
                "INS SIZE => %d\n"
                "INS NUM R REGS => %d\n"
                "INS NUM W REGS => %d\n",
                package->opcodeAssembly, (void *)package->opcodeAddress,
                package->opcodeSize, package->numReadRegs,
                package->numWriteRegs);

            instCounter++;
        }

        bblCounter++;
    }

    return 0;
}

int sinuca::traceReader::sinuca3TraceReader::SinucaTraceReader::
    TraceNextDynamic(unsigned int *nextBbl) {
    //-------------------------------------//
    static size_t bufSize = 0, offset = 0;
    static char buf[BUFFER_SIZE];

    if (offset == bufSize) {
        if (readBufSizeFromFile(&bufSize, this->DynamicTraceFile)) {
            return 1;
        }
        if (readBuffer(buf, &offset, bufSize, this->DynamicTraceFile)) {
            return 1;
        }
    }
    *nextBbl = *(unsigned int *)(buf + offset);
    increaseOffset(&offset, sizeof(*nextBbl));

    return 0;
}

int sinuca::traceReader::sinuca3TraceReader::SinucaTraceReader::TraceNextMemory(
    InstructionPacket *package) {
    //-------------------------//
    static size_t offset = 0, bufSize = 0;
    static char buf[BUFFER_SIZE];
    unsigned short numMemOps;
    sinuca::traceGenerator::DataMEM *data;

    if (offset == bufSize) {
        if (readBufSizeFromFile(&bufSize, this->MemoryTraceFile)) {
            return 1;
        }
        if (readBuffer(buf, &offset, bufSize, this->MemoryTraceFile)) {
            return 1;
        }
    }

    /* In case the instruction performs non standard memory operations
     * with variable number of operands, the number of readings/writings
     * is written directly to the memory trace file */
    if (package->isNonStdMemOp) {
        package->numReadings = *(unsigned short *)(buf + offset);
        increaseOffset(&offset, sizeof(unsigned short));
        package->numWritings = *(unsigned short *)(buf + offset);
        increaseOffset(&offset, sizeof(unsigned short));
    }

    data = (sinuca::traceGenerator::DataMEM *)(buf + offset);
    for (unsigned short readIt; readIt < package->numReadings; readIt++) {
        package->readsAddr[readIt] = data->addr;
        package->readsSize[readIt] = data->size;
        data++;
    }
    for (unsigned short writeIt; writeIt < package->numWritings; writeIt++) {
        package->writesAddr[writeIt] = data->addr;
        package->writesSize[writeIt] = data->size;
        data++;
    }
    increaseOffset(&offset, (size_t)((char *)data - buf) - offset);

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
    }

    *package = &this->binaryDict[this->currentBBL][this->currentOpcode];
    this->TraceNextMemory(*package);

    this->currentOpcode++;
    if (this->currentOpcode >= this->binaryBBLsSize[this->currentBBL]) {
        this->isInsideBBL = false;
    }

    this->fetchInstructions++;

    return FetchResultOk;
}

sinuca::traceReader::FetchResult
sinuca::traceReader::sinuca3TraceReader::SinucaTraceReader::Fetch(
    const InstructionPacket **ret) {
    //----------------------------//
    InstructionPacket *ptr;
    FetchResult result = this->TraceFetch(&ptr);
    if (result != FetchResultOk) return result;

    *ret = const_cast<InstructionPacket *>(ptr);

    SINUCA3_DEBUG_PRINTF("Fetched: %s\n", (*ret)->opcodeAssembly);

    return FetchResultOk;
}

void sinuca::traceReader::sinuca3TraceReader::SinucaTraceReader::
    PrintStatistics() {
    //---------------//
    SINUCA3_LOG_PRINTF("###########################\n");
    SINUCA3_LOG_PRINTF("Sinuca3 Trace Reader\n");
    SINUCA3_LOG_PRINTF("Fetch Instructions:%lu\n", this->fetchInstructions);
    SINUCA3_LOG_PRINTF("###########################\n");
}

void readRegs(const char *buf, size_t *offset, unsigned short *vet,
              unsigned short numRegs) {
    //-------------------------------//
    memcpy(vet, buf + *offset, sizeof(*vet) * numRegs);
    increaseOffset(offset, sizeof(*vet) * numRegs);
}

int readBuffer(char *buf, size_t *offset, size_t bufSize, FILE *file) {
    if (bufSize > BUFFER_SIZE) {
        return 1;
    }
    fread(buf, 1, bufSize, file);
    *offset = 0;

    return 0;
}

void readDataINSBytes(char *buf, size_t *offset,
                      sinuca::InstructionPacket *package) {
    //---------------------------------------------------//
    sinuca::traceGenerator::DataINS *data;

    data = (sinuca::traceGenerator::DataINS *)(buf + *offset);
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
        return 1;
    }

    return 0;
}
