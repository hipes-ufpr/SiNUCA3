ifndef CC
  CC = clang
  ifeq "$(shell which clang)" ""
    CC = gcc
    ifeq "$(shell which gcc)" ""
      CC = cc
      ifeq "$(shell which cc)" ""
        $(error "Could not detect C compiler. Set one with CC or install clang, gcc or cc.")
      endif
    endif
  endif
endif

NOOPT  = -O0 -fno-inline -fno-builtin -fno-omit-frame-pointer
CFLAGS = -Wall -Wextra -std=c11 -O1 -g -I../x86_trace_generator/
ADD_FILES = ../x86_trace_generator/instrumentation_control.s

all: factorials intrinsics pi

factorials: factorials.c $(ADD_FILES)
	$(CC) $(CFLAGS) -o $@ $^

intrinsics: intrinsics.c $(ADD_FILES)
	$(CC) $(CFLAGS) -o $@ $^

pi: pi.c $(ADD_FILES)
	$(CC) -static $(CFLAGS) -o $@ $^ -L. -fopenmp

clean:
	-rm factorials intrinsics pi
