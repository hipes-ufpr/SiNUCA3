#include "file_handler.hpp"

#include <cassert>
#include <cstdio>

#include "../utils/logging.hpp"

inline void printFileErrorLog(const char *path, const char *mode) {
    SINUCA3_ERROR_PRINTF("Could not open [%s] in [%s] mode\n", path, mode);
}

trace::TraceFileReader::TraceFileReader(std::string path) {
    char mode[] = "rb";
    tf.file = fopen(path.c_str(), mode);
    if (tf.file == NULL) {printFileErrorLog(path.c_str(), mode);}
    this->eofLocation = 0;
    this->eofFound = false;
}

int trace::TraceFileReader::RetrieveLenBytes(void *ptr, size_t len) {
    size_t read = fread(ptr, len, 1, this->tf.file);
    return read;
}

int trace::TraceFileReader::SetBufActiveSize(size_t size) {
    if (size > BUFFER_SIZE) {return 1;}
    this->bufActiveSize = size;
    return 0;
}

void trace::TraceFileReader::RetrieveBuffer() {
    int read = this->RetrieveLenBytes(this->tf.buf, this->bufActiveSize);
    if (read < this->bufActiveSize) {
        this->eofLocation = read;
        this->eofFound = true;
    }
    this->tf.offset = 0;
}

void* trace::TraceFileReader::GetData(size_t len) {
    void *ptr = (void*)(this->tf.buf + this->tf.offset);
    this->tf.offset += len;
    return ptr;
}

trace::TraceFileWriter::TraceFileWriter(std::string path) {
    char mode[] = "wb";
    this->tf.file = fopen(path.c_str(), mode);
    if (this->tf.file == NULL) {printFileErrorLog(path.c_str(), mode);}
}

int trace::TraceFileWriter::AppendToBuffer(void *ptr, size_t len) {
    if (BUFFER_SIZE - this->tf.offset < len) {return 1;}
    memcpy(this->tf.buf + this->tf.offset, ptr, len);
    this->tf.offset += len;
    return 0;
}

void trace::TraceFileWriter::FlushLenBytes(void *ptr, size_t len) {
    size_t written = fwrite(ptr, len, 1, this->tf.file);
    assert(written == len && "fwrite returned something wrong");
}

void trace::TraceFileWriter::FlushBuffer() {
    this->FlushLenBytes(this->tf.buf, this->tf.offset);
    this->tf.offset = 0;
}
