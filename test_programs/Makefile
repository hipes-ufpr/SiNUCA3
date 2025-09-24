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

CFLAGS = -Wall -Wextra -std=c11 -O3 -g -I../x86_trace_generator/ -no-pie -fno-omit-frame-pointer

all: factorials intrinsics

%: ../x86_trace_generator/instrumentation_control.c %.c
	$(CC) $(CFLAGS) -o $@ $^
