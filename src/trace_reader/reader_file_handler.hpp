#ifndef READERFILEHANDLER_HPP_
#define READERFILEHANDLER_HPP_

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

class StaticTraceFile {
  private:
    char *mmapPtr;
    size_t mmapOffset;
    size_t mmapSize;
    int fd;
    unsigned int totalBBLs;
    unsigned int totalIns;
    unsigned int numThreads;

  public:
    StaticTraceFile(const char *);
    ~StaticTraceFile();
    unsigned int GetTotalBBLs();
    unsigned int GetTotalIns();
    unsigned int GetNumThreads();
    unsigned int GetNewBBlSize();
    void ReadNextPackage(InstructionInfo *);
};

class DynamicTraceFile : public TraceFileReader {
  public:
    DynamicTraceFile(const char *imageName, THREADID tid,
                     const char *traceFolderPath);
    int ReadNextBBl(BBlId *);
};

class MemoryTraceFile : public TraceFileReader {
  public:
    MemoryTraceFile(const char *imageName, THREADID tid,
                    const char *traceFolderPath);
    int ReadNextMemAccess(InstructionInfo *, InstructionPacket *);
};

}  // namespace traceReader
}  // namespace sinuca

#endif