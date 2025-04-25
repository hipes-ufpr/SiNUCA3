#include "file_handler.hpp"

#include "sinuca3_pintool.hpp"

const char* FormatThreadSuffix(traceGenerator::THREADID tid) {
    static char sufixBuffer[32];  // Buffer estático para garantir que a string
                                  // permaneça válida
    std::snprintf(sufixBuffer, sizeof(sufixBuffer), "_tid%d", tid);
    return sufixBuffer;
}

/* ============================================= */
/* StaticTraceFile */
/* ============================================= */

traceGenerator::StaticTraceFile::StaticTraceFile(const char* imageName)
    : TraceFile("static_", imageName, "") {
    this->offset = 0;
    this->numThreads = 0;
    this->bblCount = 0;
    this->instCount = 0;

    /* This space will be used to store the amount   *
     * of BBLs in the trace, number of instructions, *
     * and total number of threads                   */
    fseek(this->file, 3 * sizeof(unsigned int), SEEK_SET);
}

traceGenerator::StaticTraceFile::~StaticTraceFile() {
    this->FlushBuffer();
    rewind(this->file);
    fwrite(&this->numThreads, sizeof(numThreads), 1, this->file);
    fwrite(&this->bblCount, sizeof(bblCount), 1, this->file);
    fwrite(&this->instCount, sizeof(instCount), 1, this->file);
}

void traceGenerator::StaticTraceFile::NewBBL(unsigned int numIns) {
    this->WriteToBuffer((void*)&numIns, sizeof(numIns));
    this->bblCount++;
}

void traceGenerator::StaticTraceFile::Write(const struct DataINS* data) {
    this->WriteToBuffer((void*)data, sizeof(struct DataINS));
    this->instCount++;
}

/* ============================================= */
/* DynamicTraceFile */
/* ============================================= */

traceGenerator::DynamicTraceFile::DynamicTraceFile(const char* imageName,
                                                   THREADID tid)
    : TraceFile("dynamic_", imageName, FormatThreadSuffix(tid)) {}

traceGenerator::DynamicTraceFile::~DynamicTraceFile() {
    this->FlushBuffer();
}

void traceGenerator::DynamicTraceFile::Write(const UINT32 bblId) {
    this->WriteToBuffer((void*)&bblId, sizeof(bblId));
}

/* ============================================= */
/* MemoryTraceFile */
/* ============================================= */

traceGenerator::MemoryTraceFile::MemoryTraceFile(const char* imageName,
                                                 THREADID tid)
    : TraceFile("memory_", imageName, FormatThreadSuffix(tid)) {}

traceGenerator::MemoryTraceFile::~MemoryTraceFile() {
    this->FlushBuffer();
}

void traceGenerator::MemoryTraceFile::FlushBuffer() {
    // MemoryTraceFile stores how much someone can read before each buffer.
    size_t written = fwrite(&this->offset, 1, sizeof(this->offset), this->file);
    assert(written == sizeof(this->offset) && "fwrite returned something wrong");
    traceGenerator::TraceFile::FlushBuffer();
}

void traceGenerator::MemoryTraceFile::WriteStd(const struct DataMEM* data) {
    this->WriteToBuffer((void*)data, sizeof(struct DataMEM));
}

void traceGenerator::MemoryTraceFile::WriteNonStd(
    const struct DataMEM* readings, unsigned short numReadings,
    const struct DataMEM* writings, unsigned short numWritings) {
    this->WriteToBuffer((void*)&numReadings, sizeof(numReadings));
    this->WriteToBuffer((void*)&numWritings, sizeof(numWritings));

    this->WriteToBuffer((void*)readings, numReadings * sizeof(*readings));
    this->WriteToBuffer((void*)&writings, numWritings * sizeof(*writings));
}
