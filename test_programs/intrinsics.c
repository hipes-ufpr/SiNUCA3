#include <instrumentation_control.h>

DECLARE_INTRINSIC(MemsetImpl) {
    ENTER_INTRINSIC_IMPLEMENTATION();

    unsigned char* buffer;
    int size;

    __asm__ volatile("" : "=D"(buffer) : :);
    __asm__ volatile("" : "=S"(size) : :);

    for (int i = 0; i < size; ++i) {
        buffer[i] = 0xfe;
    }

    EXIT_INTRINSIC_IMPLEMENTATION();
}

int main(void) {
    volatile int size = 4096;
    volatile unsigned char buffer[size];

    InitIntrinsics();

    BeginInstrumentationBlock();
    EnableThreadInstrumentation();

    CALL_INTRINSIC(MemsetImpl, , "D"(buffer), "S"(size));

    // More instructions to fill the bottom of the trace, so we guarantee
    // simulation of the intrinsic.
    for (int i = 0; i < 2; ++i) {
        if (buffer[i] != 0xfe) {
            return 1;
        }
    }

    DisableThreadInstrumentation();
    EndInstrumentationBlock();

    DeInitIntrinsics();

    return 0;
}
