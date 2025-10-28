#include <instrumentation_control.h>
#include <omp.h>

#include <string.h>

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

void TestOneThread() {
#pragma omp parallel num_threads(1)
    {
        EnableThreadInstrumentation();
        int a = 10;
        a = IterativeFactorial(a);
        int b = 5;
        b = RecursiveFactorial(b);
        DisableThreadInstrumentation();
    }
}

void TestOmpBarrier() {
#pragma omp parallel
    {
        EnableThreadInstrumentation();
        int a = 10;
        a = IterativeFactorial(a);
#pragma omp barrier
        int b = 5;
        b = RecursiveFactorial(b);
        DisableThreadInstrumentation();
    }
}

void TestOmpGlobalCriticalBlock() {
#pragma omp parallel
    {
        EnableThreadInstrumentation();
        int a = 10;
        int b = 5;
#pragma omp critical
        {
            a = IterativeFactorial(a);
            b = RecursiveFactorial(b);
        }
        DisableThreadInstrumentation();
    }
}

void TestOmpNamedCriticalBlock() {
#pragma omp parallel
    {
        EnableThreadInstrumentation();
        int a = 10;
        int b = 5;
#pragma omp critical(crit)
        {
            a = IterativeFactorial(a);
            b = RecursiveFactorial(b);
        }
        DisableThreadInstrumentation();
    }
}

void TestOmpLock() {}

void TestOmpSingleBlock() {
#pragma omp parallel
    {
        EnableThreadInstrumentation();
        int a = 10;
        int b = 5;
#pragma omp single
        {
            a = IterativeFactorial(a);
            b = RecursiveFactorial(b);
        }
        DisableThreadInstrumentation();
    }
}

void TestOmpMasterBlock() {}

#define TEST_SIZE 7

void TestOmpFor() {
    int arr[TEST_SIZE];

#pragma omp parallel shared(arr)
    {
        EnableThreadInstrumentation();
#pragma omp for
        for (int i = 0; i < TEST_SIZE; ++i) {
            arr[i] = IterativeFactorial(i);
        }
        DisableThreadInstrumentation();
    }
}

#define TEST(func)                      \
    if (!strcmp(test, #func)) {         \
        func();                         \
    }

int main(int argc, char* argv[]) {
    if (argc < 2) {
        return 1;
    }

    char* test = argv[1];

    BeginInstrumentationBlock();

    TEST(TestOneThread)
    TEST(TestOmpGlobalCriticalBlock)
    TEST(TestOmpNamedCriticalBlock)
    TEST(TestOmpSingleBlock)
    TEST(TestOmpMasterBlock)
    TEST(TestOmpBarrier)
    TEST(TestOmpFor)

    EndInstrumentationBlock();

    return 0;
}