#ifndef X86GENERATORFILEHANDLER_HPP_
#define X86GENERATORFILEHANDLER_HPP_

#include <cassert>
#include <string>

#include "../src/utils/file_handler.hpp"

namespace trace {
namespace traceGenerator {

class StaticTraceFile : public TraceFileWriter {
  public:
    StaticTraceFile(std::string);
    ~StaticTraceFile();
    virtual void PrepareData(void *, int) override;
  private:
    unsigned int threadCount;
    unsigned int bblCount;
    unsigned int instCount;

    void NewBBL(unsigned int);
    void CreateDataINS();
};

class DynamicTraceFile : public TraceFileWriter {
  public:
    DynamicTraceFile(std::string);
    ~DynamicTraceFile();
    virtual void PrepareData(void *, int) override;
};

class MemoryTraceFile : public TraceFileWriter {
  public:
    MemoryTraceFile(std::string);
    ~MemoryTraceFile();
    virtual void PrepareData(void *, int) override;
  private:
    void WriteStd();
    void WriteNonStd();
};

}  // namespace traceGenerator
}  // namespace trace

#endif
