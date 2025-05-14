Pintool compile command:
```
make
```

To use the trace generator, you need to load the program via the Intel Pin.
Example command:

./pin/pin -t ./obj_intel64/*.so -trace_folder "..." -- ./program_to_trace
