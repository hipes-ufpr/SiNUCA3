#include <instrumentation_control.h>

#define n 3

// Naive matrix multiplication algorithm
void MatrixMultiply(int A[][n], int B[][n], int C[][n]) {
    for (int i = 0; i < n; ++i) {
        for (int j = 0; j < n; ++j) {
            C[i][j] = 0;
            for (int k = 0; k < n; ++k) {
                C[i][j] += A[i][k] * B[k][j];
            }
        }
    }
}

int main(void) {
    int A[][n] = {{1, 2, 3},
                   {4, 5, 6},
                   {7, 8, 9}};
    int B[][n] = {{9, 8, 7},
                   {6, 5, 4},
                   {3, 2, 1}};
    int C[n][n];

    BeginInstrumentationBlock();

    MatrixMultiply(A, B, C);

    EndInstrumentationBlock();

    return 0;
}