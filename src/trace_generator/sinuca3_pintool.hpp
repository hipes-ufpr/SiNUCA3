#ifndef PINTOOL_HPP_

#include <cstddef> // size_t
#include <cstring> // memcpy

#define BUFFER_SIZE 1<<20

inline void copy(char* buf, size_t* used, void* src, size_t size) {
    memcpy(buf+*used, src, size);
    (*used)+=size;
}

inline void setBit(unsigned char *byte, int position) {
    *byte |= (1 << position);
}

struct DataINS {
    long addr;
    unsigned short int baseReg;
    unsigned short int indexReg;
    unsigned char size;
    unsigned char booleanValues;
    unsigned char numMemReads;
    unsigned char numMemWrites;
    unsigned char numReadregs;
    unsigned char numWriteRegs;
} __attribute__((packed)); // no padding

struct DataMEM {
    long addr;
    int size;
} __attribute__((packed)); // no padding

#define PINTOOL_HPP_
#endif