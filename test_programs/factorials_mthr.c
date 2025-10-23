#include <instrumentation_control.h>
#include <omp.h>

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
    omp_set_nested(1);

    int arr[6];

    BeginInstrumentationBlock();

#pragma omp parallel shared(arr) num_threads(2)
    {

#pragma omp for
        for (int i = 0; i < 6; ++i) {
            EnableThreadInstrumentation();
            arr[i] = IterativeFactorial(i);
            DisableThreadInstrumentation();
        }
    }

#pragma omp parallel num_threads(3)
    {
        volatile int b = 5;
#pragma omp critical(crit1)
        {
            EnableThreadInstrumentation();
            b = RecursiveFactorial(b);
            DisableThreadInstrumentation();
        }

#pragma omp barrier
    }

    EndInstrumentationBlock();

    return 0;
}