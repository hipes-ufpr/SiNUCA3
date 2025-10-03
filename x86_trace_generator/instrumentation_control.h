#ifndef SINUCA3_INSTRUMENTATION_CONTROL_H_
#define SINUCA3_INSTRUMENTATION_CONTROL_H_

//
// Copyright (C) 2025  HiPES - Universidade Federal do Paraná
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
void BeginInstrumentationBlock(void);

/**
 * @brief Ends an instrumentation block.
 *
 * Code following this call will no longer be instrumented.
 * This must be paired with a preceding BeginInstrumentationBlock call.
 */
void EndInstrumentationBlock(void);

/**
 * @brief Enables analysis code execution for the current thread.
 *
 * This function allows the execution of previously inserted instrumentation
 * code (analysis) for the calling thread.
 */
void EnableThreadInstrumentation(void);

/**
 * @brief Disables analysis code execution for the current thread.
 */
void DisableThreadInstrumentation(void);

/**
 * @brief Initializes the structures needed for handling intrinsics.
 */
void InitIntrinsics(void);
/**
 * @brief De-initializes the structures needed for handling intrinsics.
 */
void DeInitIntrinsics(void);

/**
 * @brief Don't call. Switches context for an intrinsic implementation.
 */
void __IntrinsicsSwitchContext(void);

/**
 * @brief Don't call. Holds the current intrinsic virtual call.
 */
extern void (*__intrinsicCall)(void);

/**
 * @details Defines an intrinsic function. Example:
 *
 * ```
 * DEFINE_INTRINSIC(Factorial) {
 *     long value;
 *     GetParameterGPR(1, &value);
 *     for (int i = value - 1; i > 0; --i) value *= i;
 *     SetReturnGPR(0, &value);
 * }
 * ```
 */
#define DEFINE_INTRINSIC(name)                                               \
    __asm__(                                                                 \
        ".intel_syntax noprefix\n"                                           \
        ".section .text\n"                                                   \
        ".type __" #name                                                     \
        "Loader,@function\n"                                                 \
        "__" #name                                                           \
        "Loader:\n"                                                          \
        "\tpush rbp\n"                                                       \
        "\tmov rbp, rsp\n"                                                   \
        "\tpush rax\n" /* We will need to keep it's value. */                \
        "\tlea rax, [rip + " #name                                           \
        "]\n"                                                                \
        "\tmov [rip + __intrinsicCall], rax\n"                               \
        "\tpop rax\n" /* Restore rax value to perform the context switch. */ \
        "\tcall __IntrinsicsSwitchContext\n"                                 \
        "\tpop rbp\n"                                                        \
        "\tret\n"                                                            \
        ".att_syntax prefix\n");                                             \
    void name(void)

/**
 * @details Inline Assembly template for calling intrinsics. For instance, to
 * call the intrinsic Factorial passing a value in rbx as parameter and getting
 * a return in rax, one would:
 *
 * ```
 * int ret;
 * __asm__ volatile(CALL_INTRINSIC_TEMPLATE(Factorial) : "=a"(ret) : "b"(5) :);
 * ```
 */
#define CALL_INTRINSIC_TEMPLATE(intrinsic) \
    ".intel_syntax noprefix\n"             \
    "\tcall __" #intrinsic                 \
    "Loader\n"                             \
    ".att_syntax prefix\n"

/** @brief Gets a parameter value passed in a GPR. */
void GetParameterGPR(int reg, void* output64bit);
/** @brief Sets a return value passed in a GPR. */
void SetReturnGPR(int reg, const void* input64bit);
/** @brief Gets a parameter value passed in a XMM (SSE) register. */
void GetParameterXMM(int reg, void* output128bit);
/** @brief Sets a return value passed in a XMM (SSE) register. */
void SetReturnXMM(int reg, const void* input128bit);
/** @brief Gets a parameter value passed in a YMM (AVX) register. */
void GetParameterYMM(int reg, void* output256bit);
/** @brief Sets a return value passed in a YMM (AVX) register. */
void SetReturnYMM(int reg, const void* input256bit);

#ifdef __cplusplus
}
#endif  // __cplusplus

#endif  // SINUCA3_INSTRUMENTATION_CONTROL_H_
