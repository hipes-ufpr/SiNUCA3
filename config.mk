CC = clang
CPP = clang++

# The compiler will search for .h from this folder, so you dont need to use include with relative paths.
# Ex: include of src/engine/some.hpp from src/some/folder/exemple.cpp
#
# #include "../../engine/some.hpp"
#
# can become
#
# #include "engine/some.hpp"
# 
INCLUDE_DIR=$(SRC)

DEPFLAGS = -MMD -MP
CFLAGS = -Wall -Wextra -std=c99 $(DEPFLAGS) -I$(INCLUDE_DIR)
CPPFLAGS = -Wall -Wextra -fno-exceptions -std=c++98 $(DEPFLAGS) -I$(INCLUDE_DIR)

C_RELEASE_FLAGS = -O3 -march=native -DNDEBUG
CPP_RELEASE_FLAGS = $(C_RELEASE_FLAGS)

C_DEBUG_FLAGS = -Og -gdwarf-2
CPP_DEBUG_FLAGS = $(C_DEBUG_FLAGS)

LD_FLAGS = -lyaml -lz

TARGET = sinuca3
