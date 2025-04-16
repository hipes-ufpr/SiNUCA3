#ifndef PINTOOL_HPP_

#define MEMREAD_EA IARG_MEMORYREAD_EA
#define MEMREAD_SIZE IARG_MEMORYREAD_SIZE
#define MEMWRITE_EA IARG_MEMORYWRITE_EA
#define MEMWRITE_SIZE IARG_MEMORYWRITE_SIZE
#define MEMREAD2_EA IARG_MEMORYREAD2_EA

#include "../src/engine/default_packets.hpp"

namespace traceGenerator {

static inline void CopyToBuf(char* buf, size_t* used, void* src, size_t size) {
    memcpy(buf + *used, src, size);
    (*used) += size;
}

static inline void SetBit(unsigned char* byte, int position, bool value) {
    if (value == true) {
        *byte |= (1 << position);
    } else {
        *byte &= 0xff - (1 << position);
    }
}

struct DataINS {
    long addr;
    unsigned short int baseReg;
    unsigned short int indexReg;
    unsigned char size;
    unsigned char booleanValues;
    unsigned char numReadRegs;
    unsigned char numWriteRegs;
    sinuca::Branch branchType;
} __attribute__((packed));  // no padding

struct DataMEM {
    long addr;
    int size;
} __attribute__((packed));  // no padding

}  // namespace traceGenerator

#define PINTOOL_HPP_
#endif
