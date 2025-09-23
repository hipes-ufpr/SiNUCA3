#include <instrumentation_control.h>

void MemsetImpl(void) {
    __asm__ volatile(
        ".intel_syntax noprefix\n"
        /* Who though getting rid of pusha was a good idea? */
        "push rax\n"
        "push rbx\n"
        "push rcx\n"
        "push rdx\n"
        "push rsi\n"
        "push rdi\n"
        "push r8\n"
        "push r9\n"
        "push r10\n"
        "push r11\n"
        "push r12\n"
        "push r13\n"
        "push r14\n"
        "push r15\n"
        ".att_syntax\n");

    unsigned char* buffer;
    int size;

    __asm__ volatile("" : "=D"(buffer) : :);
    __asm__ volatile("" : "=S"(size) : :);

    for (int i = 0; i < size; ++i) {
        buffer[i] = 0xfe;
    }

    __asm__ volatile(
        ".intel_syntax noprefix\n"
        /* Who though getting rid of popa was a good idea? */
        "pop r15\n"
        "pop r14\n"
        "pop r13\n"
        "pop r12\n"
        "pop r11\n"
        "pop r10\n"
        "pop r9\n"
        "pop r8\n"
        "pop rdi\n"
        "pop rsi\n"
        "pop rdx\n"
        "pop rcx\n"
        "pop rbx\n"
        "pop rax\n"
        ".att_syntax\n");
}

int main(void) {
    volatile int size = 4096;
    volatile unsigned char buffer[size];

    BeginInstrumentationBlock();
    EnableThreadInstrumentation();

    __asm__ volatile("call MemsetImpl" : : "D"(buffer), "S"(size) :);

    // More instructions to fill the bottom of the trace, so we guarantee
    // simulation of the intrinsic.
    for (int i = 0; i < 2; ++i) {
        if (buffer[i] != 0xfe) {
            return 1;
        }
    }

    DisableThreadInstrumentation();
    EndInstrumentationBlock();

    return 0;
}
