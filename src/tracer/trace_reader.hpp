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
 * @brief Pure virtual TraceReader class that all trace readers must implement.
 */

#include <engine/default_packets.hpp>

enum FetchResult {
    FetchResultOk,
    FetchResultEnd,
    FetchResultError,
};

class TraceReader {
  public:
    /**
     * @brief Class attributes initializer.
     * @param imageName Name of the executable used to generate the traces.
     * @param sourceDir Complete path to the directory that stores the traces.
     * @return Non-zero on failure.
     */
    virtual int OpenTrace(const char *imageName, const char *sourceDir) = 0;
    /**
     * @return Number of threads.
     */
    virtual int GetTotalThreads() = 0;
    /**
     * @brief Number of currently fetched instructions.
     * @param tid Thread Identifier.
     */
    virtual unsigned long GetNumberOfFetchedInst(int tid) = 0;
    /**
     * @brief Number of instructions to be fetched.
     * @param tid Thread Identifier.
     */
    virtual unsigned long GetTotalInstToBeFetched(int tid) = 0;
    /**
     * @brief Number of basic blocks in static trace.
     */
    virtual unsigned long GetTotalBBLs() = 0;
    /**
     * @brief Any statistic of interest.
     */
    virtual void PrintStatistics() = 0;
    /**
     * @brief Get next executed instruction.
     * @param ret Pointer to struct that will be filled.
     * @param tid Thread identifier.
     */
    virtual FetchResult Fetch(InstructionPacket *ret, int tid) = 0;
    /**
     * @brief Free allocated memory in OpenTrace.
     */
    virtual void CloseTrace() = 0;
    virtual ~TraceReader() {}
};

#endif  // SINUCA3_TRACE_READER_HPP_
