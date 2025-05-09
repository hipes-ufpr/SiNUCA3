#ifndef FILEHANDLER_HPP_
#define FILEHANDLER_HPP_

#include <cstddef>
#include <cstdio> // FILE*
#include <string>

#include "../engine/default_packets.hpp" // sinuca::Branch

const int MAX_INSTRUCTION_NAME_LENGTH = 32;
// 1MiB 
const unsigned long BUFFER_SIZE = 1 << 20;
// Used in alignas to avoid false sharing
const unsigned long CACHE_LINE_SIZE = 64;
// Adjust if needed
const unsigned long MAX_IMAGE_NAME_SIZE = 255;

namespace trace {

typedef unsigned int BBLID;
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

struct TraceFile {
    unsigned char *buf;
    FILE *file;
    size_t offset;  // in bytes

    inline TraceFile() : offset(0) {
        this->buf = new unsigned char[BUFFER_SIZE];
    }
    inline ~TraceFile() {
        delete[] this->buf;
        fclose(file);
    }
};

class TraceFileReader {
  protected:
    bool eofFound;
    size_t eofLocation;
    size_t bufActiveSize;
    TraceFile tf;

    TraceFileReader(std::string);
    int RetrieveLenBytes(void *, size_t);
    int SetBufActiveSize(size_t);
    void RetrieveBuffer();
    virtual void InterpretData(void *, int) = 0;
};

class TraceFileWriter {
  protected:
    TraceFile tf;

    TraceFileWriter(std::string);
    int AppendToBuffer(void *, size_t);
    void FlushLenBytes(void *, size_t);
    void FlushBuffer();
    virtual void PrepareData(void *, int) = 0;
};

}  // namespace trace

#endif
