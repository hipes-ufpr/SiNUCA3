Pintool compile command:
```
make
```

To use the trace generator, you need to load the program via the Intel Pin.

Example command:
./pin/pin -t ./obj-intel64/my_pintool.so -o my_dir -n my_max_inst -- ./my_program

Change the instances beginning with 'my' to your own values.

Options:
-f: Force instrumentation even when no blocks are defined.
-o: Output directory.
-n: Maximum number of instructions to add to trace.