#ifndef __BUFFER__
#define __BUFFER__

#include <cstddef>
#include <cstdio>

#define BUFFER_SIZE 1 << 20

class Buffer {
  public:
    int readBuffer(FILE *file) {
        if (this->bufSize > BUFFER_SIZE) {
            return 1;
        }
        fread(this->store, 1, this->bufSize, file);
        this->offset = 0;

        return 0;
    }
    int readBufSizeFromFile(FILE *file) {
        size_t read = fread(&this->bufSize, 1, sizeof(this->bufSize), file);
        if (read <= 0) {
            return 1;
        }
    
        return 0;
    }

    Buffer() : offset(0), bufSize(0) {}

    char store[BUFFER_SIZE];
    size_t offset;
    size_t bufSize;
};

#endif