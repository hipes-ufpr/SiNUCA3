#ifndef FILEHANDLER_HPP_
#define FILEHANDLER_HPP_

#include <cassert>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "../src/utils/logging.hpp"

const unsigned long BUFFER_SIZE = 1 << 20;

// Used in alignas to avoid false sharing
const unsigned long CACHE_LINE_SIZE = 64;

const unsigned long MAX_IMAGE_NAME_SIZE = 64;

namespace traceGenerator {

const std::string traceFolderPath("../trace/");

typedef unsigned int THREADID;
typedef unsigned long UINT32;

class TraceFile {
    protected:
        unsigned char *buf;
        FILE *file;
        size_t numUsedBytes;

        TraceFile(const char* prefix, const char* imageName, const char* sufix){
            this->buf = new unsigned char[BUFFER_SIZE];
            this->numUsedBytes = 0;
            std::string filePath = traceFolderPath + prefix + imageName + sufix + ".trace";
            this->file = fopen(filePath.c_str(), "wb");
        }

        virtual ~TraceFile() {
            delete[] this->buf;
            fclose(this->file);
        }

        void WriteToBuffer(void *src, size_t size){
            if(this->numUsedBytes + size >= BUFFER_SIZE)
                this->FlushBuffer();

            memcpy(this->buf + this->numUsedBytes, src, size);
            this->numUsedBytes += size;
        }

        void FlushBuffer() {
            size_t written = fwrite(this->buf, 1, this->numUsedBytes, this->file);
            assert(written < this->numUsedBytes && "fwrite returned something wrong");
            this->numUsedBytes = 0;
        }
};

class StaticTraceFile : TraceFile {
    public:
        unsigned int numThreads;
        unsigned int bblCount;
        unsigned int instCount;

        StaticTraceFile(const char* imageName);
        ~StaticTraceFile();
        void NewBBL(UINT32 numIns);
        void Write(const struct DataINS *data);
};

class DynamicTraceFile : TraceFile {
    public:
        DynamicTraceFile(const char* imageName, THREADID tid);
        void Write(const UINT32 bblId);
};

class MemoryTraceFile : TraceFile {
    public:
        MemoryTraceFile(const char* imageName, THREADID tid);
        void WriteStd(const struct DataMEM *data);
        void WriteNonStd(const struct DataMEM *readings, unsigned short numReadings, const struct DataMEM *writings, unsigned short numWritings);
};

}  // namespace traceGenerator

#endif
