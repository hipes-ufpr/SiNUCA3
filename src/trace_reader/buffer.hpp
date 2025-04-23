#ifndef SINUCA3_TRACE_READER_BUFFER_HPP_
#define SINUCA3_TRACE_READER_BUFFER_HPP_

#include <cstddef>
#include <cstdio>

const unsigned long BUFFER_SIZE = 1 << 20;

struct Buffer {
    int readBuffer(FILE *file) {
        int result = fread(this->store, 1, this->bufSize, file);
        this->offset = 0;
        if (result <= 0)
            return 1;
        else if ((unsigned long)result < this->bufSize)
            this->eofLocation = result;

        return 0;
    }
    int readBufSizeFromFile(FILE *file) {
        size_t read = fread(&this->bufSize, 1, sizeof(this->bufSize), file);
        if (read <= 0) {
            return 1;
        }

        return 0;
    }

    Buffer() : offset(0), bufSize(0), eofLocation(0) {}

    char store[BUFFER_SIZE];
    size_t offset;
    size_t bufSize;
    size_t eofLocation;
};

#endif
