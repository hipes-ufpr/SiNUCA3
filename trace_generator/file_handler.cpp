#include "file_handler.hpp"
#include "sinuca3_pintool.hpp"

const char* formatThreadSufix(traceGenerator::THREADID tid) {
    static char sufixBuffer[32]; // Buffer estático para garantir que a string permaneça válida
    std::snprintf(sufixBuffer, sizeof(sufixBuffer), "_%d", tid);
    return sufixBuffer;
}

traceGenerator::StaticTraceFile::StaticTraceFile(const char* imageName)
: TraceFile("static_", imageName, "" )
{
    this->numUsedBytes = 0;
    this->numThreads = 0;
    this->bblCount = 0;
    this->instCount = 0;

    /* This space will be used to store the amount   *
        * of BBLs in the trace, number of instructions, *
        * and total number of threads                   */
    fseek(this->file, 3 * sizeof(unsigned int),
            SEEK_SET);
}

traceGenerator::StaticTraceFile::~StaticTraceFile(){
    rewind(this->file);
    fwrite(&this->numThreads, 1, sizeof(numThreads), this->file);
    fwrite(&this->bblCount, 1, sizeof(bblCount), this->file);
    fwrite(&this->instCount, 1, sizeof(instCount), this->file);
}

void traceGenerator::StaticTraceFile::NewBBL(UINT32 numIns){
    this->WriteToBuffer((void*) &numIns, sizeof(numIns));
    this->bblCount++;
}

void traceGenerator::StaticTraceFile::Write(const struct DataINS *data){
    this->WriteToBuffer((void*) data, sizeof(struct DataINS));
    this->instCount++;
}

traceGenerator::DynamicTraceFile::DynamicTraceFile(const char* imageName, THREADID tid)
: TraceFile("dynamic_", imageName, formatThreadSufix(tid))
{}

void traceGenerator::DynamicTraceFile::Write(const UINT32 bblId){
    this->WriteToBuffer((void*) &bblId, sizeof(bblId));
}

traceGenerator::MemoryTraceFile::MemoryTraceFile(const char* imageName, THREADID tid)
: TraceFile("memory_", imageName, formatThreadSufix(tid))
{}

void traceGenerator::MemoryTraceFile::WriteStd(const struct DataMEM *data){
    this->WriteToBuffer((void*) data, sizeof(struct DataMEM));
}

void traceGenerator::MemoryTraceFile::WriteNonStd(const struct DataMEM *readings, unsigned short numReadings, const struct DataMEM *writings, unsigned short numWritings){
    this->WriteToBuffer((void*) &numReadings, sizeof(numReadings));
    this->WriteToBuffer((void*) &numWritings, sizeof(numWritings));

    this->WriteToBuffer((void*) readings, numReadings * sizeof(*readings));
    this->WriteToBuffer((void*) &writings, numWritings * sizeof(*writings));
}
