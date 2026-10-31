
# --------------------------------------------------
# Default build mode and platform
# --------------------------------------------------
TARGET_NAME := diagmouse
MODE ?= release
PLATFORM ?= linux
DEFINES :=

# --------------------------------------------------
# Resolve project root relative to this Makefile
# The project root is two directories above the Makefile
# --------------------------------------------------

MAKEFILE_DIR := $(dir $(abspath $(lastword $(MAKEFILE_LIST))))
PRJ_DIR := $(abspath $(MAKEFILE_DIR)../..)

# --------------------------------------------------
# Platform-specific settings
# --------------------------------------------------

ifeq ($(PLATFORM),Dos)
    CXX := i586-pc-msdosdjgpp-g++
    STRIP := i586-pc-msdosdjgpp-strip

else ifeq ($(PLATFORM),Windows)
    CXX := x86_64-w64-mingw32-g++
    STRIP := x86_64-w64-mingw32-strip
    DEFINES += _WIN32 WIN32 _WIN64 WIN64

else ifeq ($(PLATFORM),linux)
    CXX := /usr/local/gcc-5.4.0/bin/g++
    STRIP := strip

else
    $(error Unsupported platform: $(PLATFORM))
endif

# --------------------------------------------------
# Project directories
# --------------------------------------------------

SRC_DIRS := \
    $(PRJ_DIR)/examples/$(TARGET_NAME) \
    $(PRJ_DIR)/vgd \
    $(PRJ_DIR)/vgd/font

BIN_DIR := /games/lvgd/demos
OBJ_DIR := $(PRJ_DIR)/bin/$(PLATFORM)/$(MODE)/obj/$(TARGET_NAME)
TARGET := $(BIN_DIR)/$(TARGET_NAME)

# --------------------------------------------------
# Include directories
# --------------------------------------------------

INC_DIRS := $(SRC_DIRS)
CINCS := $(foreach DIR,$(INC_DIRS),-I"$(DIR)")

# --------------------------------------------------
# Compiler and linker flags
# --------------------------------------------------

CXXFLAGS := -Wall $(CINCS) -std=c++17 -Wextra
CXXFLAGS += -m32
CXXFLAGS += -march=i486
CXXFLAGS += -mtune=i486
CXXFLAGS += -fdata-sections
CXXFLAGS += -ffunction-sections
CXXFLAGS += -fno-exceptions
CXXFLAGS += -fno-rtti
CXXFLAGS += -fno-unwind-tables
CXXFLAGS += -fno-asynchronous-unwind-tables

LDFLAGS :=
LDFLAGS += -m32
LDFLAGS += -static -pthread -lrt

LIBRARIES :=
CLIBS := $(foreach LIB,$(LIBRARIES),-l$(LIB))

ifeq ($(MODE),release)
    CXXFLAGS += -O3 -DNDEBUG
    LDFLAGS += -s
    # Enable if supported and desired:
    # LDFLAGS += -Wl,--gc-sections

else ifeq ($(MODE),debug)
    CXXFLAGS += -g -O0
    DEFINES += DEBUG

else
    $(error Unsupported build mode: $(MODE))
endif

# --------------------------------------------------
# Discover source files recursively
# --------------------------------------------------

SRC := $(foreach DIR,$(SRC_DIRS),$(shell find "$(DIR)" -type f -name '*.cpp' 2>/dev/null))

ifeq ($(strip $(SRC)),)
    $(error No .cpp source files found in: $(SRC_DIRS))
endif

# --------------------------------------------------
# Map each source to a unique object path
# --------------------------------------------------

source_root = $(firstword $(foreach DIR,$(SRC_DIRS),$(if $(filter $(DIR)/%,$(1)),$(DIR))))

source_rel = $(patsubst $(call source_root,$(1))/%,%,$(1))

source_tag = $(notdir $(patsubst %/,%,$(call source_root,$(1))))

object_for = $(OBJ_DIR)/$(call source_tag,$(1))/$(call source_rel,$(1)).o

OBJ := $(foreach FILE,$(SRC),$(call object_for,$(FILE)))
DEP := $(OBJ:.o=.d)

# --------------------------------------------------
# Targets
# --------------------------------------------------

.PHONY: all program clean run rebuild

all: $(TARGET)

program: $(TARGET)

# --------------------------------------------------
# Link executable
# --------------------------------------------------

$(TARGET): $(OBJ)
	@mkdir -p "$(@D)"
	$(CXX) $(LDFLAGS) -o "$@" $^ $(CLIBS)

# --------------------------------------------------
# Compile source files
# --------------------------------------------------

define COMPILE_RULE
$(call object_for,$(1)): $(1)
	@mkdir -p "$$(@D)"
	$$(CXX) $$(CXXFLAGS) $$(foreach DEF,$$(DEFINES),-D$$(DEF)) -MMD -MP -c -o "$$@" "$$<"
endef

$(foreach FILE,$(SRC),$(eval $(call COMPILE_RULE,$(FILE))))

# --------------------------------------------------
# Clean build artifacts
# --------------------------------------------------

clean:
	rm -rf "$(BIN_DIR)/obj"
	rm -f "$(TARGET)"

# --------------------------------------------------
# Run executable
# --------------------------------------------------

run: $(TARGET)
	"$(TARGET)"

# --------------------------------------------------
# Rebuild from scratch
# --------------------------------------------------

rebuild:
	$(MAKE) clean MODE=$(MODE) PLATFORM=$(PLATFORM)
	$(MAKE) all MODE=$(MODE) PLATFORM=$(PLATFORM)

# --------------------------------------------------
# Include generated dependency files
# --------------------------------------------------

-include $(DEP)
