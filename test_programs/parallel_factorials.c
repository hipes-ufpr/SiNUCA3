#include <omp.h>
#include <instrumentation_control.h>

int __attribute__((noinline)) IterativeFactorial(int x) {
    if (x <= 1) {
        return 1;
    }

    int res = 1;
    for (int i = 2; i < x; ++i) {
        res *= i;
    }

    return res;
}

int main(void) {
    volatile int a = 5;
    volatile int b = 5;

    BeginInstrumentationBlock();
    EnableThreadInstrumentation();

    a = IterativeFactorial(a);

    DisableThreadInstrumentation();
    EndInstrumentationBlock();

    return 0;
}
