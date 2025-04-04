Pintool compile command:
```
make
```

To use the trace generator, you need to load the program via the Intel Pin.
Example command (from trace_generator folder):

./pin/pin -t ./obj_intel64/*.so -- ./program_to_trace
