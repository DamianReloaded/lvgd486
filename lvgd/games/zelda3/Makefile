################################################################
#
# LVGD486 - Zelda3 Native
# Experimental native game backend
#
################################################################

# Build configuration: debug or release.
MODE ?= release

# Strict 32-bit i486 architecture.
ARCH_FLAGS = -m32 -march=i486 -mtune=i486

TOOLCHAIN = /usr/local/gcc-5.4.0/bin
TOOLCHAIN_ROOT = $(abspath $(TOOLCHAIN)/..)

CC = $(TOOLCHAIN)/gcc
CXX = $(TOOLCHAIN)/g++

# GCC version and target architecture.
GCC_VERSION = $(shell $(CXX) -dumpversion)
GCC_TARGET = $(shell $(CXX) -dumpmachine)

# C++ standard library directories.
GCC_INCLUDE = $(TOOLCHAIN_ROOT)/include/c++/$(GCC_VERSION)

CXX_INCLUDE_FLAGS = \
	-isystem $(GCC_INCLUDE) \
	-isystem $(GCC_INCLUDE)/$(GCC_TARGET) \
	-isystem $(GCC_INCLUDE)/backward

# Common compiler flags.
COMMON_CFLAGS = $(ARCH_FLAGS) -Wall -DLINUX -pthread
COMMON_CXXFLAGS = $(ARCH_FLAGS) -Wall -std=c++11 $(CXX_INCLUDE_FLAGS) -pthread

# Select build mode.
ifeq ($(MODE),debug)
	CFLAGS = $(COMMON_CFLAGS) -O0 -g -gdwarf-2 -w
	CXXFLAGS = $(COMMON_CXXFLAGS) -O0 -g -gdwarf-2 -w
	LDFLAGS = $(ARCH_FLAGS) -pthread -static -g
else ifeq ($(MODE),release)
	CFLAGS = $(COMMON_CFLAGS) -O2 -DNDEBUG -w
	CXXFLAGS = $(COMMON_CXXFLAGS) -O2 -DNDEBUG -w
	LDFLAGS = $(ARCH_FLAGS) -pthread -static -s
else
	$(error Invalid MODE '$(MODE)'. Use MODE=debug or MODE=release)
endif

# Include paths.
CPPFLAGS = -Isrc -Isnes -Ivgd -Ivgd/Display -Ivgd/Input -Ivgd/Util -Ivgd/Sound

# Libraries.
LIBS = -lm

# Object directory.
O = linux

# Source discovery.
C_SOURCES = $(shell find src snes vgd -type f -name '*.c' 2>/dev/null)
CXX_SOURCES = $(shell find src snes vgd -type f -name '*.cpp' 2>/dev/null)

# Object files.
C_OBJECTS = $(patsubst %.c,$(O)/%.o,$(C_SOURCES))
CXX_OBJECTS = $(patsubst %.cpp,$(O)/%.o,$(CXX_SOURCES))

OBJS = $(C_OBJECTS) $(CXX_OBJECTS)

# Default target.
all: $(O)/zelda

# Link executable.
$(O)/zelda: $(OBJS)
	$(CXX) $(LDFLAGS) $^ -o $@ $(LIBS)

# Compile C source files.
$(O)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

# Compile C++ source files.
$(O)/%.o: %.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c $< -o $@

# Display discovered sources.
sources:
	@echo "C sources:"
	@printf '%s\n' $(C_SOURCES)
	@echo
	@echo "C++ sources:"
	@printf '%s\n' $(CXX_SOURCES)

# Clean build files.
clean:
	rm -rf $(O)
	rm -f *~ *.o

.PHONY: all clean sources
