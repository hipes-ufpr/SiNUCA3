#ifndef X86_READER_FILE_HANDLER_HPP_
#define X86_READER_FILE_HANDLER_HPP_

//
// Copyright (C) 2024  HiPES - Universidade Federal do Paraná
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <https://www.gnu.org/licenses/>.
//

/**
 * @file x86_reader_file_handler.hpp
 * @brief Public API of the x86_64 trace reader.
 */

#include "../engine/default_packets.hpp"
#include "../utils/file_handler.hpp"

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
    unsigned long mmapOffset;
    unsigned long mmapSize;
    int fd;
    bool isValid;

    void *GetData(unsigned long);
    void GetFlagValues(InstructionInfo *, DataINS *);
    void GetBranchFields(sinuca::StaticInstructionInfo *, DataINS *);
    void GetRegs(sinuca::StaticInstructionInfo *, DataINS *);

  public:
    StaticTraceFile(const char *folderPath, const char *img);
    ~StaticTraceFile();
    inline unsigned int GetTotalBBLs() { return this->totalBBLs; }
    inline unsigned int GetTotalIns() { return this->totalIns; }
    inline unsigned int GetNumThreads() { return this->numThreads; }
    void ReadNextPackage(InstructionInfo *);
    unsigned int GetNewBBlSize();
    bool Valid();
};

class DynamicTraceFile : public TraceFileReader {
  private:
    bool isValid;

  public:
    DynamicTraceFile(const char *folderPath, const char *img, THREADID tid);
    int ReadNextBBl(BBLID *);
    bool Valid();
};

class MemoryTraceFile : public TraceFileReader {
  private:
    unsigned short GetNumOps();
    DataMEM *GetDataMemArr(unsigned short len);
    bool isValid;

  public:
    MemoryTraceFile(const char *folderPath, const char *img, THREADID tid);
    void MemRetrieveBuffer();
    int ReadNextMemAccess(InstructionInfo *, DynamicInstructionInfo *);
    bool Valid();
};

}  // namespace traceReader
}  // namespace sinuca

#endif
