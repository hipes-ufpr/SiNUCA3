CC = clang
CPP = clang++

CFLAGS = -Wall -Wextra -std=c99
CPPFLAGS = -Wall -Wextra -fno-exceptions -std=c++98

C_RELEASE_FLAGS = -O3 -march=native -DNDEBUG
CPP_RELEASE_FLAGS = $(C_RELEASE_FLAGS)

C_DEBUG_FLAGS = -Og -g
CPP_DEBUG_FLAGS = $(C_DEBUG_FLAGS)

TARGET = sinuca3
