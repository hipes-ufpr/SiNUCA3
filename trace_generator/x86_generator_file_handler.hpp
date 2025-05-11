#ifndef X86GENERATORFILEHANDLER_HPP_
#define X86GENERATORFILEHANDLER_HPP_

#include <cassert>
#include <cstddef>
#include <string>

#include "../src/utils/file_handler.hpp"
#include "pin.H"

namespace trace {
namespace traceGenerator {

class StaticTraceFile : public TraceFileWriter {
  private:
    unsigned int threadCount;
    unsigned int bblCount;
    unsigned int instCount;

    void ResetFlags(struct DataINS *);
    void SetFlags(struct DataINS *, const INS *);
    void SetBranchFields(struct DataINS *, const INS *);
    void FillRegs(struct DataINS *, const INS *);
  public:
    StaticTraceFile(std::string, std::string);
    ~StaticTraceFile();
    void PrepareData(struct DataINS *, const INS *);
    void StAppendToBuffer(void *, size_t);
    inline void IncBBlCount() { this->bblCount++; }
    inline void IncInstCount() { this->instCount++; }
    inline void IncThreadCount() { this->threadCount++; }
    inline unsigned int GetBBlCount() { return this->bblCount; }
};

class DynamicTraceFile : public TraceFileWriter {
  public:
    DynamicTraceFile(std::string, std::string, THREADID);
    ~DynamicTraceFile();
    void DynAppendToBuffer(void *, size_t);
};

class MemoryTraceFile : public TraceFileWriter {
  public:
    MemoryTraceFile(std::string, std::string, THREADID);
    ~MemoryTraceFile();
    void PrepareDataNonStdAccess(unsigned short *, struct DataMEM[], unsigned short *,
                     struct DataMEM[], PIN_MULTI_MEM_ACCESS_INFO *);
    void MemAppendToBuffer(void *, size_t);
};

}  // namespace traceGenerator
}  // namespace trace

#endif
