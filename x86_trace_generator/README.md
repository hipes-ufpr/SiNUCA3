Pintool compile command:
```
make
```

To use the trace generator, you need to load the program via the Intel Pin.
Example command:

./pin/pin -t ./obj_intel64/*.so -o {directory} -- ./program_to_trace
