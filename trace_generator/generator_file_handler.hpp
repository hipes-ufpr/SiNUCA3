#ifndef GENERATORFILEHANDLER_HPP_
#define GENERATORFILEHANDLER_HPP_

#include <cassert>
#include <cstdio>
#include <cstring>

#include "../src/utils/file_handler.hpp"

using namespace trace;

namespace traceGenerator {

class StaticTraceFile : public TraceFileGenerator {
  public:
    unsigned int numThreads;
    unsigned int bblCount;
    unsigned int instCount;

    StaticTraceFile(const char *, const char *);
    ~StaticTraceFile();
    void NewBBL(unsigned int);
    void Write(const struct DataINS *);
};

class DynamicTraceFile : public TraceFileGenerator {
  public:
    DynamicTraceFile(const char *, THREADID, const char *);
    ~DynamicTraceFile();
    void Write(const BBlId bblId);
};

class MemoryTraceFile : public TraceFileGenerator {
  public:
    MemoryTraceFile(const char *, THREADID, const char *);
    ~MemoryTraceFile();
    virtual void FlushBuffer() override;
    void WriteStd(const struct DataMEM *data);
    void WriteNonStd(const struct DataMEM *, unsigned short,
                     const struct DataMEM *, unsigned short);
};

}  // namespace traceGenerator

#endif
