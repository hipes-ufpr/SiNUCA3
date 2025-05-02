#ifndef FILEHANDLER_HPP_
#define FILEHANDLER_HPP_

#include <cassert>
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <string>

#include "../engine/default_packets.hpp"
#include "../utils/logging.hpp"

const unsigned long BUFFER_SIZE = 1 << 20;
// Used in alignas to avoid false sharing
const unsigned long CACHE_LINE_SIZE = 64;
const unsigned long MAX_IMAGE_NAME_SIZE = 64;
const int MAX_INSTRUCTION_NAME_LENGTH = 32;

namespace trace {
  
typedef unsigned int BBlId;
typedef unsigned int THREADID;


enum BooleanValuesIndex {
  IS_PREDICATED = 0,
  IS_PREFETCH = 1,
  IS_CONTROL_FLOW = 2,
  IS_INDIRECT_CONTROL_FLOW = 3,
  IS_NON_STANDARD_MEM_OP = 4,
  IS_READ = 5,
  IS_READ2 = 6,
  IS_WRITE = 7,
};

struct DataINS {
  char name[MAX_INSTRUCTION_NAME_LENGTH];
  long addr;
  unsigned short int readRegs[64];
  unsigned short int writeRegs[64];
  unsigned short int baseReg;
  unsigned short int indexReg;
  unsigned char size;
  unsigned char booleanValues;
  unsigned char numReadRegs;
  unsigned char numWriteRegs;
  sinuca::Branch branchType;
} __attribute__((packed));  // no padding

struct DataMEM {
  unsigned long addr;
  unsigned int size;
} __attribute__((packed));  // no padding

class TraceFile {
  protected:
    unsigned char *buf;
    FILE *file;
    size_t offset;  // in bytes
    std::string filePath;

    TraceFile(const char *prefix, const char *imageName, const char *sufix, std::string tracePath) {
        this->buf = new unsigned char[BUFFER_SIZE];
        this->offset = 0;
        this->filePath = tracePath + prefix + imageName + sufix + ".trace";
    }

    virtual ~TraceFile() {
        delete[] this->buf;
        fclose(this->file);
    }
};

class TraceFileReader : public TraceFile {
  protected:
    size_t eofLocation;
    size_t bufSize;

    TraceFileReader(const char *prefix, const char *imageName, const char *sufix,
                const char *traceFolderPath)
        : TraceFile(prefix, imageName, sufix, std::string(traceFolderPath)) {
        this->file = fopen(this->filePath.c_str(), "rb");
        if (this->file == NULL) {
            SINUCA3_ERROR_PRINTF("Could not open => %s\n", this->filePath.c_str());
        }
    }

    int ReadBuffer() {
        size_t result = fread(this->buf, 1, this->bufSize, file);
        this->offset = 0;

        if (result <= 0) {
            return 1;
        } else if (result < this->bufSize) {
            this->eofLocation = result;
        }

        return 0;
    }

    int ReadBufSizeFromFile() {
        size_t read = fread(&this->bufSize, 1, sizeof(this->bufSize), this->file);
        if (read <= 0) {return 1;}
        return 0;
    }
};

class TraceFileGenerator : public TraceFile {
  protected:
    TraceFileGenerator(const char *prefix, const char *imageName, const char *sufix,
                const char *traceFolderPath)
        : TraceFile(prefix, imageName, sufix, std::string(traceFolderPath)) {
        this->file = fopen(this->filePath.c_str(), "wb");
    }

    void WriteToBuffer(void *src, size_t size) {
        if (this->offset + size >= BUFFER_SIZE) this->FlushBuffer();

        memcpy(this->buf + this->offset, src, size);
        this->offset += size;
    }

    virtual void FlushBuffer() {
        size_t written = fwrite(this->buf, 1, this->offset, this->file);
        assert(written == this->offset && "fwrite returned something wrong");
        this->offset = 0;
    }
};

}  // namespace trace

#endif
