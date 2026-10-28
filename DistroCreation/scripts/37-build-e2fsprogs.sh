#!/bin/sh

# ============================================================
# LVGD486 - Build e2fsprogs (Static, i386)
#
# Compile e2fsprogs 1.37 using gcc-3.3 for i386.
#
# Required compiler in PATH:
#   gcc-3.3
#
# Target architecture:
#   i386 (32-bit x86)
#
# Source directory:
#   /home/user/sources/e2fsprogs
#
# Build directory:
#   /home/user/sources/build/e2fsprogs
#
# Required utilities:
#   mke2fs
#   e2fsck
#   tune2fs
#   dumpe2fs
#   resize2fs
#
# All executable ELF outputs must be statically linked.
# Installation is handled by a separate script.
# ============================================================

set -e

# ============================================================
# Constants
# ============================================================

SOURCES_DIR="/home/user/sources"
SOURCE_DIR="${SOURCES_DIR}/e2fsprogs"
BUILD_ROOT="${SOURCES_DIR}/build"
BUILD_DIR="${BUILD_ROOT}/e2fsprogs"

E2FSPROGS_VERSION="1.37"

CC="gcc-3.3"
EXPECTED_GCC_VERSION="3.3"
TARGET_ARCH="i386"

MKE2FS_EXECUTABLE="${BUILD_DIR}/misc/mke2fs"
E2FSCK_EXECUTABLE="${BUILD_DIR}/e2fsck/e2fsck"
TUNE2FS_EXECUTABLE="${BUILD_DIR}/misc/tune2fs"
DUMPE2FS_EXECUTABLE="${BUILD_DIR}/misc/dumpe2fs"
RESIZE2FS_EXECUTABLE="${BUILD_DIR}/resize/resize2fs"

# ============================================================
# Error handling
# ============================================================

fail()
{
    echo "ERROR: $*" >&2
    exit 1
}

# ============================================================
# Compiler flags
#
# -m32        Generate 32-bit code.
# -march=i386 Target the original Intel 386 instruction set.
# -fcommon    Preserve common-symbol handling for legacy code.
# -static     Link executables statically.
# -s          Strip symbols from linked executables.
# ============================================================

CFLAGS="${CFLAGS:--O2}"
CFLAGS="${CFLAGS} -m32 -march=i386 -fcommon"

LDFLAGS="-m32 -static -s"

export CC
export CFLAGS
export LDFLAGS

# ============================================================
# Verify permissions
# ============================================================

[ "$(id -u)" -eq 0 ] ||
    fail "Run this script as root to build e2fsprogs."

# ============================================================
# Verify expected paths
# ============================================================

[ "$SOURCE_DIR" = "/home/user/sources/e2fsprogs" ] ||
    fail "Unexpected source directory: ${SOURCE_DIR}"

[ "$BUILD_ROOT" = "/home/user/sources/build" ] ||
    fail "Unexpected build root: ${BUILD_ROOT}"

[ "$BUILD_DIR" = "/home/user/sources/build/e2fsprogs" ] ||
    fail "Unexpected build directory: ${BUILD_DIR}"

# ============================================================
# Verify required tools
# ============================================================

for tool in \
    make cp find file grep readelf rm mkdir getconf nm
do
    command -v "$tool" >/dev/null 2>&1 ||
        fail "Required build tool not found: ${tool}"
done

CC_PATH=$(command -v "$CC" 2>/dev/null) ||
    fail "Required compiler '${CC}' not found in PATH."

[ -x "$CC_PATH" ] ||
    fail "Compiler is not executable: ${CC_PATH}"

# ============================================================
# Verify compiler version
# ============================================================

echo "============================================================"
echo "Checking compiler"
echo "============================================================"

GCC_VERSION=$("$CC" -dumpversion 2>/dev/null) ||
    fail "Could not determine GCC version."

case "$GCC_VERSION" in
    3.3|3.3.*)
        ;;
    *)
        fail "GCC 3.3 is required; ${CC} reports version ${GCC_VERSION}."
        ;;
esac

echo "Compiler command: ${CC}"
echo "Compiler path: ${CC_PATH}"
echo "GCC version: ${GCC_VERSION}"

# ============================================================
# Verify compiler target architecture
# ============================================================

GCC_TARGET=$("$CC" -dumpmachine 2>/dev/null) ||
    fail "Could not determine GCC target architecture."

case "$GCC_TARGET" in
    i386-*|i486-*|i586-*|i686-*)
        ;;
    *)
        fail "Compiler target '${GCC_TARGET}' is not an i386-family target."
        ;;
esac

echo "Compiler target: ${GCC_TARGET}"

# ============================================================
# Verify actual 32-bit i386 compilation and static linking
# ============================================================

echo
echo "============================================================"
echo "Testing i386 compiler and static linker support"
echo "============================================================"

mkdir -p "$BUILD_ROOT"

TEST_DIR="${BUILD_ROOT}/.e2fsprogs-i386-test"
TEST_SOURCE="${TEST_DIR}/test.c"
TEST_EXECUTABLE="${TEST_DIR}/test"

rm -rf "$TEST_DIR"
mkdir -p "$TEST_DIR" ||
    fail "Could not create compiler test directory."

cat > "$TEST_SOURCE" <<'EOF'
int main(void)
{
    return 0;
}
EOF

if ! "$CC" \
    -m32 \
    -march=i386 \
    -static \
    -fcommon \
    -O2 \
    -o "$TEST_EXECUTABLE" \
    "$TEST_SOURCE"
then
    rm -rf "$TEST_DIR"
    fail "GCC 3.3 cannot compile and statically link an i386 executable. Check the assembler, linker, and 32-bit static libraries."
fi

TEST_FILE=$(file "$TEST_EXECUTABLE" 2>&1) ||
    fail "Could not inspect the i386 compiler test."

echo "$TEST_FILE"

echo "$TEST_FILE" | grep -q 'ELF 32-bit' ||
    fail "Compiler test did not produce a 32-bit ELF executable."

echo "$TEST_FILE" | grep -q 'Intel 80386' ||
    fail "Compiler test is not identified as Intel 80386."

echo "$TEST_FILE" | grep -q 'statically linked' ||
    fail "Compiler test executable is not statically linked."

if readelf -l "$TEST_EXECUTABLE" 2>/dev/null |
    grep -q 'INTERP'
then
    rm -rf "$TEST_DIR"
    fail "Compiler test has a dynamic ELF interpreter."
fi

rm -rf "$TEST_DIR"

echo "GCC 3.3 i386 static-link test passed."

# ============================================================
# Verify source tree
# ============================================================

echo
echo "============================================================"
echo "Checking e2fsprogs source tree"
echo "============================================================"

[ -d "$SOURCE_DIR" ] ||
    fail "e2fsprogs source directory not found: ${SOURCE_DIR}"

[ -f "${SOURCE_DIR}/configure" ] ||
    fail "e2fsprogs configure script not found. Run 11-extract-sources.sh first."

[ -f "${SOURCE_DIR}/Makefile.in" ] ||
    fail "e2fsprogs Makefile.in not found."

[ -d "${SOURCE_DIR}/e2fsck" ] ||
    fail "e2fsprogs e2fsck source directory not found."

[ -d "${SOURCE_DIR}/misc" ] ||
    fail "e2fsprogs misc source directory not found."

[ -d "${SOURCE_DIR}/resize" ] ||
    fail "e2fsprogs resize source directory not found."

echo "e2fsprogs version: ${E2FSPROGS_VERSION}"
echo "Source directory: ${SOURCE_DIR}"
echo "Build directory: ${BUILD_DIR}"
echo "Compiler: ${CC}"
echo "Compiler path: ${CC_PATH}"
echo "Compiler target: ${GCC_TARGET}"
echo "Architecture flags: -m32 -march=i386"
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
echo "Cleaning previous e2fsprogs build"
echo "============================================================"

[ "$BUILD_ROOT" = "/home/user/sources/build" ] ||
    fail "Unexpected build root: ${BUILD_ROOT}"

[ "$BUILD_DIR" = "${BUILD_ROOT}/e2fsprogs" ] ||
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
echo "Copying e2fsprogs source tree"
echo "============================================================"

cp -R "${SOURCE_DIR}/." "$BUILD_DIR/" ||
    fail "Could not copy e2fsprogs source tree."

# ============================================================
# Configure
# ============================================================

echo
echo "============================================================"
echo "Configuring e2fsprogs ${E2FSPROGS_VERSION} for i386"
echo "============================================================"

cd "$BUILD_DIR"

CC="$CC" \
CFLAGS="$CFLAGS" \
LDFLAGS="$LDFLAGS" \
./configure \
    --prefix=/usr \
    --with-root-prefix= \
    --disable-nls \
    --disable-evms ||
    fail "e2fsprogs configuration failed."

# ============================================================
# Verify configure selected the requested compiler
# ============================================================

echo
echo "Configured Makefile compiler settings:"

grep -E '^(CC|CFLAGS|LDFLAGS)[[:space:]]*=' Makefile ||
    true

# ============================================================
# Build
# ============================================================

echo
echo "============================================================"
echo "Building e2fsprogs ${E2FSPROGS_VERSION} for i386"
echo "============================================================"

make -j"$JOBS" \
    CC="$CC" \
    CFLAGS="$CFLAGS" \
    LDFLAGS="$LDFLAGS" ||
    fail "e2fsprogs compilation failed."

# ============================================================
# Verify required executables
# ============================================================

echo
echo "============================================================"
echo "Verifying required build outputs"
echo "============================================================"

for executable in \
    "$MKE2FS_EXECUTABLE" \
    "$E2FSCK_EXECUTABLE" \
    "$TUNE2FS_EXECUTABLE" \
    "$DUMPE2FS_EXECUTABLE" \
    "$RESIZE2FS_EXECUTABLE"
do
    [ -s "$executable" ] ||
        fail "Required executable was not produced: ${executable}"

    [ -x "$executable" ] ||
        fail "Required output is not executable: ${executable}"
done

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

find "$BUILD_DIR" -type f -perm -111 -print |
while IFS= read -r executable
do
    case "$executable" in
        "$EXECUTABLE_LIST"|"$EXECUTABLE_TEMP")
            continue
            ;;
    esac

    FILE_OUTPUT=$(file "$executable" 2>/dev/null || true)

    case "$FILE_OUTPUT" in
        *"ELF "*)
            echo "$executable"
            ;;
    esac
done > "$EXECUTABLE_TEMP"

mv "$EXECUTABLE_TEMP" "$EXECUTABLE_LIST"

[ -s "$EXECUTABLE_LIST" ] ||
    fail "No executable ELF files found under ${BUILD_DIR}."

for required_executable in \
    "$MKE2FS_EXECUTABLE" \
    "$E2FSCK_EXECUTABLE" \
    "$TUNE2FS_EXECUTABLE" \
    "$DUMPE2FS_EXECUTABLE" \
    "$RESIZE2FS_EXECUTABLE"
do
    grep -Fx "$required_executable" "$EXECUTABLE_LIST" \
        >/dev/null 2>&1 ||
        fail "Required executable is missing from the ELF list: ${required_executable}"
done

cat "$EXECUTABLE_LIST"

# ============================================================
# Verify i386 architecture and static linkage
# ============================================================

echo
echo "============================================================"
echo "Verifying i386 architecture and static linkage"
echo "============================================================"

while IFS= read -r executable
do
    [ -n "$executable" ] || continue

    echo
    echo "Checking: ${executable}"

    FILE_OUTPUT=$(file "$executable" 2>&1) ||
        fail "Could not inspect ${executable}"

    echo "$FILE_OUTPUT"

    echo "$FILE_OUTPUT" | grep -q 'ELF 32-bit' ||
        fail "${executable} is not a 32-bit ELF executable."

    echo "$FILE_OUTPUT" | grep -q 'Intel 80386' ||
        fail "${executable} is not an Intel 80386 executable."

    echo "$FILE_OUTPUT" | grep -q 'statically linked' ||
        fail "${executable} is not confirmed statically linked."

    if readelf -l "$executable" 2>/dev/null |
        grep -q 'INTERP'
    then
        fail "${executable} has a dynamic ELF interpreter."
    fi

    if readelf -d "$executable" 2>/dev/null |
        grep -q 'NEEDED'
    then
        fail "${executable} has dynamic library dependencies."
    fi

    echo "i386 architecture confirmed."
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
echo "e2fsprogs ${E2FSPROGS_VERSION} build completed successfully"
echo "============================================================"

echo "Build directory: ${BUILD_DIR}"
echo "mke2fs: ${MKE2FS_EXECUTABLE}"
echo "e2fsck: ${E2FSCK_EXECUTABLE}"
echo "tune2fs: ${TUNE2FS_EXECUTABLE}"
echo "dumpe2fs: ${DUMPE2FS_EXECUTABLE}"
echo "resize2fs: ${RESIZE2FS_EXECUTABLE}"
echo "Compiler: ${CC}"
echo "Compiler path: ${CC_PATH}"
echo "GCC version: ${GCC_VERSION}"
echo "Compiler target: ${GCC_TARGET}"
echo "Target architecture: ${TARGET_ARCH}"
echo "Architecture flags: -m32 -march=i386"
echo "Parallel jobs: ${JOBS}"
echo "CFLAGS: ${CFLAGS}"
echo "LDFLAGS: ${LDFLAGS}"
echo "Link mode: static"
echo
echo "Build started from a clean directory."
echo "All discovered executable ELF outputs passed i386 and static-link checks."
echo "No installation was performed."
echo "The host installation and distro staging tree were not modified."