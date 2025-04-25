#ifndef PINTOOL_HPP_

#define MEMREAD_EA IARG_MEMORYREAD_EA
#define MEMREAD_SIZE IARG_MEMORYREAD_SIZE
#define MEMWRITE_EA IARG_MEMORYWRITE_EA
#define MEMWRITE_SIZE IARG_MEMORYWRITE_SIZE
#define MEMREAD2_EA IARG_MEMORYREAD2_EA

#include "../src/engine/default_packets.hpp"

namespace traceGenerator {

const int MAX_INSTRUCTION_NAME_LENGTH = 32;

static inline void SetBit(unsigned char* byte, int position, bool value) {
    if (value == true) {
        *byte |= (1 << position);
    } else {
        *byte &= 0xff - (1 << position);
    }
}

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

}  // namespace traceGenerator

#define PINTOOL_HPP_
#endif
