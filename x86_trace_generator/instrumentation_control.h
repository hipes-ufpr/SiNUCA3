#ifndef SINUCA3_INSTRUMENTATION_CONTROL_H_
#define SINUCA3_INSTRUMENTATION_CONTROL_H_

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
 * @file instrumentation_control.h
 * @brief API for instrumenting applications with SiNUCA3.
 */

/**
 * @brief Begins an instrumentation block. Code after this call will be
 * instrumented.
 */
__attribute__((noinline, used)) void BeginInstrumentationBlock() {}

/**
 * @brief Ends an instrumentation block.
 */
__attribute__((noinline, used)) void EndInstrumentationBlock() {}

/**
 * @brief Enables thread instrumentation for the current thread. Must be called
 * inside an instrumentation block (i.e., after BeginInstrumentationBlock and
 * before EndInstrumentationBlock).
 */
__attribute__((noinline, used)) void EnableThreadInstrumentation() {}

/**
 * @brief Disables thread instrumentation for the current thread.
 */
__attribute__((noinline, used)) void DisableThreadInstrumentation() {}

#endif
