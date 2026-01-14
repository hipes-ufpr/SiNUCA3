#include <assert.h>
#include <instrumentation_control.h>

DEFINE_INTRINSIC(Factorial) {
    long value;
    GetParameterGPR(1, &value);

    for (int i = value - 1; i > 0; --i) value *= i;

    SetReturnGPR(0, &value);
}

int main(void) {
    InitIntrinsics();

    BeginInstrumentationBlock();
    volatile int ret;

    __asm__ volatile(CALL_INTRINSIC_TEMPLATE(Factorial) : "=a"(ret) : "b"(5) :);

    assert(ret == 120);

    EndInstrumentationBlock();

    DeInitIntrinsics();

    return 0;
}
