#ifndef SINUCA3_TRACE_READER_HPP_
#define SINUCA3_TRACE_READER_HPP_

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
 * @file trace_reader.hpp
 * @brief Pure virtual TraceReader class, that all trace readers must implement.
 */

#include <cstring>

#include "../sinuca3.hpp"

namespace sinuca {
namespace traceReader {

enum FetchResult {
    FetchResultOk,
    FetchResultEnd,
    FetchResultError,
};

/**
 * @brief TraceReader is a pure virtual class that all trace readers must
 * implement.
 */
class TraceReader {
  public:
    /** @brief Return non-zero on failure. */
    virtual int OpenTrace(const char* traceFileName) = 0;
    virtual unsigned long GetTraceSize() = 0;
    virtual unsigned long GetNumberOfFetchedInstructions() = 0;
    virtual void PrintStatistics() = 0;
    virtual FetchResult Fetch(InstructionPacket* ret) = 0;
    virtual ~TraceReader() {}
};

}  // namespace traceReader
}  // namespace sinuca

#endif  // SINUCA3_TRACE_READER_HPP_
