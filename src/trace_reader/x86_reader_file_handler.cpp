#include "x86_reader_file_handler.hpp"

#include <cstddef>

#include "../utils/logging.hpp"

sinuca::traceReader::StaticTraceFile::StaticTraceFile(std::string folderPath,
                                                      std::string img) {
    std::string staticPath = FormatPathTidOut(folderPath, "static", img);
    this->fd = open(staticPath.c_str(), O_RDONLY);
    if (fd == -1) {
        SINUCA3_ERROR_PRINTF("Could not open [%s]\n", staticPath.c_str());
        return;
    }
    this->mmapSize = lseek(fd, 0, SEEK_END);
    SINUCA3_DEBUG_PRINTF("Mmap Size [%lu]\n", this->mmapSize);
    this->mmapPtr =
        (char *)mmap(0, this->mmapSize, PROT_READ, MAP_PRIVATE, fd, 0);
    this->mmapOffset = 3 * sizeof(unsigned int);

    // Get number of threads
    this->numThreads = *(unsigned int *)(this->mmapPtr + 0);
    SINUCA3_DEBUG_PRINTF("Number of Threads [%u]\n", this->numThreads);

    // Get total number of Basic Blocks
    this->totalBBLs =
        *(unsigned int *)(this->mmapPtr + sizeof(this->numThreads));
    SINUCA3_DEBUG_PRINTF("Number of BBLs [%u]\n", this->totalBBLs);
    // Get total number of instructions
    this->totalIns =
        *(unsigned int *)(this->mmapPtr + sizeof(this->numThreads) +
                          sizeof(this->totalBBLs));
    SINUCA3_DEBUG_PRINTF("Number of Instructions [%u]\n", this->totalIns);
}

void sinuca::traceReader::StaticTraceFile::ReadNextPackage(
    InstructionInfo *info) {
    DataINS *data = (DataINS *)(this->GetData(sizeof(DataINS)));

    size_t len = strlen(data->name);
    memcpy(info->staticInfo.opcodeAssembly, data->name, len + 1);
    
    info->staticInfo.opcodeSize = data->size;
    info->staticInfo.baseReg = data->baseReg;
    info->staticInfo.indexReg = data->indexReg;
    info->staticInfo.opcodeAddress = data->addr;
    
    this->GetFlagValues(info, data);
    this->GetBranchFields(&info->staticInfo, data);
    this->GetReadRegs(&info->staticInfo, data);
    this->GetWriteRegs(&info->staticInfo, data);
}

unsigned int sinuca::traceReader::StaticTraceFile::GetNewBBlSize() {
    unsigned int *numIns;
    numIns = (unsigned int *)(this->GetData(SIZE_NUM_BBL_INS));
    return *numIns;
}

sinuca::traceReader::StaticTraceFile::~StaticTraceFile() {
    munmap(this->mmapPtr, this->mmapSize);
    close(this->fd);
}

sinuca::traceReader::DynamicTraceFile::DynamicTraceFile(std::string folderPath,
                                                        std::string img,
                                                        THREADID tid)
    : TraceFileReader(FormatPathTidIn(folderPath, "dynamic", img, tid)) {
    this->bufActiveSize =
        (unsigned int)(BUFFER_SIZE / sizeof(BBLID)) * sizeof(BBLID);
    this->RetrieveBuffer();  // First buffer read
}

int sinuca::traceReader::DynamicTraceFile::ReadNextBBl(BBLID *bbl) {
    if (this->eofFound && this->tf.offset == this->eofLocation) {
        return 1;
    }
    if (this->tf.offset >= this->bufActiveSize) {
        this->RetrieveBuffer();
    }
    *bbl = *(BBLID *)(this->GetData(sizeof(BBLID)));

    return 0;
}

sinuca::traceReader::MemoryTraceFile::MemoryTraceFile(std::string folderPath,
                                                      std::string img,
                                                      THREADID tid)
    : TraceFileReader(FormatPathTidIn(folderPath, "memory", img, tid)) {
    this->RetrieveLenBytes((void *)&this->bufActiveSize,
                           sizeof(this->tf.offset));
    this->RetrieveBuffer();
}

int sinuca::traceReader::MemoryTraceFile::ReadNextMemAccess(
    InstructionInfo *insInfo, DynamicInstructionInfo *dynInfo) {
    DataMEM *data;

    if (this->tf.offset >= this->bufActiveSize) {
        this->RetrieveLenBytes((void *)this->bufActiveSize,
                               sizeof(this->tf.offset));
        this->RetrieveBuffer();
    }

    /*
     * In case the instruction performs non standard memory operations
     * with variable number of operands, the number of readings/writings
     * is written directly to the memory trace file
     *
     * Otherwise, it was written in the static trace file.
     */
    if (insInfo->staticInfo.isNonStdMemOp) {
        dynInfo->numReadings =
            *(unsigned short *)(this->GetData(SIZE_NUM_MEM_R_W));
        dynInfo->numWritings =
            *(unsigned short *)(this->GetData(SIZE_NUM_MEM_R_W));
    } else {
        dynInfo->numReadings = insInfo->staticNumReadings;
        dynInfo->numWritings = insInfo->staticNumWritings;
    }

    data = (DataMEM *)(this->GetData(dynInfo->numReadings * sizeof(DataMEM)));
    for (unsigned short readIt = 0; readIt < dynInfo->numReadings; readIt++) {
        dynInfo->readsAddr[readIt] = data->addr;
        dynInfo->readsSize[readIt] = data->size;
        data++;
    }
    data = (DataMEM *)(this->GetData(dynInfo->numWritings * sizeof(DataMEM)));
    for (unsigned short writeIt = 0; writeIt < dynInfo->numWritings;
         writeIt++) {
        dynInfo->writesAddr[writeIt] = data->addr;
        dynInfo->writesSize[writeIt] = data->size;
        data++;
    }

    return 0;
}

bool GetBitBool(unsigned char byte) { return (byte == 1); }

void *sinuca::traceReader::StaticTraceFile::GetData(size_t len) {
    void *ptr = (void *)(this->mmapPtr + this->mmapOffset);
    this->mmapOffset += len;
    return ptr;
}

void sinuca::traceReader::StaticTraceFile::GetFlagValues(InstructionInfo *info,
                                                         struct DataINS *data) {
    info->staticInfo.isPredicated = GetBitBool(data->isPredicated);
    info->staticInfo.isPrefetch = GetBitBool(data->isPrefetch);
    info->staticInfo.isNonStdMemOp = GetBitBool(data->isNonStandardMemOp);

    info->staticNumReadings = 0;
    info->staticNumWritings = 0;
    if (info->staticInfo.isNonStdMemOp == false) {
        if (GetBitBool(data->isRead)) {
            info->staticNumReadings++;
        }
        if (GetBitBool(data->isRead2)) {
            info->staticNumReadings++;
        }
        if (GetBitBool(data->isWrite)) {
            info->staticNumWritings++;
        }
    }
}

void sinuca::traceReader::StaticTraceFile::GetBranchFields(
    sinuca::StaticInstructionInfo *info, struct DataINS *data) {
    info->isIndirect = GetBitBool(data->isIndirectControlFlow);
    info->isControlFlow = GetBitBool(data->isControlFlow);
    info->branchType = data->branchType;
}

void sinuca::traceReader::StaticTraceFile::GetReadRegs(
    sinuca::StaticInstructionInfo *info, struct DataINS *data) {
    info->numReadRegs = data->numReadRegs;
    memcpy(info->readRegs, data->readRegs,
           data->numReadRegs * sizeof(*data->readRegs));
}

void sinuca::traceReader::StaticTraceFile::GetWriteRegs(
    sinuca::StaticInstructionInfo *info, struct DataINS *data) {
    info->numWriteRegs = data->numWriteRegs;
    memcpy(info->writeRegs, data->writeRegs,
           data->numWriteRegs * sizeof(*data->writeRegs));
}