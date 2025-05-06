#include "reader_file_handler.hpp"

#include <cstddef>

#include "../utils/logging.hpp"

const char *FormatThreadSuffix(THREADID tid) {
    static char sufixBuffer[32];
    snprintf(sufixBuffer, sizeof(sufixBuffer), "_tid%d", tid);
    return sufixBuffer;
}

sinuca::traceReader::StaticTraceFile::StaticTraceFile(const char *staticPath) {
    this->fd = open(staticPath, O_RDONLY);
    if (fd == -1) {
        SINUCA3_ERROR_PRINTF("Could not open => %s\n", staticPath);
        return;
    }
    this->mmapSize = lseek(fd, 0, SEEK_END);
    this->mmapPtr =
        (char *)mmap(0, this->mmapSize, PROT_READ, MAP_PRIVATE, fd, 0);
    this->mmapOffset = 3 * sizeof(unsigned int);

    this->numThreads = *(unsigned int *)(this->mmapPtr + 0);
    SINUCA3_DEBUG_PRINTF("Number of Threads => %u\n", this->numThreads);
    this->totalBBLs = *(unsigned int *)(this->mmapPtr + 4);
    SINUCA3_DEBUG_PRINTF("Number of BBLs => %u\n", this->totalBBLs);
    this->totalIns = *(unsigned int *)(this->mmapPtr + 8);
    SINUCA3_DEBUG_PRINTF("Number of Instructions => %u\n", this->totalIns);
}

unsigned int sinuca::traceReader::StaticTraceFile::GetNumThreads() {
    return this->numThreads;
}

unsigned int sinuca::traceReader::StaticTraceFile::GetTotalBBLs() {
    return this->totalBBLs;
}

unsigned int sinuca::traceReader::StaticTraceFile::GetTotalIns() {
    return this->totalIns;
}

bool GetBitBool(unsigned char byte, int pos) {
    return (byte & (1 << pos)) != 0;
}

void sinuca::traceReader::StaticTraceFile::ReadNextPackage(
    InstructionInfo *info) {
    DataINS *data = (DataINS *)(this->mmapPtr + this->mmapOffset);
    this->mmapOffset += sizeof(struct DataINS);

    sinuca::StaticInstructionInfo *staticInfo = &info->staticInfo;
    strncpy(staticInfo->opcodeAssembly, data->name,
            MAX_INSTRUCTION_NAME_LENGTH);

    staticInfo->opcodeSize = data->size;
    staticInfo->baseReg = data->baseReg;
    staticInfo->indexReg = data->indexReg;
    staticInfo->opcodeAddress = data->addr;
    staticInfo->branchType = data->branchType;
    staticInfo->numReadRegs = data->numReadRegs;
    staticInfo->numWriteRegs = data->numWriteRegs;

    staticInfo->isPredicated = GetBitBool(data->booleanValues, IS_PREDICATED);
    staticInfo->isPrefetch = GetBitBool(data->booleanValues, IS_PREFETCH);
    staticInfo->isIndirect =
        GetBitBool(data->booleanValues, IS_INDIRECT_CONTROL_FLOW);
    staticInfo->isNonStdMemOp =
        GetBitBool(data->booleanValues, IS_NON_STANDARD_MEM_OP);
    staticInfo->isControlFlow =
        GetBitBool(data->booleanValues, IS_CONTROL_FLOW);

    info->staticNumReadings = 0;
    info->staticNumWritings = 0;
    if (staticInfo->isNonStdMemOp == false) {
        if (GetBitBool(data->booleanValues, IS_READ)) {
            info->staticNumReadings++;
        }
        if (GetBitBool(data->booleanValues, IS_READ2)) {
            info->staticNumReadings++;
        }
        if (GetBitBool(data->booleanValues, IS_WRITE)) {
            info->staticNumWritings++;
        }
    }

    memcpy(staticInfo->readRegs, data->readRegs,
           data->numReadRegs * sizeof(*data->readRegs));
    memcpy(staticInfo->writeRegs, data->writeRegs,
           data->numWriteRegs * sizeof(*data->writeRegs));

    SINUCA3_DEBUG_PRINTF("INS NAME => %s\n", staticInfo->opcodeAssembly);
}

unsigned int sinuca::traceReader::StaticTraceFile::GetNewBBlSize() {
    unsigned int numIns;
    numIns = *(unsigned int *)(this->mmapPtr + this->mmapOffset);
    this->mmapOffset += sizeof(numIns);

    return numIns;
}

sinuca::traceReader::StaticTraceFile::~StaticTraceFile() {
    munmap(this->mmapPtr, this->mmapSize);
    close(this->fd);
}

sinuca::traceReader::DynamicTraceFile::DynamicTraceFile(const char *imageName,
                                                        THREADID tid,
                                                        const char *path)
    : TraceFileReader("dynamic_", imageName, FormatThreadSuffix(tid), path) {
    this->bufSize = (unsigned int)(BUFFER_SIZE / sizeof(BBlId)) * sizeof(BBlId);
    if (this->ReadBuffer()) {  // first buffer read
        SINUCA3_DEBUG_PRINTF("Could not fill Dynamic Buffer\n");
    }
    this->eofLocation = 0;
    this->offset = 0;
}

int sinuca::traceReader::DynamicTraceFile::ReadNextBBl(BBlId *bbl) {
    if (this->eofLocation > 0 && this->offset == this->eofLocation) {
        return 1;
    }
    if (this->offset >= this->bufSize) {
        if (this->ReadBuffer()) {
            SINUCA3_DEBUG_PRINTF("Could not fill Dynamic Buffer\n");
            return 1;
        }
    }

    *bbl = *(BBlId *)(this->buf + this->offset);
    this->offset += sizeof(BBlId);

    return 0;
}

sinuca::traceReader::MemoryTraceFile::MemoryTraceFile(const char *imageName,
                                                      THREADID tid,
                                                      const char *path)
    : TraceFileReader("memory_", imageName, FormatThreadSuffix(tid), path) {
    if (this->ReadBufSizeFromFile()) {
        SINUCA3_DEBUG_PRINTF("Invalid from Mem Trace File\n");
        return;
    }
    if (this->ReadBuffer()) {  // first buffer read
        SINUCA3_DEBUG_PRINTF("Could not fill Memory Buffer\n");
    }
    this->eofLocation = 0;
    this->offset = 0;
}

int sinuca::traceReader::MemoryTraceFile::ReadNextMemAccess(
    InstructionInfo *insInfo, DynamicInstructionInfo *dynInfo) {
    DataMEM *data;

    if (this->offset >= this->bufSize) {
        if (this->ReadBufSizeFromFile()) {
            SINUCA3_DEBUG_PRINTF("Invalid buffer size from Mem Trace File\n");
            return 1;
        }
        if (this->ReadBuffer()) {
            SINUCA3_DEBUG_PRINTF("Could not fill Memory Buffer\n");
            return 1;
        }
    }

    /* In case the instruction performs non standard memory operations
     * with variable number of operands, the number of readings/writings
     * is written directly to the memory trace file
     *
     * Otherwise, it was written in the static trace file. */
    if (insInfo->staticInfo.isNonStdMemOp) {
        dynInfo->numReadings = *(unsigned short *)(this->buf + this->offset);
        this->offset += sizeof(unsigned short);
        dynInfo->numWritings = *(unsigned short *)(this->buf + this->offset);
        this->offset += sizeof(unsigned short);
    } else {
        dynInfo->numReadings = insInfo->staticNumReadings;
        dynInfo->numWritings = insInfo->staticNumWritings;
    }

    data = (DataMEM *)(this->buf + this->offset);
    for (unsigned short readIt = 0; readIt < dynInfo->numReadings; readIt++) {
        dynInfo->readsAddr[readIt] = data->addr;
        dynInfo->readsSize[readIt] = data->size;
        data++;
    }
    for (unsigned short writeIt = 0; writeIt < dynInfo->numWritings;
         writeIt++) {
        dynInfo->writesAddr[writeIt] = data->addr;
        dynInfo->writesSize[writeIt] = data->size;
        data++;
    }

    this->offset = (size_t)((unsigned char *)data - this->buf);

    return 0;
}