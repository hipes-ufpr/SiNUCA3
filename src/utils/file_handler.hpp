#ifndef FILEHANDLER_HPP_
#define FILEHANDLER_HPP_

#include <cstddef>
#include <cstdio>  // FILE*

#include "../engine/default_packets.hpp"  // sinuca::Branch

const int MAX_INSTRUCTION_NAME_LENGTH = 32;
// 1MiB
const unsigned long BUFFER_SIZE = 1 << 20;
// Used in alignas to avoid false sharing
const unsigned long CACHE_LINE_SIZE = 64;
// Adjust if needed
const unsigned long MAX_IMAGE_NAME_SIZE = 255;
// Used to standardize reading and writing
const size_t SIZE_NUM_MEM_R_W = sizeof(unsigned short);
// Used to standardize reading and writing
const size_t SIZE_NUM_BBL_INS = sizeof(unsigned int);
// Adjust if needed
const unsigned long MAX_REG_OPERANDS = 8;

namespace trace {

typedef unsigned int BBLID;
typedef unsigned int THREADID;

struct DataINS {
    long addr;
    char name[MAX_INSTRUCTION_NAME_LENGTH];
    unsigned short int readRegs[MAX_REG_OPERANDS];
    unsigned short int writeRegs[MAX_REG_OPERANDS];
    unsigned short int baseReg;
    unsigned short int indexReg;
    unsigned char size;
    unsigned char numReadRegs;
    unsigned char numWriteRegs;
    unsigned char isPredicated : 1;
    unsigned char isPrefetch : 1;
    unsigned char isControlFlow : 1;
    unsigned char isIndirectControlFlow : 1;
    unsigned char isNonStandardMemOp : 1;
    unsigned char isRead : 1;
    unsigned char isRead2 : 1;
    unsigned char isWrite : 1;
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

    void UseFile(const char *path);
    size_t RetrieveLenBytes(void *, size_t);
    int SetBufActiveSize(size_t);
    void RetrieveBuffer();
    void *GetData(size_t);
};

class TraceFileWriter {
  protected:
    TraceFile tf;

    void UseFile(const char *);
    int AppendToBuffer(void *, size_t);
    void FlushLenBytes(void *, size_t);
    void FlushBuffer();
};

unsigned long GetPathTidInSize(const char *sourceDir, const char *prefix,
                               const char *imageName);
void FormatPathTidIn(char *dest, const char *sourceDir, const char *prefix,
                     const char *imageName, unsigned long bufferSize,
                     THREADID tid);

unsigned long GetPathTidOutSize(const char *sourceDir, const char *prefix,
                                const char *imageName);
void FormatPathTidOut(char *dest, const char *sourceDir, const char *prefix,
                      const char *imageName, unsigned long bufferSize);

}  // namespace trace

#endif
