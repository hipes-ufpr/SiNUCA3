#ifndef PINTOOL_HPP_

#include <cstddef>  // size_t
#include <cstring>  // memcpy

#include "../src/utils/logging.hpp"

#define BUFFER_SIZE 1 << 20
#define MEMREAD_EA IARG_MEMORYREAD_EA
#define MEMREAD_SIZE IARG_MEMORYREAD_SIZE
#define MEMWRITE_EA IARG_MEMORYWRITE_EA
#define MEMWRITE_SIZE IARG_MEMORYWRITE_SIZE
#define MEMREAD2_EA IARG_MEMORYREAD2_EA

namespace sinuca {
namespace traceGenerator {

struct Buffer {
    char store[BUFFER_SIZE];
    size_t numUsedBytes;
    size_t minSpaceNecessary;

    Buffer(size_t min) {
        numUsedBytes = 0;
        this->minSpaceNecessary = min;
    }

    inline void loadBufToFile(FILE* file) {
        fwrite(&numUsedBytes, 1, sizeof(size_t), file);
        size_t written = fwrite(this->store, 1, this->numUsedBytes, file);
        if (written < this->numUsedBytes) {
            SINUCA3_ERROR_PRINTF("Buffer error");
        }
        this->numUsedBytes = 0;
    }

    inline bool isBufFull() {
        return ((BUFFER_SIZE) - this->numUsedBytes < this->minSpaceNecessary);
    }

    inline void setMinNecessary(size_t num) { this->minSpaceNecessary = num; }
};

struct DataINS {
    long addr;
    unsigned short int baseReg;
    unsigned short int indexReg;
    unsigned char size;
    unsigned char booleanValues;
    unsigned char numReadRegs;
    unsigned char numWriteRegs;
} __attribute__((packed));  // no padding

struct DataMEM {
    long addr;
    int size;
} __attribute__((packed));  // no padding

}  // namespace traceGenerator
}  // namespace sinuca

#define PINTOOL_HPP_
#endif
