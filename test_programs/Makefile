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
CFLAGS = -Wall -Wextra -std=c99 -O3 -I../x86_trace_generator/ -g
OPENMP = -static -fopenmp
SRCS = $(shell find *.c)
EXEC = $(SRCS:.c=)

all: $(EXEC)

%_mthr: %_mthr.c
	$(CC) $(CFLAGS) -o $@ $^ $(NOOPT) $(OPENMP)

%_seq: %_seq.c
	$(CC) $(CFLAGS) -o $@ $^ $(NOOPT)

clean:
	rm -f $(EXEC)
