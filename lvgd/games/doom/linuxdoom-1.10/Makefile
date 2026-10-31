################################################################
#
# LVGD486 - LinuxDoom 1.10
# Native Linux VGA backend
#
################################################################

# Build configuration: debug or release.
MODE ?= release

# Strict 32-bit i386 architecture.
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
COMMON_CFLAGS = $(ARCH_FLAGS) -Wall -DNORMALUNIX -DLINUX -pthread
COMMON_CXXFLAGS = $(ARCH_FLAGS) -Wall -std=c++11 $(CXX_INCLUDE_FLAGS) -pthread

# Select build mode.
ifeq ($(MODE),debug)
	CFLAGS = $(COMMON_CFLAGS) -O0 -g -gdwarf-2
	CXXFLAGS = $(COMMON_CXXFLAGS) -O0 -g -gdwarf-2
	LDFLAGS = $(ARCH_FLAGS) -pthread -static -g
else ifeq ($(MODE),release)
	CFLAGS = $(COMMON_CFLAGS) -O2 -DNDEBUG
	CXXFLAGS = $(COMMON_CXXFLAGS) -O2 -DNDEBUG
	LDFLAGS = $(ARCH_FLAGS) -pthread -static -s
else
	$(error Invalid MODE '$(MODE)'. Use MODE=debug or MODE=release)
endif

CPPFLAGS = -Ivgd/Display -Ivgd/Input -Ivgd/Util -Ivgd/Sound

# Custom allocator is enabled only in Memory.cpp.
MEMORY_CPPFLAGS = -DCUSTOM_DOS_ALLOCATOR

LIBS = -lm

# Object directory.
O = linux

# Object files.
OBJS = \
	$(O)/doomdef.o \
	$(O)/doomstat.o \
	$(O)/dstrings.o \
	$(O)/i_system.o \
	$(O)/i_sound.o \
	$(O)/i_video.o \
	$(O)/DisplayMode13h.o \
	$(O)/Keyboard.o \
	$(O)/Mouse.o \
	$(O)/Joystick.o \
	$(O)/Memory.o \
	$(O)/Sound.o \
	$(O)/AudioMixer.o \
	$(O)/Midi.o \
	$(O)/MidiPcm.o \
	$(O)/Synthesizer.o \
	$(O)/MidiSong.o \
	$(O)/MidiPlayer.o \
	$(O)/MidiFile.o \
	$(O)/MusFile.o \
	$(O)/i_net.o \
	$(O)/tables.o \
	$(O)/f_finale.o \
	$(O)/f_wipe.o \
	$(O)/d_main.o \
	$(O)/d_net.o \
	$(O)/d_items.o \
	$(O)/g_game.o \
	$(O)/m_menu.o \
	$(O)/m_misc.o \
	$(O)/m_argv.o \
	$(O)/m_bbox.o \
	$(O)/m_fixed.o \
	$(O)/m_swap.o \
	$(O)/m_cheat.o \
	$(O)/m_random.o \
	$(O)/am_map.o \
	$(O)/p_ceilng.o \
	$(O)/p_doors.o \
	$(O)/p_enemy.o \
	$(O)/p_floor.o \
	$(O)/p_inter.o \
	$(O)/p_lights.o \
	$(O)/p_map.o \
	$(O)/p_maputl.o \
	$(O)/p_plats.o \
	$(O)/p_pspr.o \
	$(O)/p_setup.o \
	$(O)/p_sight.o \
	$(O)/p_spec.o \
	$(O)/p_switch.o \
	$(O)/p_mobj.o \
	$(O)/p_telept.o \
	$(O)/p_tick.o \
	$(O)/p_saveg.o \
	$(O)/p_user.o \
	$(O)/r_bsp.o \
	$(O)/r_data.o \
	$(O)/r_draw.o \
	$(O)/r_main.o \
	$(O)/r_plane.o \
	$(O)/r_segs.o \
	$(O)/r_sky.o \
	$(O)/r_things.o \
	$(O)/w_wad.o \
	$(O)/wi_stuff.o \
	$(O)/v_video.o \
	$(O)/st_lib.o \
	$(O)/st_stuff.o \
	$(O)/hu_stuff.o \
	$(O)/hu_lib.o \
	$(O)/s_sound.o \
	$(O)/z_zone.o \
	$(O)/info.o \
	$(O)/sounds.o

# Default target.
all: $(O)/doom

# Create object directory.
$(O):
	mkdir -p $(O)

# Link executable using the selected C++ linker.
$(O)/doom: $(OBJS) $(O)/i_main.o
	$(CXX) $(LDFLAGS) $^ -o $@ $(LIBS)

# Compile C source files.
$(O)/%.o: %.c | $(O)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

# Compile C++ video backend.
$(O)/i_video.o: i_video.cpp | $(O)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c $< -o $@

# Compile C++ sound backend.
$(O)/i_sound.o: i_sound.cpp | $(O)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c $< -o $@

# Compile VGA display implementation.
$(O)/DisplayMode13h.o: vgd/Display/DisplayMode13h.cpp | $(O)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c $< -o $@

# Compile keyboard implementation.
$(O)/Keyboard.o: vgd/Input/Keyboard.cpp | $(O)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c $< -o $@

# Compile mouse implementation.
$(O)/Mouse.o: vgd/Input/Mouse.cpp | $(O)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c $< -o $@

# Compile joystick implementation.
$(O)/Joystick.o: vgd/Input/Joystick.cpp | $(O)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c $< -o $@

# Compile custom memory implementation.
$(O)/Memory.o: vgd/Util/Memory.cpp | $(O)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(MEMORY_CPPFLAGS) -c $< -o $@

# Compile Sound implementation.
$(O)/Sound.o: vgd/Sound/Sound.cpp | $(O)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c $< -o $@

# Compile AudioMixer implementation.
$(O)/AudioMixer.o: vgd/Sound/AudioMixer.cpp | $(O)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c $< -o $@

# Compile MIDI output implementation.
$(O)/Midi.o: vgd/Sound/Midi.cpp | $(O)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c $< -o $@

# Compile software MIDI implementation.
$(O)/MidiPcm.o: vgd/Sound/MidiPcm.cpp | $(O)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c $< -o $@

# Compile software Synthesizer
$(O)/Synthesizer.o: vgd/Sound/Synthesizer.cpp | $(O)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c $< -o $@

# Compile MIDI song implementation.
$(O)/MidiSong.o: vgd/Sound/MidiSong.cpp | $(O)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c $< -o $@

# Compile MIDI player implementation.
$(O)/MidiPlayer.o: vgd/Sound/MidiPlayer.cpp | $(O)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c $< -o $@

# Compile Standard MIDI File implementation.
$(O)/MidiFile.o: vgd/Sound/MidiFile.cpp | $(O)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c $< -o $@

# Compile MUS file implementation.
$(O)/MusFile.o: vgd/Sound/MusFile.cpp | $(O)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c $< -o $@

# Verify C++ standard library headers and String.hpp.
check-limits:
	@echo "Checking GCC $(GCC_VERSION) C++ headers..."
	@printf '#include <limits>\n#include "String.hpp"\nint main() { return 0; }\n' | \
		$(CXX) $(CPPFLAGS) $(CXXFLAGS) -x c++ -fsyntax-only -
	@echo "C++ headers and String.hpp found successfully."

# Clean build files.
clean:
	rm -f *.o *~ *.flc
	rm -f $(O)/*.o $(O)/doom

.PHONY: all clean check-limits
