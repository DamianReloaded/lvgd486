
#!/bin/sh

# ============================================================
# LVGD486 - Build GCC 5.4.0
# Script: 15-build-gcc5.sh
#
# Bootstrap compiler: GCC 4.0.2
# Default distro compiler: GCC 4.0.2
# Additional toolchain: GCC 5.4.0
#
# Installation is performed separately by the staging installer.
# ============================================================

set -e

# ============================================================
# Clean host environment
# ============================================================

unset LIBRARY_PATH
unset CPATH
unset C_INCLUDE_PATH
unset CPLUS_INCLUDE_PATH
unset PKG_CONFIG_PATH

# ============================================================
# Constants
# ============================================================

SOURCES_DIR="/home/user/sources"
SOURCE_DIR="${SOURCES_DIR}/gcc-5.4.0"
BUILD_DIR="${SOURCES_DIR}/build/gcc-5.4.0"

# Existing host-side GCC 4.0.2 bootstrap installation.
GCC4_PREFIX="/usr/local/gcc-4.0.2"
GCC4_CC="${GCC4_PREFIX}/bin/gcc"
GCC4_CXX="${GCC4_PREFIX}/bin/g++"

INSTALL_PREFIX="/usr/local/gcc-5.4.0"
MAKE_EXECUTABLE="/usr/local/bin/make"

GCC_VERSION="5.4.0"
GCC4_VERSION="4.0.2"
MAKE_VERSION_REQUIRED="3.81"

CONFIGURE_SCRIPT="${SOURCE_DIR}/configure"

# Preserve the existing static-linking configuration.
LDFLAGS="-static"
STAGE1_LDFLAGS="-static"
BOOT_LDFLAGS="-static"

export LDFLAGS
export STAGE1_LDFLAGS
export BOOT_LDFLAGS

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
    "$@" || fail "Command failed: $*"
}

# ============================================================
# Verify root privileges
# ============================================================

[ "$(id -u)" -eq 0 ] ||
    fail "Run this script as root."

# ============================================================
# Verify required commands
# ============================================================

for COMMAND in \
    id sed find head dirname mkdir getconf grep rm
do
    command -v "$COMMAND" >/dev/null 2>&1 ||
        fail "Required command not found: $COMMAND"
done

# ============================================================
# Verify GNU Make 3.81
# ============================================================

[ -x "$MAKE_EXECUTABLE" ] ||
    fail "GNU Make not found at $MAKE_EXECUTABLE. Run 12-build-make.sh first."

ACTUAL_MAKE_VERSION=$(
    "$MAKE_EXECUTABLE" --version | sed -n '1s/.* //p'
)

[ "$ACTUAL_MAKE_VERSION" = "$MAKE_VERSION_REQUIRED" ] ||
    fail "Expected GNU Make $MAKE_VERSION_REQUIRED, found $ACTUAL_MAKE_VERSION."

# ============================================================
# Verify GCC 4.0.2 bootstrap compiler
# ============================================================

[ -x "$GCC4_CC" ] ||
    fail "GCC 4.0.2 C compiler not found: $GCC4_CC"

[ -x "$GCC4_CXX" ] ||
    fail "GCC 4.0.2 C++ compiler not found: $GCC4_CXX"

ACTUAL_GCC4_VERSION=$("$GCC4_CC" -dumpversion)

[ "$ACTUAL_GCC4_VERSION" = "$GCC4_VERSION" ] ||
    fail "Expected GCC $GCC4_VERSION, found $ACTUAL_GCC4_VERSION."

ACTUAL_GXX4_VERSION=$("$GCC4_CXX" -dumpversion)

[ "$ACTUAL_GXX4_VERSION" = "$GCC4_VERSION" ] ||
    fail "Expected G++ $GCC4_VERSION, found $ACTUAL_GXX4_VERSION."

echo "Bootstrap C compiler:"
"$GCC4_CC" --version | head -n 1

echo "Bootstrap C++ compiler:"
"$GCC4_CXX" --version | head -n 1

# ============================================================
# Locate GCC 4.0.2 C++ runtime library
# ============================================================

GCC4_LIBSTDCPP=$(
    find "$GCC4_PREFIX" \
        -name 'libstdc++.so.6*' -print 2>/dev/null |
        head -n 1
)

[ -n "$GCC4_LIBSTDCPP" ] ||
    fail "GCC 4.0.2 libstdc++.so.6 not found under $GCC4_PREFIX."

GCC4_LIB_DIR=$(dirname "$GCC4_LIBSTDCPP")

LD_LIBRARY_PATH="${GCC4_LIB_DIR}${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
export LD_LIBRARY_PATH

echo "GCC 4.0.2 C++ runtime: $GCC4_LIBSTDCPP"
echo "Library search path: $LD_LIBRARY_PATH"

# ============================================================
# Verify GCC 5.4.0 source tree
# ============================================================

[ -x "$CONFIGURE_SCRIPT" ] ||
    fail "GCC $GCC_VERSION configure script not found: $CONFIGURE_SCRIPT"

[ -f "$SOURCE_DIR/gcc/Makefile.in" ] ||
    fail "GCC source tree appears incomplete: $SOURCE_DIR"

# ============================================================
# Detect available processors
# ============================================================

JOBS=$(getconf _NPROCESSORS_ONLN 2>/dev/null || true)

case "$JOBS" in
    ''|*[!0-9]*|0)
        JOBS=$(grep -c '^processor[[:space:]]*:' /proc/cpuinfo 2>/dev/null || true)
        ;;
esac

case "$JOBS" in
    ''|*[!0-9]*|0)
        JOBS=1
        ;;
esac

echo "Static linker flags: $LDFLAGS"
echo "Stage 1 linker flags: $STAGE1_LDFLAGS"
echo "Bootstrap linker flags: $BOOT_LDFLAGS"
echo "Available processors: $JOBS"
echo "Parallel build jobs: $JOBS"

# ============================================================
# Prepare a clean build directory
# ============================================================

[ "$BUILD_DIR" = "/home/user/sources/build/gcc-5.4.0" ] ||
    fail "Unexpected build directory path: $BUILD_DIR"

echo "Removing previous GCC build directory..."

if [ -d "$BUILD_DIR" ]; then
    run rm -rf "$BUILD_DIR"
fi

run mkdir -p "$BUILD_DIR"

# ============================================================
# Configure GCC 5.4.0
# ============================================================

echo
echo "============================================================"
echo "Configuring GCC ${GCC_VERSION}"
echo "============================================================"

cd "$BUILD_DIR" ||
    fail "Cannot enter build directory: $BUILD_DIR"

CC="$GCC4_CC" \
CXX="$GCC4_CXX" \
LDFLAGS="$LDFLAGS" \
"$CONFIGURE_SCRIPT" \
    --prefix="$INSTALL_PREFIX" \
    --enable-languages=c,c++ \
    --disable-nls \
    --disable-multilib \
    --disable-shared \
    --with-stage1-ldflags="$STAGE1_LDFLAGS" \
    --with-boot-ldflags="$BOOT_LDFLAGS" \
    --with-arch=i486 \
    --with-tune=i486 \
    --disable-libsanitizer \
    --disable-libcilkrts ||
    fail "GCC $GCC_VERSION configuration failed."

# ============================================================
# Build GCC 5.4.0
# ============================================================

echo
echo "============================================================"
echo "Building GCC ${GCC_VERSION}"
echo "============================================================"

run "$MAKE_EXECUTABLE" -j"$JOBS"

# ============================================================
# Verify build outputs
# ============================================================

echo
echo "============================================================"
echo "Verifying GCC build outputs"
echo "============================================================"

[ -x "$BUILD_DIR/gcc/xgcc" ] ||
    fail "GCC bootstrap compiler was not produced."

echo "GCC bootstrap compiler:"
ls -l "$BUILD_DIR/gcc/xgcc"

"$BUILD_DIR/gcc/xgcc" --version ||
    fail "GCC bootstrap compiler failed to run."

# ============================================================
# Report staging installation instructions
# ============================================================

echo
echo "============================================================"
echo "GCC ${GCC_VERSION} build completed"
echo "============================================================"

echo "GCC $GCC_VERSION built successfully."
echo
echo "Configured installation prefix:"
echo "  $INSTALL_PREFIX"
echo
echo "No files have been installed into the host system."
echo
echo "Install into the distro staging tree with:"
echo
echo "  cd $BUILD_DIR"
echo "  $MAKE_EXECUTABLE install DESTDIR=/home/user/distrodev"
echo
echo "Expected staged compiler paths:"
echo "  /home/user/distrodev/usr/local/gcc-5.4.0/bin/gcc"
echo "  /home/user/distrodev/usr/local/gcc-5.4.0/bin/g++"

exit 0
