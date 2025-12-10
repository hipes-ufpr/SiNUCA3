#include <instrumentation_control.h>

int __attribute__((noinline)) IterativeFactorial(int x) {
    if (x == 0 || x == 1) {
        return 1;
    }

    int res = 1;
    for (int i = 2; i < x; ++i) {
        res *= i;
    }

    return res;
}

int __attribute__((noinline)) RecursiveFactorial(int x) {
    if (x == 0) {
        return 1;
    }

    return x * RecursiveFactorial(x - 1);
}

int main(void) {
    volatile int a = 5;
    volatile int b = 5;

    BeginInstrumentationBlock();

    a = IterativeFactorial(a);
    b = RecursiveFactorial(b);

    EndInstrumentationBlock();

    return 0;
}