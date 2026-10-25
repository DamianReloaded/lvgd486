#!/bin/sh

# ============================================================
# LVGD486 - Build KBD (Static)
# Script: 25-build-kbd.sh
#
# Compile KBD 1.12 from a clean build directory.
#
# Source directory:
#   /home/user/sources/kbd
#
# Build directory:
#   /home/user/sources/build/kbd
#
# All executable outputs must be statically linked.
# Installation is handled by a separate script.
# ============================================================

set -e

# ============================================================
# Constants
# ============================================================

SOURCES_DIR="/home/user/sources"
SOURCE_DIR="${SOURCES_DIR}/kbd"
BUILD_ROOT="${SOURCES_DIR}/build"
BUILD_DIR="${BUILD_ROOT}/kbd"

KBD_VERSION="1.12"

LOADKEYS_EXECUTABLE="${BUILD_DIR}/src/loadkeys"
DUMPKEYS_EXECUTABLE="${BUILD_DIR}/src/dumpkeys"

# ============================================================
# Compiler flags
# ============================================================

CFLAGS="${CFLAGS:--O2}"
LDFLAGS="-static -s"
CC="${CC:-gcc}"

export CFLAGS
export LDFLAGS

# ============================================================
# Error handling
# ============================================================

fail()
{
    echo "ERROR: $*" >&2
    exit 1
}

# ============================================================
# Verify permissions
# ============================================================

[ "$(id -u)" -eq 0 ] ||
    fail "Run this script as root to build KBD."

# ============================================================
# Verify expected paths
# ============================================================

[ "$SOURCE_DIR" = "/home/user/sources/kbd" ] ||
    fail "Unexpected source directory: ${SOURCE_DIR}"

[ "$BUILD_ROOT" = "/home/user/sources/build" ] ||
    fail "Unexpected build root: ${BUILD_ROOT}"

[ "$BUILD_DIR" = "/home/user/sources/build/kbd" ] ||
    fail "Unexpected build directory: ${BUILD_DIR}"

# ============================================================
# Verify required tools
# ============================================================

for tool in make gcc cp find file grep readelf rm mkdir getconf
do
    command -v "$tool" >/dev/null 2>&1 ||
        fail "Required build tool not found: ${tool}"
done

command -v "$CC" >/dev/null 2>&1 ||
    fail "Configured C compiler not found: ${CC}"

# ============================================================
# Verify source tree
# ============================================================

echo "============================================================"
echo "Checking KBD source tree"
echo "============================================================"

[ -d "$SOURCE_DIR" ] ||
    fail "KBD source directory not found: ${SOURCE_DIR}"

[ -f "${SOURCE_DIR}/configure" ] ||
    fail "KBD configure script not found. Run 11-extract-sources.sh first."

[ -f "${SOURCE_DIR}/Makefile.in" ] ||
    fail "KBD Makefile.in not found."

[ -d "${SOURCE_DIR}/src" ] ||
    fail "KBD src directory not found."

echo "KBD version: ${KBD_VERSION}"
echo "Source directory: ${SOURCE_DIR}"
echo "Build directory: ${BUILD_DIR}"
echo "Compiler: ${CC}"
echo "CFLAGS: ${CFLAGS}"
echo "LDFLAGS: ${LDFLAGS}"

# ============================================================
# Detect available processors
# ============================================================

JOBS=$(getconf _NPROCESSORS_ONLN 2>/dev/null || true)

case "$JOBS" in
    ''|*[!0-9]*|0)
        JOBS=$(grep -c '^processor[[:space:]]*:' \
            /proc/cpuinfo 2>/dev/null || true)
        ;;
esac

case "$JOBS" in
    ''|*[!0-9]*|0)
        JOBS=1
        ;;
esac

echo "Parallel build jobs: ${JOBS}"

# ============================================================
# Clean previous build completely
# ============================================================

echo
echo "============================================================"
echo "Cleaning previous KBD build"
echo "============================================================"

mkdir -p "$BUILD_ROOT"

[ "$BUILD_ROOT" = "/home/user/sources/build" ] ||
    fail "Unexpected build root: ${BUILD_ROOT}"

[ "$BUILD_DIR" = "${BUILD_ROOT}/kbd" ] ||
    fail "Unsafe build directory: ${BUILD_DIR}"

if [ -e "$BUILD_DIR" ] || [ -L "$BUILD_DIR" ]; then
    echo "Removing: ${BUILD_DIR}"

    rm -rf "$BUILD_DIR" ||
        fail "Could not remove previous build directory."
fi

[ ! -e "$BUILD_DIR" ] && [ ! -L "$BUILD_DIR" ] ||
    fail "Previous build directory still exists."

mkdir -p "$BUILD_DIR" ||
    fail "Could not create fresh build directory."

echo "Fresh build directory created."

# ============================================================
# Copy pristine source tree
# ============================================================

echo
echo "============================================================"
echo "Copying KBD source tree"
echo "============================================================"

cp -R "${SOURCE_DIR}/." "$BUILD_DIR/" ||
    fail "Could not copy KBD source tree."

# ============================================================
# Configure
# ============================================================

echo
echo "============================================================"
echo "Configuring KBD ${KBD_VERSION}"
echo "============================================================"

cd "$BUILD_DIR"

CC="$CC" \
CFLAGS="$CFLAGS" \
LDFLAGS="$LDFLAGS" \
./configure \
    --prefix=/usr \
    --datadir=/usr/share \
    --mandir=/usr/share/man \
    --disable-nls ||
    fail "KBD configuration failed."

# ============================================================
# Build
#
# Pass variables directly to make. KBD 1.12's Makefiles
# assign their own default CFLAGS, LDFLAGS, and CC values,
# which can override environment variables.
#
# Command-line make assignments ensure the static linker
# flags are used for the executable link commands.
# ============================================================

echo
echo "============================================================"
echo "Building KBD ${KBD_VERSION}"
echo "============================================================"

make -j"$JOBS" \
    CC="$CC" \
    CFLAGS="$CFLAGS" \
    LDFLAGS="$LDFLAGS" ||
    fail "KBD compilation failed."

# ============================================================
# Verify required executables
# ============================================================

echo
echo "============================================================"
echo "Verifying required build outputs"
echo "============================================================"

[ -s "$LOADKEYS_EXECUTABLE" ] ||
    fail "loadkeys executable was not produced: ${LOADKEYS_EXECUTABLE}"

[ -x "$LOADKEYS_EXECUTABLE" ] ||
    fail "loadkeys is not executable."

[ -s "$DUMPKEYS_EXECUTABLE" ] ||
    fail "dumpkeys executable was not produced: ${DUMPKEYS_EXECUTABLE}"

[ -x "$DUMPKEYS_EXECUTABLE" ] ||
    fail "dumpkeys is not executable."

# ============================================================
# Discover executable ELF files
# ============================================================

echo
echo "============================================================"
echo "Discovering executable outputs"
echo "============================================================"

EXECUTABLE_LIST="${BUILD_DIR}/.static-executables"
EXECUTABLE_TEMP="${BUILD_DIR}/.static-executables.tmp"

: > "$EXECUTABLE_TEMP"

find "${BUILD_DIR}/src" -type f -perm -111 -print |
while IFS= read -r executable
do
    FILE_OUTPUT=$(file "$executable" 2>/dev/null || true)

    case "$FILE_OUTPUT" in
        *"ELF "*)
            echo "$executable"
            ;;
    esac
done > "$EXECUTABLE_TEMP"

mv "$EXECUTABLE_TEMP" "$EXECUTABLE_LIST"

[ -s "$EXECUTABLE_LIST" ] ||
    fail "No executable ELF files found under ${BUILD_DIR}/src."

grep -Fx "$LOADKEYS_EXECUTABLE" "$EXECUTABLE_LIST" >/dev/null 2>&1 ||
    fail "loadkeys was not included in the executable list."

grep -Fx "$DUMPKEYS_EXECUTABLE" "$EXECUTABLE_LIST" >/dev/null 2>&1 ||
    fail "dumpkeys was not included in the executable list."

cat "$EXECUTABLE_LIST"

# ============================================================
# Verify static linkage of every executable
# ============================================================

echo
echo "============================================================"
echo "Verifying static linkage"
echo "============================================================"

while IFS= read -r executable
do
    [ -n "$executable" ] || continue

    echo
    echo "Checking: ${executable}"

    FILE_OUTPUT=$(file "$executable" 2>&1) ||
        fail "Could not inspect ${executable}"

    echo "$FILE_OUTPUT"

    echo "$FILE_OUTPUT" |
        grep -q 'statically linked' ||
        fail "${executable} is not confirmed statically linked."

    if readelf -l "$executable" 2>/dev/null |
        grep -q 'INTERP'; then
        fail "${executable} has a dynamic ELF interpreter."
    fi

    if readelf -d "$executable" 2>/dev/null |
        grep -q 'NEEDED'; then
        fail "${executable} has dynamic library dependencies."
    fi

    echo "Static linkage confirmed."

done < "$EXECUTABLE_LIST"

# ============================================================
# Remove temporary verification files
# ============================================================

rm -f "$EXECUTABLE_LIST" "$EXECUTABLE_TEMP"

# ============================================================
# Final report
# ============================================================

echo
echo "============================================================"
echo "KBD ${KBD_VERSION} build completed successfully"
echo "============================================================"

echo "Build directory: ${BUILD_DIR}"
echo "loadkeys: ${LOADKEYS_EXECUTABLE}"
echo "dumpkeys: ${DUMPKEYS_EXECUTABLE}"
echo "Parallel jobs: ${JOBS}"
echo "Link mode: static"
echo
echo "Build started from a clean directory."
echo "All discovered executable ELF outputs passed static-link checks."
echo "No installation was performed."
echo "The host installation and distro staging tree were not modified."