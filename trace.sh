#!/bin/bash

# You can use this script to generate a trace using our x86 trace generator (with pin).
# Maybe in the future, the sinuca3 executable will be able to function as a wraper for this.
# For now, I am just using this to not have to remember khow to use it.
# All traces will be genereted in ./trace/

# Usage:
# ./trace.sh <program to trace>

PINTOOL_PATH=./x86_trace_generator
PINTOOL_SO_PATH=$PINTOOL_PATH/obj-intel64/sinuca3_pintool.so

if ! test -f $PINTOOL_SO_PATH; then
  echo -e "Pintool is not compiled yet. Compiling...\n\n"
  make --directory=$PINTOOL_PATH
  echo -e "\n\n-----------------------------------------\n\n"
fi

./external/pin/pin -t $PINTOOL_SO_PATH -o ./trace/ -- $@
