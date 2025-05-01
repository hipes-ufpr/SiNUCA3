CC = clang
CPP = clang++

CFLAGS = -Wall -Wextra -std=c99
CPPFLAGS = -Wall -Wextra -fno-exceptions -std=c++98

ifdef SINUCA3_TRACEABLE
CPPFLAGS += -DSINUCA3_TRACEABLE
# So clang does not optimize away the useless function calls.
C_DEBUG_FLAGS = -O0 -gdwarf-2
else
C_DEBUG_FLAGS = -Og -gdwarf-2
endif

CPP_DEBUG_FLAGS = $(C_DEBUG_FLAGS)

C_RELEASE_FLAGS = -O3 -march=native -DNDEBUG
CPP_RELEASE_FLAGS = $(C_RELEASE_FLAGS)

LD_FLAGS = -lyaml -lz

TARGET = sinuca3
