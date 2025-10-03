.intel_syntax noprefix

#
# Copyright (C) 2025  HiPES - Universidade Federal do Paraná
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <https://www.gnu.org/licenses/>.
#

#
# @file intrumentation_control.s
# @brief Implementation of the instrumentation control functions for x86_64.
#

#
# I wrote most of the working Assembly code between 23:00 and 5:00, in a weekend
# night. Them I slept until 12:00, and once again I'm here.
#
# "Knowing how to read x86_64 Assembly is the sign of a gentleman. Knowing how
# to write it well is the sign of a wasted life."
#
# ~ Gabriel
#

.section .data

intrinsicTable:
	.quad 0
xsaveBuffer:
	.quad 0
gprBuffer:
	.quad 0
__intrinsicCall:
	.quad 0

.section .text

.globl __intrinsicCall

.globl BeginInstrumentationBlock
.globl EndInstrumentationBlock
.globl EnableThreadInstrumentation
.globl DisableThreadInstrumentation
.globl InitIntrinsics
.globl DeInitIntrinsics
.globl __IntrinsicsSwitchContext
.globl GetParameterGPR
.globl SetReturnGPR
.globl GetParameterXMM
.globl SetReturnXMM
.globl GetParameterYMM
.globl SetReturnYMM

.type BeginInstrumentationBlock,@function
BeginInstrumentationBlock:
	ret

.type EndInstrumentationBlock,@function
EndInstrumentationBlock:
	ret

.type EnableThreadInstrumentation,@function
EnableThreadInstrumentation:
	ret

.type DisableThreadInstrumentation,@function
DisableThreadInstrumentation:
	ret

.type InitIntrinsics,@function
InitIntrinsics:
	push rbp
	mov rbp, rsp

	push rbx
	# Need to maintain the stack aligned to 16 bytes to call libc.
	push rbx

	# Save intrinsic table.
	mov [rip + intrinsicTable], rdi

	# Allocate xsaveBuffer.
	mov eax, 0xd
	mov ecx, 0
	cpuid
	mov ecx, ecx # Zero upper 32 bits of rcx.
	mov rdi, 64
	mov rsi, rcx
	call aligned_alloc
	mov [rip + xsaveBuffer], rax

	# One more because we needed to maintain the stack aligned to 16 bytes.
	pop rbx
	pop rbx

	pop rbp
	ret

.type DeInitIntrinsics,@function
DeInitIntrinsics:
	mov rdi, [rip + xsaveBuffer]
	jmp free

.type __IntrinsicsSwitchContext,@function
__IntrinsicsSwitchContext:
	push rbp
	mov rbp, rsp

	# General-purpouse registers. We save them in reverse order so they're
	# ordered (remember, the stack grows downwards).
	push r15
	push r14
	push r13
	push r12
	push r11
	push r10
	push r9
	push r8
	push rdi
	push rsi
	push rdx
	push rcx
	push rbx
	push rax

	mov [rip + gprBuffer], rsp

	# Flags register.
	pushfq

	# We need to maintain the stack aligned to 16 bytes to call libc.
	push rax

	# Extended CPU state.
	mov rbx, [rip + xsaveBuffer]
	mov rax, -1
	mov rdx, -1
	xsave [rbx]

	# Perform the virtual call.
	mov rax, [rip + __intrinsicCall]
	call rax

	# Restore extended CPU state.
	mov rax, [rip + xsaveBuffer]
	xrstor [rax]

	# Fix the stack alignment and restore flags.
	pop rax
	popfq

	# Restore general-purpouse registers.
	pop rax
	pop rbx
	pop rcx
	pop rdx
	pop rsi
	pop rdi
	pop r8
	pop r9
	pop r10
	pop r11
	pop r12
	pop r13
	pop r14
	pop r15

	pop rbp
	ret

.type GetParameterGPR,@function
GetParameterGPR:
	mov rcx, rdi
	shl rcx, 3
	mov rax, [rip + gprBuffer]
	mov rax, [rax + rcx]
	mov [rsi], rax
	ret

.type SetReturnGPR,@function
SetReturnGPR:
	mov rcx, rdi
	shl rcx, 3
	mov rsi, [rsi]
	mov rax, [rip + gprBuffer]
	mov [rax + rcx], rsi
	ret

.equ XSAVE_SSE_OFFSET, 160
.equ XSAVE_AVX_OFFSET, 576

.type GetParameterXMM,@function
GetParameterXMM:
	mov rcx, rdi
	shl rcx, 4
	mov rax, [rip + xsaveBuffer]
	movups xmm0, [rax + rcx + XSAVE_SSE_OFFSET]
	movups [rsi], xmm0
	ret

.type SetReturnXMM,@function
SetReturnXMM:
	mov rcx, rdi
	shl rcx, 4
	mov rax, [rip + xsaveBuffer]
	movups xmm0, [rsi]
	movups [rax + rcx + XSAVE_SSE_OFFSET], xmm0
	ret

.type GetParameterYMM,@function
GetParameterYMM:
	mov rcx, rdi
	shl rcx, 4
	mov rax, [rip + xsaveBuffer]
	movups xmm0, [rax + rcx + XSAVE_SSE_OFFSET]
	movups xmm1, [rax + rcx + XSAVE_AVX_OFFSET]
	movups [rsi], xmm0
	movups [rsi + 16], xmm1
	ret

.type SetReturnYMM,@function
SetReturnYMM:
	mov rcx, rdi
	shl rcx, 4
	mov rax, [rip + xsaveBuffer]
	movups xmm0, [rsi]
	movups xmm1, [rsi + 16]
	movups [rax + rcx + XSAVE_SSE_OFFSET], xmm0
	movups [rax + rcx + XSAVE_AVX_OFFSET], xmm1
	ret

.att_syntax prefix
