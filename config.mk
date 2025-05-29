CC = clang
CPP = clang++

DEPFLAGS = -MMD -MP
CFLAGS = -Wall -Wextra -std=c99 $(DEPFLAGS)
CPPFLAGS = -Wall -Wextra -fno-exceptions -std=c++98 $(DEPFLAGS)

C_RELEASE_FLAGS = -O3 -march=native -DNDEBUG
CPP_RELEASE_FLAGS = $(C_RELEASE_FLAGS)

C_DEBUG_FLAGS = -Og -gdwarf-2
CPP_DEBUG_FLAGS = $(C_DEBUG_FLAGS)

LD_FLAGS = -lyaml -lz

TARGET = sinuca3
