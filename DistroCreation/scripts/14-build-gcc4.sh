
#!/bin/sh

# ============================================================
# LVGD486 - Build GCC 4.0.2
#
# Bootstrap compiler: GCC 3.3
# Default distro compiler: GCC 4.0.2
#
# Installation is performed separately by the staging installer.
# ============================================================

set -e

# ============================================================
# Constants
# ============================================================

SOURCES_DIR="/home/user/sources"
SOURCE_DIR="${SOURCES_DIR}/gcc-4.0.2"
BUILD_DIR="${SOURCES_DIR}/build/gcc-4.0.2"

# GCC 4.0.2 will be the default compiler in the staged distro.
INSTALL_PREFIX="/usr"

BOOTSTRAP_CC="/usr/bin/gcc-3.3"
BOOTSTRAP_CXX="/usr/bin/g++-3.3"

MAKE_EXECUTABLE="/usr/local/bin/make"
MAKE_VERSION_REQUIRED="3.81"

GCC_VERSION="4.0.2"
BOOTSTRAP_GCC_VERSION="3.3"

CONFIGURE_SCRIPT="${SOURCE_DIR}/configure"

# Set CLEAN_BUILD=1 to discard the existing GCC build directory.
CLEAN_BUILD="${CLEAN_BUILD:-1}"

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
# Prerequisites
# ============================================================

[ "$(id -u)" -eq 0 ] ||
    fail "Run this script as root."

[ -x "$BOOTSTRAP_CC" ] ||
    fail "Bootstrap C compiler not found: $BOOTSTRAP_CC"

[ -x "$BOOTSTRAP_CXX" ] ||
    fail "Bootstrap C++ compiler not found: $BOOTSTRAP_CXX"

[ -x "$MAKE_EXECUTABLE" ] ||
    fail "GNU Make not found at $MAKE_EXECUTABLE. Run 12-build-make.sh first."

[ -x "$CONFIGURE_SCRIPT" ] ||
    fail "GCC $GCC_VERSION configure script not found: $CONFIGURE_SCRIPT"

[ -f "$SOURCE_DIR/gcc/Makefile.in" ] ||
    fail "GCC source tree appears incomplete: $SOURCE_DIR"

ACTUAL_MAKE_VERSION=$(
    "$MAKE_EXECUTABLE" --version | sed -n '1s/.* //p'
)

[ "$ACTUAL_MAKE_VERSION" = "$MAKE_VERSION_REQUIRED" ] ||
    fail "Expected GNU Make $MAKE_VERSION_REQUIRED, found $ACTUAL_MAKE_VERSION."

ACTUAL_BOOTSTRAP_CC_VERSION=$(
    "$BOOTSTRAP_CC" -dumpversion
)

case "$ACTUAL_BOOTSTRAP_CC_VERSION" in
    3.3|3.3.*)
        ;;
    *)
        fail "Expected GCC 3.3, found version $ACTUAL_BOOTSTRAP_CC_VERSION at $BOOTSTRAP_CC."
        ;;
esac

ACTUAL_BOOTSTRAP_CXX_VERSION=$(
    "$BOOTSTRAP_CXX" -dumpversion
)

case "$ACTUAL_BOOTSTRAP_CXX_VERSION" in
    3.3|3.3.*)
        ;;
    *)
        fail "Expected GCC 3.3 C++ compiler, found version $ACTUAL_BOOTSTRAP_CXX_VERSION at $BOOTSTRAP_CXX."
        ;;
esac

echo "Bootstrap C compiler:"
"$BOOTSTRAP_CC" --version | head -n 1

echo "Bootstrap C++ compiler:"
"$BOOTSTRAP_CXX" --version | head -n 1

# ============================================================
# Prepare build directory
# ============================================================

echo
echo "============================================================"
echo "Preparing GCC ${GCC_VERSION} build"
echo "============================================================"

if [ "$CLEAN_BUILD" = "1" ]; then
    echo "Removing existing GCC build directory:"
    echo "$BUILD_DIR"

    [ "$BUILD_DIR" = "${SOURCES_DIR}/build/gcc-${GCC_VERSION}" ] ||
        fail "Refusing to remove unexpected build directory: $BUILD_DIR"

    run rm -rf "$BUILD_DIR"
elif [ "$CLEAN_BUILD" != "0" ]; then
    fail "CLEAN_BUILD must be 0 or 1."
fi

run mkdir -p "$BUILD_DIR"

cd "$BUILD_DIR" ||
    fail "Cannot enter build directory: $BUILD_DIR"

# ============================================================
# Configure GCC 4.0.2
# ============================================================

echo
echo "============================================================"
echo "Configuring GCC ${GCC_VERSION}"
echo "============================================================"

echo "Bootstrap C compiler:   $BOOTSTRAP_CC"
echo "Bootstrap C++ compiler: $BOOTSTRAP_CXX"
echo "Installation prefix:    $INSTALL_PREFIX"
echo "Target architecture:    i486"
echo "Languages:               C, C++"

# Prevent host environment variables from injecting unrelated
# include paths or libraries into this legacy compiler build.
unset LIBRARY_PATH
unset CPATH
unset C_INCLUDE_PATH
unset CPLUS_INCLUDE_PATH
unset PKG_CONFIG_PATH

CC="$BOOTSTRAP_CC" \
CXX="$BOOTSTRAP_CXX" \
"$CONFIGURE_SCRIPT" \
    --prefix="$INSTALL_PREFIX" \
    --enable-languages=c,c++ \
    --disable-nls \
    --disable-multilib \
    --with-arch=i486 \
    --with-tune=i486 ||
    fail "GCC configuration failed."

# ============================================================
# Build GCC 4.0.2
# ============================================================

echo
echo "============================================================"
echo "Building GCC ${GCC_VERSION}"
echo "============================================================"

# Use one job for this legacy compiler to minimize memory usage.
run "$MAKE_EXECUTABLE" -j1

# ============================================================
# Verify build outputs
# ============================================================

echo
echo "============================================================"
echo "Verifying GCC build outputs"
echo "============================================================"

GCC_BUILD_EXECUTABLE="${BUILD_DIR}/gcc/xgcc"

[ -x "$GCC_BUILD_EXECUTABLE" ] ||
    fail "GCC build executable not found: $GCC_BUILD_EXECUTABLE"

echo "GCC build executable:"
ls -l "$GCC_BUILD_EXECUTABLE"

"$GCC_BUILD_EXECUTABLE" --version ||
    fail "Built GCC executable failed to run."

# ============================================================
# Installation instructions
# ============================================================

echo
echo "============================================================"
echo "GCC ${GCC_VERSION} build completed"
echo "============================================================"

echo "GCC was built successfully."
echo
echo "Configured installation prefix:"
echo "  $INSTALL_PREFIX"
echo
echo "This script does not install GCC into the host system."
echo "Install it into the distro staging tree using:"
echo
echo "  cd $BUILD_DIR"
echo "  $MAKE_EXECUTABLE install DESTDIR=/home/user/distrodev"
echo
echo "Expected staged compiler paths:"
echo "  /home/user/distrodev/usr/bin/gcc"
echo "  /home/user/distrodev/usr/bin/g++"
echo
echo "GCC 5.4.0 should remain a separate additional toolchain at:"
echo "  /home/user/distrodev/usr/local/gcc-5.4.0"

exit 0
