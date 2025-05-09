#include "x86_generator_file_handler.hpp"

std::string FormatPathTidIn(std::string sourceDir, std::string prefix,
                            std::string imageName, std::string tid) {
    return sourceDir + "/" + prefix + "_" + imageName + "_tid" + tid + ".trace";
}

std::string FormatPathTidOut(std::string sourceDir, std::string prefix,
                             std::string imageName) {
    return sourceDir + "/" + prefix + "_" + imageName + ".trace";
}

trace::traceGenerator::StaticTraceFile::StaticTraceFile(std::string path)
    : TraceFileWriter(path) {
    this->threadCount = 0;
    this->bblCount = 0;
    this->instCount = 0;

    /*
     * This space will be used to store the total amount of BBLs,
     * number of instructions, and total number of threads
     */
    fseek(this->tf.file, 3 * sizeof(unsigned int), SEEK_SET);
}

trace::traceGenerator::StaticTraceFile::~StaticTraceFile() {
    this->FlushBuffer();
    rewind(this->tf.file);
    fwrite(&this->threadCount, sizeof(threadCount), 1, this->tf.file);
    fwrite(&this->bblCount, sizeof(bblCount), 1, this->tf.file);
    fwrite(&this->instCount, sizeof(instCount), 1, this->tf.file);
}

void trace::traceGenerator::StaticTraceFile::NewBBL(unsigned int numIns) {
    this->bblCount++;
}

void trace::traceGenerator::StaticTraceFile::CreateDataINS() {
    this->instCount++;
}

void trace::traceGenerator::StaticTraceFile::PrepareData(void *ptr, int op) {}

trace::traceGenerator::DynamicTraceFile::DynamicTraceFile(std::string path)
    : TraceFileWriter(path) {}

trace::traceGenerator::DynamicTraceFile::~DynamicTraceFile() {
    this->FlushBuffer();
}

void trace::traceGenerator::DynamicTraceFile::PrepareData(void *ptr, int op) {}

trace::traceGenerator::MemoryTraceFile::MemoryTraceFile(std::string path)
    : TraceFileWriter(path) {}

trace::traceGenerator::MemoryTraceFile::~MemoryTraceFile() {
    this->FlushBuffer();
}

void trace::traceGenerator::MemoryTraceFile::PrepareData(void *ptr, int op) {}

void trace::traceGenerator::MemoryTraceFile::WriteStd() {}

void trace::traceGenerator::MemoryTraceFile::WriteNonStd() {}
