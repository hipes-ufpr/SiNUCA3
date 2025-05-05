#ifndef FILEHANDLER_HPP_
#define FILEHANDLER_HPP_

#include <cassert>
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <string>

#include "../engine/default_packets.hpp" // sinuca::Branch

const int MAX_INSTRUCTION_NAME_LENGTH = 32;
// 1MiB 
const unsigned long BUFFER_SIZE = 1 << 20;
// Used in alignas to avoid false sharing
const unsigned long CACHE_LINE_SIZE = 64;
// Adjust if needed
const unsigned long MAX_IMAGE_NAME_SIZE = 64;

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

    TraceFile();
    void SetFilePath(const char *, const char *, const char *, const char *);
    virtual ~TraceFile();
};

class TraceFileReader : public TraceFile {
  protected:
    size_t eofLocation;
    size_t bufSize;

    TraceFileReader(const char *, const char *, const char *, const char *);
    int ReadBuffer();
    int ReadBufSizeFromFile();
};

class TraceFileGenerator : public TraceFile {
  protected:
    TraceFileGenerator(const char *, const char *, const char *, const char *);
    void WriteToBuffer(void *, size_t);
    virtual void FlushBuffer();
};

}  // namespace trace

#endif
