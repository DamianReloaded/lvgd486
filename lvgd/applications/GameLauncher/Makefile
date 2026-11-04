
# --------------------------------------------------
# Default build mode and platform
# --------------------------------------------------

TARGET_NAME := GameLauncher
MODE ?= release
PLATFORM ?= linux
DEFINES :=

# --------------------------------------------------
# Resolve project root relative to this Makefile
# --------------------------------------------------

MAKEFILE_PATH := $(abspath $(lastword $(MAKEFILE_LIST)))
MAKEFILE_DIR := $(patsubst %/,%,$(dir $(MAKEFILE_PATH)))
PRJ_DIR := $(abspath $(MAKEFILE_DIR)/../..)

# --------------------------------------------------
# Application directory
# --------------------------------------------------

APP_DIR := $(MAKEFILE_DIR)

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
    $(APP_DIR) \
    $(PRJ_DIR)/vgd \
    $(PRJ_DIR)/vgd/font

BIN_DIR := /games/lvgd
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

LDFLAGS := -m32 -static -pthread

LIBRARIES := rt
CLIBS := $(foreach LIB,$(LIBRARIES),-l$(LIB))

ifeq ($(MODE),release)
    CXXFLAGS += -O3 -DNDEBUG
    LDFLAGS += -s

else ifeq ($(MODE),debug)
    CXXFLAGS += -g -O0
    DEFINES += DEBUG

else
    $(error Unsupported build mode: $(MODE))
endif

# --------------------------------------------------
# Discover source files recursively
# --------------------------------------------------

MAIN_SRC := $(APP_DIR)/GameLauncher.cpp

SRC := $(sort $(foreach DIR,$(SRC_DIRS),$(shell find "$(DIR)" -type f -name '*.cpp' 2>/dev/null)))

ifeq ($(wildcard $(MAIN_SRC)),)
    $(error Application entry point not found: $(MAIN_SRC))
endif

ifeq ($(strip $(SRC)),)
    $(error No .cpp source files found in: $(SRC_DIRS))
endif

# --------------------------------------------------
# Map each source to a unique object path
# --------------------------------------------------

source_root = $(firstword $(foreach DIR,$(SRC_DIRS),$(if $(filter $(DIR)/%,$(1)),$(DIR))))

source_rel = $(patsubst $(call source_root,$(1))/%,%,$(1))

source_tag = $(notdir $(call source_root,$(1)))

object_for = $(OBJ_DIR)/$(call source_tag,$(1))/$(call source_rel,$(1)).o

dependency_for = $(patsubst %.o,%.d,$(call object_for,$(1)))

OBJ := $(foreach FILE,$(SRC),$(call object_for,$(FILE)))
DEP := $(foreach FILE,$(SRC),$(call dependency_for,$(FILE)))

# --------------------------------------------------
# Default targets
# --------------------------------------------------

.PHONY: all program clean run rebuild

all: $(TARGET)

program: $(TARGET)

# --------------------------------------------------
# Generate dependency files
# --------------------------------------------------

define DEPENDENCY_RULE
$(call dependency_for,$(1)): $(1) $(MAKEFILE_PATH)
	@mkdir -p "$$(@D)"
	$$(CXX) $$(CXXFLAGS) $$(foreach DEF,$$(DEFINES),-D$$(DEF)) -MM -MP -MT "$(call object_for,$(1)) $(call dependency_for,$(1))" -MF "$$@" "$$<"
endef

$(foreach FILE,$(SRC),$(eval $(call DEPENDENCY_RULE,$(FILE))))

# --------------------------------------------------
# Compile source files
# --------------------------------------------------

define COMPILE_RULE
$(call object_for,$(1)): $(1) $(call dependency_for,$(1)) $(MAKEFILE_PATH)
	@mkdir -p "$$(@D)"
	$$(CXX) $$(CXXFLAGS) $$(foreach DEF,$$(DEFINES),-D$$(DEF)) -c -o "$$@" "$$<"
endef

$(foreach FILE,$(SRC),$(eval $(call COMPILE_RULE,$(FILE))))

# --------------------------------------------------
# Link executable
# --------------------------------------------------

$(TARGET): $(OBJ)
	@mkdir -p "$(@D)"
	$(CXX) $(LDFLAGS) -o "$@" $(OBJ) $(CLIBS)

# --------------------------------------------------
# Clean build artifacts
# --------------------------------------------------

clean:
	rm -rf "$(OBJ_DIR)"
	rm -f "$(TARGET)"

# --------------------------------------------------
# Run executable
# --------------------------------------------------

run: $(TARGET)
	cd "$(APP_DIR)" && "$(TARGET)"

# --------------------------------------------------
# Rebuild from scratch
# --------------------------------------------------

rebuild:
	$(MAKE) -f "$(MAKEFILE_PATH)" clean MODE=$(MODE) PLATFORM=$(PLATFORM)
	$(MAKE) -f "$(MAKEFILE_PATH)" all MODE=$(MODE) PLATFORM=$(PLATFORM)

# --------------------------------------------------
# Include generated dependency files
# --------------------------------------------------

ifneq ($(MAKECMDGOALS),clean)
-include $(DEP)
endif
