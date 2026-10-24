#!/bin/sh

# ============================================================
# LVGD486 - Install Development Toolchain
# Script: 20-install-staging-devtools.sh
# ============================================================

set -e

# ============================================================
# Constants
# ============================================================

SOURCES_DIR="/home/user/sources"
LINUX_SOURCE="${SOURCES_DIR}/linux"

BINUTILS_BUILD="${SOURCES_DIR}/build/binutils-2.15"
GLIBC_BUILD="${SOURCES_DIR}/build/glibc-2.3.2"
MAKE_BUILD="${SOURCES_DIR}/build/make"
GCC_BUILD="${SOURCES_DIR}/build/gcc-5.4.0"

MAKE_EXECUTABLE="/usr/local/bin/make"
GDB_EXECUTABLE="/usr/local/bin/gdb"
GDBSERVER_EXECUTABLE="/usr/local/bin/gdbserver"

DISTRODEV_ROOT="/home/user/distrodev"

BINUTILS_VERSION="2.15"
GLIBC_VERSION="2.3.2"

# ============================================================
# Error handling
# ============================================================

fail()
{
    echo "ERROR: $*" >&2
    exit 1
}

run()
{
    echo "RUN: $*"

    "$@"
    STATUS=$?

    if [ "$STATUS" -ne 0 ]; then
        fail "Command failed (exit status $STATUS): $*"
    fi
}

# ============================================================
# Prerequisites
# ============================================================

[ "$(id -u)" -eq 0 ] ||
    fail "Run this script as root."

[ -x "$MAKE_EXECUTABLE" ] ||
    fail "GNU Make not found: $MAKE_EXECUTABLE"

[ -f "$LINUX_SOURCE/Makefile" ] ||
    fail "Linux kernel source tree not found: $LINUX_SOURCE"

[ -f "$LINUX_SOURCE/include/linux/errno.h" ] ||
    fail "Linux kernel header not found: $LINUX_SOURCE/include/linux/errno.h"

[ -d "$LINUX_SOURCE/include/asm-i386" ] ||
    fail "Linux 2.4 i386 architecture headers not found: $LINUX_SOURCE/include/asm-i386"

[ -f "$BINUTILS_BUILD/Makefile" ] ||
    fail "Binutils build directory is not configured: $BINUTILS_BUILD"

[ -x "$BINUTILS_BUILD/gas/as-new" ] ||
    fail "Binutils assembler build output not found."

[ -x "$BINUTILS_BUILD/ld/ld-new" ] ||
    fail "Binutils linker build output not found."

[ -f "$GLIBC_BUILD/Makefile" ] ||
    fail "glibc build directory is not configured: $GLIBC_BUILD"

[ -f "$GLIBC_BUILD/libc.a" ] ||
    fail "glibc static library build output not found."

[ -f "$GLIBC_BUILD/libc.so" ] ||
    fail "glibc shared library build output not found."

[ -f "$MAKE_BUILD/Makefile" ] ||
    fail "Make build directory is not configured: $MAKE_BUILD"

[ -f "$GCC_BUILD/Makefile" ] ||
    fail "GCC build directory is not configured: $GCC_BUILD"

[ -x "$GDB_EXECUTABLE" ] ||
    fail "GDB executable not found: $GDB_EXECUTABLE"

[ -x "$GDBSERVER_EXECUTABLE" ] ||
    fail "GDB Server executable not found: $GDBSERVER_EXECUTABLE"

[ -d "$DISTRODEV_ROOT" ] ||
    fail "Development staging root does not exist: $DISTRODEV_ROOT"

# ============================================================
# Install Binutils 2.15
# ============================================================

echo
echo "============================================================"
echo "Installing Binutils ${BINUTILS_VERSION}"
echo "============================================================"

cd "$BINUTILS_BUILD" ||
    fail "Cannot enter $BINUTILS_BUILD"

run "$MAKE_EXECUTABLE" install DESTDIR="$DISTRODEV_ROOT"

# ============================================================
# Install Linux 2.4 userspace headers
# ============================================================

echo
echo "============================================================"
echo "Installing Linux 2.4 userspace headers"
echo "============================================================"

KERNEL_INCLUDE_DIR="$DISTRODEV_ROOT/usr/include"

run mkdir -p "$KERNEL_INCLUDE_DIR/linux"
run mkdir -p "$KERNEL_INCLUDE_DIR/asm"

# Copy common Linux headers.
run cp -a "$LINUX_SOURCE/include/linux/." \
    "$KERNEL_INCLUDE_DIR/linux/"

# Copy i386 architecture headers into the conventional
# userspace include path.
run cp -a "$LINUX_SOURCE/include/asm-i386/." \
    "$KERNEL_INCLUDE_DIR/asm/"

# Copy generic architecture headers if present.
if [ -d "$LINUX_SOURCE/include/asm-generic" ]; then
    run mkdir -p "$KERNEL_INCLUDE_DIR/asm-generic"

    run cp -a "$LINUX_SOURCE/include/asm-generic/." \
        "$KERNEL_INCLUDE_DIR/asm-generic/"
fi

[ -f "$KERNEL_INCLUDE_DIR/linux/errno.h" ] ||
    fail "Linux header not installed: $KERNEL_INCLUDE_DIR/linux/errno.h"

[ -f "$KERNEL_INCLUDE_DIR/asm/errno.h" ] ||
    fail "i386 architecture header not installed: $KERNEL_INCLUDE_DIR/asm/errno.h"

echo "Linux 2.4 headers copied to $KERNEL_INCLUDE_DIR"

# ============================================================
# Install glibc 2.3.2 runtime and development files
# ============================================================

echo
echo "============================================================"
echo "Installing glibc ${GLIBC_VERSION}"
echo "============================================================"

cd "$GLIBC_BUILD" ||
    fail "Cannot enter $GLIBC_BUILD"

run "$MAKE_EXECUTABLE" install_root="$DISTRODEV_ROOT" install

# ============================================================
# Install GNU Make 3.81
# ============================================================

echo
echo "============================================================"
echo "Installing GNU Make 3.81"
echo "============================================================"

cd "$MAKE_BUILD" ||
    fail "Cannot enter $MAKE_BUILD"

run "$MAKE_EXECUTABLE" install DESTDIR="$DISTRODEV_ROOT"

# ============================================================
# Install GCC 5.4.0
# ============================================================

echo
echo "============================================================"
echo "Installing GCC 5.4.0"
echo "============================================================"

cd "$GCC_BUILD" ||
    fail "Cannot enter $GCC_BUILD"

run "$MAKE_EXECUTABLE" install DESTDIR="$DISTRODEV_ROOT"

# ============================================================
# Install GDB 6.3 and GDB Server
# ============================================================

echo
echo "============================================================"
echo "Installing GDB 6.3 and GDB Server"
echo "============================================================"

run mkdir -p "$DISTRODEV_ROOT/usr/local/bin"

run install -m 755 "$GDB_EXECUTABLE" \
    "$DISTRODEV_ROOT/usr/local/bin/gdb"

run install -m 755 "$GDBSERVER_EXECUTABLE" \
    "$DISTRODEV_ROOT/usr/local/bin/gdbserver"

# ============================================================
# Verify installed Binutils
# ============================================================

echo
echo "============================================================"
echo "Verifying Binutils installation"
echo "============================================================"

for TOOL in as ld ar ranlib nm objcopy objdump strip readelf
do
    TOOL_PATH="$DISTRODEV_ROOT/usr/local/bin/$TOOL"

    [ -x "$TOOL_PATH" ] ||
        fail "Binutils tool not installed: $TOOL_PATH"
done

"$DISTRODEV_ROOT/usr/local/bin/as" --version
"$DISTRODEV_ROOT/usr/local/bin/ld" --version

# ============================================================
# Verify Linux kernel headers
# ============================================================

echo
echo "============================================================"
echo "Verifying Linux kernel userspace headers"
echo "============================================================"

[ -f "$KERNEL_INCLUDE_DIR/linux/errno.h" ] ||
    fail "Linux errno header not installed."

[ -f "$KERNEL_INCLUDE_DIR/asm/errno.h" ] ||
    fail "i386 errno header not installed."

echo "Linux kernel headers verified:"
ls -l \
    "$KERNEL_INCLUDE_DIR/linux/errno.h" \
    "$KERNEL_INCLUDE_DIR/asm/errno.h"

# ============================================================
# Verify glibc development files
# ============================================================

echo
echo "============================================================"
echo "Verifying glibc development files"
echo "============================================================"

GLIBC_INCLUDE_DIR="$DISTRODEV_ROOT/usr/include"
GLIBC_LIB_DIR="$DISTRODEV_ROOT/usr/lib"

[ -f "$GLIBC_INCLUDE_DIR/stdio.h" ] ||
    fail "glibc header not installed: $GLIBC_INCLUDE_DIR/stdio.h"

[ -f "$GLIBC_INCLUDE_DIR/stdlib.h" ] ||
    fail "glibc header not installed: $GLIBC_INCLUDE_DIR/stdlib.h"

[ -f "$GLIBC_LIB_DIR/libc.a" ] ||
    fail "glibc static library not installed: $GLIBC_LIB_DIR/libc.a"

[ -f "$GLIBC_LIB_DIR/libc.so" ] ||
    [ -L "$GLIBC_LIB_DIR/libc.so" ] ||
    fail "glibc linker library not installed: $GLIBC_LIB_DIR/libc.so"

echo "glibc headers and development libraries verified."

# ============================================================
# Verify installed glibc runtime
# ============================================================

echo
echo "============================================================"
echo "Verifying glibc runtime"
echo "============================================================"

GLIBC_RUNTIME=$(find "$DISTRODEV_ROOT" \
    \( -type f -o -type l \) \
    -name 'libc.so.6' -print | head -n 1)

[ -n "$GLIBC_RUNTIME" ] ||
    fail "glibc runtime library libc.so.6 was not found in staging."

echo "glibc runtime library: $GLIBC_RUNTIME"

# ============================================================
# Verify installed development tools
# ============================================================

echo
echo "============================================================"
echo "Verifying development toolchain"
echo "============================================================"

for TOOL in make gcc g++ gdb gdbserver
do
    TOOL_PATH="$DISTRODEV_ROOT/usr/local/bin/$TOOL"

    [ -x "$TOOL_PATH" ] ||
        fail "Development tool not installed: $TOOL_PATH"
done

# ============================================================
# Installation summary
# ============================================================

echo
echo "============================================================"
echo "Installed development tools"
echo "============================================================"

ls -l \
    "$DISTRODEV_ROOT/usr/local/bin/as" \
    "$DISTRODEV_ROOT/usr/local/bin/ld" \
    "$DISTRODEV_ROOT/usr/local/bin/ar" \
    "$DISTRODEV_ROOT/usr/local/bin/ranlib" \
    "$DISTRODEV_ROOT/usr/local/bin/make" \
    "$DISTRODEV_ROOT/usr/local/bin/gcc" \
    "$DISTRODEV_ROOT/usr/local/bin/g++" \
    "$DISTRODEV_ROOT/usr/local/bin/gdb" \
    "$DISTRODEV_ROOT/usr/local/bin/gdbserver"

echo
echo "Linux kernel headers:"
ls -l \
    "$KERNEL_INCLUDE_DIR/linux/errno.h" \
    "$KERNEL_INCLUDE_DIR/asm/errno.h"

echo
echo "glibc development headers:"
ls -l \
    "$GLIBC_INCLUDE_DIR/stdio.h" \
    "$GLIBC_INCLUDE_DIR/stdlib.h"

echo
echo "glibc development libraries:"
ls -l \
    "$GLIBC_LIB_DIR/libc.a" \
    "$GLIBC_LIB_DIR/libc.so"

echo
echo "SUCCESS: Development toolchain staged in ${DISTRODEV_ROOT}"

exit 0