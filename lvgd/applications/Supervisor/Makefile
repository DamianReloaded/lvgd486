
TARGET_NAME := Supervisor
MODE ?= release
PLATFORM ?= linux

MAKEFILE_DIR := $(dir $(abspath $(lastword $(MAKEFILE_LIST))))
PRJ_DIR := $(abspath $(MAKEFILE_DIR)../..)

ifeq ($(PLATFORM),linux)
    CXX := /usr/local/gcc-5.4.0/bin/g++
    STRIP := strip
else
    $(error Unsupported platform: $(PLATFORM))
endif

BIN_DIR := /games/lvgd
OBJ_DIR := $(PRJ_DIR)/bin/$(PLATFORM)/$(MODE)/obj/$(TARGET_NAME)

TARGET := $(BIN_DIR)/$(TARGET_NAME)
SOURCE := $(MAKEFILE_DIR)Supervisor.cpp
OBJECT := $(OBJ_DIR)/Supervisor.o

CXXFLAGS := -Os -m32 -march=i486 -mtune=i486
CXXFLAGS += -ffreestanding -fno-builtin
CXXFLAGS += -fno-exceptions -fno-rtti
CXXFLAGS += -fno-stack-protector
CXXFLAGS += -fno-pic -fno-pie
CXXFLAGS += -fno-unwind-tables -fno-asynchronous-unwind-tables
CXXFLAGS += -ffunction-sections -fdata-sections

LDFLAGS := -m32 -nostdlib -static
LDFLAGS += -Wl,-e,_start
LDFLAGS += -Wl,--gc-sections

ifeq ($(MODE),release)
    CXXFLAGS += -DNDEBUG
else ifeq ($(MODE),debug)
    CXXFLAGS += -O0 -g -DDEBUG
else
    $(error Unsupported build mode: $(MODE))
endif

.PHONY: all clean run rebuild

all: $(TARGET)

$(TARGET): $(OBJECT)
	@mkdir -p "$(@D)"
	$(CXX) $(LDFLAGS) -o "$@" $^
	$(STRIP) --strip-all "$@"

$(OBJECT): $(SOURCE)
	@mkdir -p "$(@D)"
	$(CXX) $(CXXFLAGS) -c -o "$@" "$<"

clean:
	rm -rf "$(OBJ_DIR)"
	rm -f "$(TARGET)"

run: $(TARGET)
	"$(TARGET)" /games/doom/doom

rebuild:
	$(MAKE) clean MODE=$(MODE) PLATFORM=$(PLATFORM)
	$(MAKE) all MODE=$(MODE) PLATFORM=$(PLATFORM)
