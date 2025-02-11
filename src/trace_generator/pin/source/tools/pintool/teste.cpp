void trace_start() {}

void trace_stop() {}

int main() {
    trace_start();
    int a = 1;
    int b = 2;
    int c = a + b;
    trace_stop();

    return 0;
}