#ifndef X86READERFILEHANDLER_HPP_
#define X86READERFILEHANDLER_HPP_

#include <cstddef>

#include "../utils/file_handler.hpp"

extern "C" {
#include <fcntl.h>     // open
#include <sys/mman.h>  // mmap
#include <unistd.h>    // lseek
}

using namespace trace;

namespace sinuca {
namespace traceReader {

struct InstructionInfo {
    sinuca::StaticInstructionInfo staticInfo;

    /** @brief Fields reserved for reader internal use */
    unsigned short staticNumReadings;
    unsigned short staticNumWritings;
};

const int GET_READ_REGS = 2;
const int GET_WRITE_REGS = 3;

class StaticTraceFile {
  private:
    unsigned int totalBBLs;
    unsigned int totalIns;
    unsigned int numThreads;
    char *mmapPtr;
    size_t mmapOffset;
    size_t mmapSize;
    int fd;

    void *GetData(size_t);
    void GetFlagValues(InstructionInfo *, DataINS *);
    void GetBranchFields(sinuca::StaticInstructionInfo *, DataINS *);
    void GetReadRegs(sinuca::StaticInstructionInfo *, DataINS *);
    void GetWriteRegs(sinuca::StaticInstructionInfo *, DataINS *);

  public:
    StaticTraceFile(std::string, std::string);
    ~StaticTraceFile();
    inline unsigned int GetTotalBBLs() { return this->totalBBLs; }
    inline unsigned int GetTotalIns() { return this->totalIns; }
    inline unsigned int GetNumThreads() { return this->numThreads; }
    void ReadNextPackage(InstructionInfo *);
    unsigned int GetNewBBlSize();
};

class DynamicTraceFile : public TraceFileReader {
  public:
    DynamicTraceFile(std::string, std::string, THREADID);
    int ReadNextBBl(BBLID *);
};

class MemoryTraceFile : public TraceFileReader {
  public:
    MemoryTraceFile(std::string, std::string, THREADID);
    int ReadNextMemAccess(InstructionInfo *, DynamicInstructionInfo *);
};

}  // namespace traceReader
}  // namespace sinuca

#endif