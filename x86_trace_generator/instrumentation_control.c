#include "instrumentation_control.h"

__attribute__((noinline)) void BeginInstrumentationBlock(void) {
    __asm__ volatile("" :::);
}

__attribute__((noinline)) void EndInstrumentationBlock(void) {
    __asm__ volatile("" :::);
}

__attribute__((noinline)) void EnableThreadInstrumentation(void) {
    __asm__ volatile("" :::);
}

__attribute__((noinline)) void DisableThreadInstrumentation(void) {
    __asm__ volatile("" :::);
}

#ifdef __x86_64

#include <cpuid.h>
#include <stdlib.h>

char* __intrinsicsXSAVEbuffer;

void InitIntrinsics(void) {
    unsigned int eax;
    unsigned int ebx;
    unsigned int ecx;
    unsigned int edx;

    __cpuid_count(0xD, 0, eax, ebx, ecx, edx);

    // ecx contains the XSAVE area for all supported features.

    // We need to allocate a 64-byte aligned memory area for XSAVE. After C11,
    // we can use aligned_alloc, but before it we can try posix_memalign.
#if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L
    __intrinsicsXSAVEbuffer = (char*)aligned_alloc(64, ecx);
#elif defined(_POSIX_C_SOURCE) && _POSIX_C_SOURCE >= 200112L
    posix_memalign((void**)&__intrinsics_xsave_buffer, 64, ecx);
#else
#error "No aligned allocation support."
#endif
}

void DeInitIntrinsics(void) { free(__intrinsicsXSAVEbuffer); }

#endif  // __x86_64
