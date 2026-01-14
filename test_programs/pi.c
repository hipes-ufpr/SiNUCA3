#include <instrumentation_control.h>
#include <omp.h>
#include <stdio.h>

static long num_steps = 10;

int main() {
    double step = 1.0 / (double)num_steps;
    double pi;

    BeginInstrumentationBlock();

#pragma omp parallel
    {
        double x;
        double sum = 0.0;
#pragma omp for
        for (int i = 0; i < num_steps; i++) {
            x = (i + 0.5) * step;
            sum += 4 / (1.0 + x * x);
        }

#pragma omp critical
        pi += step * sum;

#pragma omp barrier
    }

    EndInstrumentationBlock();

    printf("pi [%.20lf]\n", pi);
}