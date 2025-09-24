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

#ifdef __cplusplus
extern "C" {
#endif  // __cplusplus

/**
 * @file instrumentation_control.h
 * @brief API for instrumenting applications with SiNUCA3.
 */

/**
 * @brief Begins an instrumentation block.
 *
 * Any code appearing after this call (until the exectuion of a
 * corresponding EndInstrumentationBlock call) becomes will be instrumented,
 * meaning that analysis code may be inserted into target program
 * during the instrumentation phase.
 */
__attribute__((noinline)) void BeginInstrumentationBlock(void);

/**
 * @brief Ends an instrumentation block.
 *
 * Code following this call will no longer be instrumented.
 * This must be paired with a preceding BeginInstrumentationBlock call.
 */
__attribute__((noinline)) void EndInstrumentationBlock(void);

/**
 * @brief Enables analysis code execution for the current thread.
 *
 * This function allows the execution of previously inserted instrumentation
 * code (analysis) for the calling thread.
 */
__attribute__((noinline)) void EnableThreadInstrumentation(void);

/**
 * @brief Disables analysis code execution for the current thread.
 */
__attribute__((noinline)) void DisableThreadInstrumentation(void);

// Intrinsics API for the x86_64 trace generator.
#ifdef __x86_64

extern char* __intrinsicsXSAVEbuffer;

void InitIntrinsics(void);
void DeInitIntrinsics(void);

#ifdef __clang__
#define DECLARE_INTRINSIC(name) __attribute__((noinline)) void name(void)
#elif __GNUC__
#define DECLARE_INTRINSIC(name) \
    __attribute__((noinline, optimize("no-omit-frame-pointer"))) void name(void)
#else
#error "Unsupported compiler."
#endif

#define ENTER_INTRINSIC_IMPLEMENTATION()                                       \
    __asm__ volatile(                                                          \
        ".intel_syntax noprefix\n" /* General-purpose registers. Who though    \
                                      getting rid of pusha was a good idea? */ \
        "push rax\n"                                                           \
        "push rbx\n"                                                           \
        "push rcx\n"                                                           \
        "push rdx\n"                                                           \
        "push rsi\n"                                                           \
        "push rdi\n"                                                           \
        "push r8\n"                                                            \
        "push r9\n"                                                            \
        "push r10\n"                                                           \
        "push r11\n"                                                           \
        "push r12\n"                                                           \
        "push r13\n"                                                           \
        "push r14\n"                                                           \
        "push r15\n"                                                           \
        "pushfq\n"       /* Flags register */                                  \
        "xor rax, rax\n" /* XSAVE magic. */                                    \
        "sub rax, 1\n"                                                         \
        "mov rdx, rax\n"                                                       \
        "mov rbx, [__intrinsicsXSAVEbuffer]\n"                                 \
        "xsave [rbx]\n"                                                        \
        ".att_syntax\n");

#define EXIT_INTRINSIC_IMPLEMENTATION() \
    __asm__ volatile(                   \
        ".intel_syntax noprefix\n"      \
        "popfq\n"                       \
        "pop r15\n"                     \
        "pop r14\n"                     \
        "pop r13\n"                     \
        "pop r12\n"                     \
        "pop r11\n"                     \
        "pop r10\n"                     \
        "pop r9\n"                      \
        "pop r8\n"                      \
        "pop rdi\n"                     \
        "pop rsi\n"                     \
        "pop rdx\n"                     \
        "pop rcx\n"                     \
        "pop rbx\n"                     \
        "pop rax\n"                     \
        ".att_syntax\n");

#define CALL_INTRINSIC(function, output, ...) \
    __asm__ volatile("call " #function "\n" : output : __VA_ARGS__ :);

#define RDI(name) "D"(name)
#define RSI(name) "S"(name)

#define GET(register) "=" register

#endif  // __x86_64

#ifdef __cplusplus
}
#endif  // __cplusplus

#endif  // SINUCA3_INSTRUMENTATION_CONTROL_H_
