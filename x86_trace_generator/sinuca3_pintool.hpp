#ifndef SINUCA3_SINUCA3_PINTOOL_HPP_
#define SINUCA3_SINUCA3_PINTOOL_HPP_

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
 * @file sinuca3_pintool.hpp
 * @brief
 */

#include <cassert>  // assert

#include "pin.H"

extern "C" {
#include <sys/stat.h>  // mkdir
#include <unistd.h>    // access
}

#include <sinuca3.hpp>
#include <utils/dynamic_trace_writer.hpp>
#include <utils/memory_trace_writer.hpp>
#include <utils/static_trace_writer.hpp>

/**
 * @brief Set this to 1 to print all rotines that name begins with "gomp",
 * case insensitive (Statically linking GOMP is recommended).
 */
extern const int DEBUG_PRINT_GOMP_RNT;

/**
 * @note Instrumentation is the process of deciding where and what code should
 * be inserted into the target program, while analysis refers to the code that
 * is actually executed at those insertion points to gather information about
 * the program’s behavior.
 *
 * @brief When enabled, this flag allows the pintool to record all instructions
 * static info into a static trace file, and allows the instrumentation phase
 * (e.g., in OnTrace) to insert analysis code into the target program. However,
 * the inserted analysis code will only execute at runtime if the corresponding
 * thread has its threadInstrumentationEnabled flag set to true.
 */
extern bool isInstrumentating;
/**
 * @note Instrumentation is the process of deciding where and what code should
 * be inserted into the target program, while analysis refers to the code that
 * is actually executed at those insertion points to gather information about
 * the program’s behavior.
 *
 * @brief Indicates, for each thread, whether it is allowed to execute
 * previously inserted analysis code.
 *
 * This flag does not control the instrumentation process itself (i.e., whether
 * code is inserted into the target program), but rather whether a specific
 * thread is permitted to execute that analysis code at runtime.
 *
 * The insertion of analysis code occurs only when the global
 * `isInstrumentating` flag is enabled. Later, during program execution, the
 * inserted code will only be executed by a thread if its corresponding entry in
 * this vector is set to true.
 *
 * When executed, the analysis code records dynamic and memory trace information
 * into files associated with the executing thread.
 */
extern std::vector<bool> isThreadAnalysisEnabled;

/** @brief Used to block two threads from trying to print simultaneously. */
extern PIN_LOCK printLock;
/** @brief OnThreadStart writes in global structures. */
extern PIN_LOCK threadStartLock;
/** @brief OnTrace writes in global structures. */
extern PIN_LOCK onTraceLock;

/**
 * @brief
 */
int Usage();
/**
 * @brief
 */
bool StrStartsWithGomp(const char* s);
/**
 * @brief
 */
void InitGompHandling();
/**
 * @brief
 */
void DebugPrintRtnName(const char* s, THREADID tid);
/**
 * @brief
 */
VOID InitInstrumentation();
/**
 * @brief
 */
VOID StopInstrumentation();
/**
 * @brief
 */
VOID EnableInstrumentationInThread(THREADID tid);
/**
 * @brief
 */
VOID DisableInstrumentationInThread(THREADID tid);
/**
 * @brief
 */
VOID InsertInstrumentionOnMemoryOperations(const INS* ins);
/**
 * @brief
 */
VOID AppendToDynamicTrace(UINT32 bblId, UINT32 numInst);
/**
 * @brief
 */
VOID AppendToMemTraceStd(ADDRINT addr, UINT32 size);
/**
 * @brief
 */
VOID AppendToMemTraceNonStd(PIN_MULTI_MEM_ACCESS_INFO* accessInfo);
/**
 * @brief
 */
VOID OnThreadStart(THREADID tid, CONTEXT* ctxt, INT32 flags, VOID* v);
/**
 * @brief
 */
VOID OnThreadFini(THREADID tid, const CONTEXT* ctxt, INT32 code, VOID* v);
/**
 * @brief
 */
VOID OnTrace(TRACE trace, VOID* ptr);
/**
 * @brief
 */
VOID OnImageLoad(IMG img, VOID* ptr);
/**
 * @brief
 */
VOID OnFini(INT32 code, VOID* ptr);

#endif