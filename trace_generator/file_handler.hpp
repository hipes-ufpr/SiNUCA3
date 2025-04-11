#ifndef FILEHANDLER_HPP_
#define FILEHANDLER_HPP_

#include <cstdio>
#include <cstring>
#include <vector>

#include "../src/utils/logging.hpp"

#define BUFFER_SIZE 1 << 20

// Used in alignas to avoid false sharing
#define CACHE_LINE_SIZE 64

#define MAX_IMAGE_NAME_SIZE 64

namespace traceGenerator {

const char traceFolderPath[] = "../trace/";

enum TraceType {
    TRACE_STATIC,
    TRACE_DYNAMIC,
    TRACE_MEMORY
};

struct __attribute__((aligned(CACHE_LINE_SIZE))) Buffer {
    char store[BUFFER_SIZE];
    size_t numUsedBytes;
    size_t minSpacePerOperation;

    // No operation with the buffer locks the lock.
    // But it is allocated here in case it is needed.
    PIN_LOCK bufferPinLock;

    Buffer() {
        numUsedBytes = 0;
        minSpacePerOperation = 0;
        PIN_InitLock(&(this->bufferPinLock));
    }

    inline void LoadBufToFile(FILE* file, bool writeBufSize) {
        if (writeBufSize == true) {
            fwrite(&numUsedBytes, 1, sizeof(size_t), file);
        }
        size_t written = fwrite(this->store, 1, this->numUsedBytes, file);
        if (written < this->numUsedBytes) {
            SINUCA3_ERROR_PRINTF("Buffer error\n");
        }
        this->numUsedBytes = 0;
    }

    inline bool IsBufFull() {
        return ((BUFFER_SIZE) - this->numUsedBytes < this->minSpacePerOperation);
    }
};

struct TraceFileHandler {
    char imgName[MAX_IMAGE_NAME_SIZE];

    FILE *staticTraceFile;
    std::vector<FILE *> dynamicTraceFiles;
    std::vector<FILE *> memoryTraceFiles;

    Buffer *staticBuffer;
    std::vector<Buffer *> dynamicBuffers;
    std::vector<Buffer *> memoryBuffers;

    TraceFileHandler(){
        SINUCA3_DEBUG_PRINTF("TraceFileHandler created\n");
        this->imgName[0] = '\0';

        //32 is an arbitrary value, just to avoid reallocation
        this->dynamicTraceFiles = std::vector<FILE *>(32, NULL);
        this->memoryTraceFiles  = std::vector<FILE *>(32, NULL);

        this->dynamicBuffers = std::vector<Buffer *>(32, NULL);
        this->memoryBuffers  = std::vector<Buffer *>(32, NULL);
    }

    void InitTraceFileHandler(const char* _imgName){
        std::strcpy(this->imgName, _imgName);
    }

    void OpenNewTraceFile(traceGenerator::TraceType type, unsigned int threadID);
    void CloseTraceFile(traceGenerator::TraceType type, unsigned int threadID);

};

}  // namespace traceGenerator

#endif
