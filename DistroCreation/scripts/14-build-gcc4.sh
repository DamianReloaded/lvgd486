
#!/bin/sh

# ============================================================
# LVGD486 - Build GCC 4.0.2
# Script: 14-build-gcc4.sh
# ============================================================

unset LIBRARY_PATH CPATH C_INCLUDE_PATH CPLUS_INCLUDE_PATH PKG_CONFIG_PATH

# Constants
SOURCES_DIR="/home/user/sources"
SOURCE_DIR="${SOURCES_DIR}/gcc-4.0.2"
BUILD_DIR="${SOURCES_DIR}/build/gcc-4.0.2"

INSTALL_PREFIX="/usr/local/gcc-4.0.2"
MAKE_EXECUTABLE="/usr/local/bin/make"

GCC_VERSION="4.0.2"
CONFIGURE_SCRIPT="${SOURCE_DIR}/configure"

# Verify root privileges
if [ "$(id -u)" -ne 0 ]; then
    echo "Error: Run this script as root."
    exit 1
fi

# Verify GNU Make 3.81
if [ ! -x "$MAKE_EXECUTABLE" ]; then
    echo "Error: GNU Make 3.81 not found at ${MAKE_EXECUTABLE}"
    echo "Run 12-build-make.sh first."
    exit 1
fi

MAKE_VERSION=$("$MAKE_EXECUTABLE" --version | sed -n '1s/.* //p')

if [ "$MAKE_VERSION" != "3.81" ]; then
    echo "Error: Expected GNU Make 3.81, found ${MAKE_VERSION}"
    exit 1
fi

# Verify GCC source tree
if [ ! -x "$CONFIGURE_SCRIPT" ]; then
    echo "Error: GCC ${GCC_VERSION} configure script not found."
    echo "Expected: ${CONFIGURE_SCRIPT}"
    echo "Run 00-download-packages.sh first."
    exit 1
fi

# Prepare build directory
mkdir -p "$BUILD_DIR" || exit 1
cd "$BUILD_DIR" || exit 1

# Configure GCC
CC=gcc \
CXX=g++ \
"$CONFIGURE_SCRIPT" \
    --prefix="$INSTALL_PREFIX" \
    --enable-languages=c,c++ \
    --disable-nls \
    --disable-multilib \
    --with-arch=i486 \
    --with-tune=i486 || exit 1

# Build GCC
"$MAKE_EXECUTABLE" -j1 || exit 1

# Install GCC
"$MAKE_EXECUTABLE" install || exit 1

# Refresh shared library cache
/sbin/ldconfig || exit 1

# Verify installation
if [ ! -x "${INSTALL_PREFIX}/bin/gcc" ] ||
   [ ! -x "${INSTALL_PREFIX}/bin/g++" ]; then
    echo "Error: GCC installation verification failed."
    exit 1
fi

"${INSTALL_PREFIX}/bin/gcc" --version || exit 1
"${INSTALL_PREFIX}/bin/g++" --version || exit 1

echo "GCC ${GCC_VERSION} built and installed successfully."
echo "GCC: ${INSTALL_PREFIX}/bin/gcc"
echo "G++: ${INSTALL_PREFIX}/bin/g++"
