CC = clang
CPP = clang++

CFLAGS = -Wall -Wextra -std=c99 -I./src
CPPFLAGS = -Wall -Wextra -fno-exceptions -std=c++98 -I./src

C_RELEASE_FLAGS = -O3 -march=native -DNDEBUG
CPP_RELEASE_FLAGS = $(C_RELEASE_FLAGS)

C_DEBUG_FLAGS = -O0 -g -gdwarf-2
CPP_DEBUG_FLAGS = $(C_DEBUG_FLAGS)

LD_FLAGS = -lyaml -lz

TARGET = sinuca3
