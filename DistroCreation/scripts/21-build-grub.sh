
#!/bin/sh

# ============================================================
# LVGD486 - Build GNU GRUB Legacy for i386
# ============================================================

set -e

# ============================================================
# Constants
# ============================================================

SOURCES_DIR="/home/user/sources"
BUILD_DIR="${SOURCES_DIR}/build/grub"
SOURCE_DIR="${SOURCES_DIR}/grub"
STAGING_DIR="/home/user/doc/grub-stage-files"

GRUB_VERSION="0.95+cvs20040624"

CONFIGURE_SCRIPT="${SOURCE_DIR}/configure"

CC="/usr/bin/gcc-3.3"
OBJCOPY="/usr/bin/objcopy"
MAKE="${MAKE:-make}"

export CC
export OBJCOPY

# Prefer the selected system toolchain.
PATH="/usr/bin:/bin:/usr/sbin:/sbin:${PATH}"
export PATH

# ============================================================
# Verify toolchain
# ============================================================

if [ ! -x "$CC" ]; then
    echo "Error: GCC 3.3 not found."
    echo "Expected: ${CC}"
    exit 1
fi

if [ ! -x "$OBJCOPY" ]; then
    echo "Error: GNU objcopy not found."
    echo "Expected: ${OBJCOPY}"
    exit 1
fi

if ! command -v "$MAKE" >/dev/null 2>&1; then
    echo "Error: make was not found."
    exit 1
fi

echo "Compiler: $("$CC" --version | head -n 1)"
echo "Compiler target: $("$CC" -dumpmachine)"
echo "Objcopy: $("$OBJCOPY" --version | head -n 1)"

case "$("$CC" -dumpmachine)" in
    i386-*|i486-*|i586-*|i686-*)
        ;;
    *)
        echo "Error: GCC 3.3 does not report an i386-family target."
        echo "Target: $("$CC" -dumpmachine)"
        exit 1
        ;;
esac

# ============================================================
# Verify source tree
# ============================================================

if [ ! -x "$CONFIGURE_SCRIPT" ]; then
    echo "Error: GRUB ${GRUB_VERSION} configure script not found."
    echo "Expected: ${CONFIGURE_SCRIPT}"
    echo "Run the source download and extraction scripts first."
    exit 1
fi

if [ ! -d "${SOURCE_DIR}/stage2" ]; then
    echo "Error: GRUB stage2 source directory not found."
    echo "Expected: ${SOURCE_DIR}/stage2"
    exit 1
fi

# ============================================================
# Verify build directory safety
# ============================================================

if [ "$BUILD_DIR" != "/home/user/sources/build/grub" ]; then
    echo "Error: Unexpected build directory path: ${BUILD_DIR}"
    exit 1
fi

# ============================================================
# Start from a clean build directory
# ============================================================

echo "Removing previous GRUB build directory..."

if [ -d "$BUILD_DIR" ]; then
    rm -rf "$BUILD_DIR"
fi

mkdir -p "$BUILD_DIR" "$STAGING_DIR"

echo "Fresh build directory created: ${BUILD_DIR}"

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

echo "Available processors: ${JOBS}"
echo "Parallel build jobs: ${JOBS}"

# ============================================================
# Configure for i386
# ============================================================

echo "Configuring GRUB Legacy for i386..."

cd "$BUILD_DIR"

if ! "$CONFIGURE_SCRIPT" \
    --build=i386-pc-linux-gnu \
    --host=i386-pc-linux-gnu \
    --prefix=/usr/local
then
    echo "Error: GRUB configuration failed."
    echo "Inspect ${BUILD_DIR}/config.log for the failing test."
    exit 1
fi

# ============================================================
# Build using available processors
# ============================================================

echo "Building GRUB Legacy..."

"$MAKE" -j"$JOBS"

# ============================================================
# Verify required build outputs
# ============================================================

BUILD_STAGE1="${BUILD_DIR}/stage1/stage1"
BUILD_STAGE2="${BUILD_DIR}/stage2/stage2"
BUILD_STAGE2_ELTORITO="${BUILD_DIR}/stage2/stage2_eltorito"
BUILD_E2FS_STAGE1_5="${BUILD_DIR}/stage2/e2fs_stage1_5"

for stage_file in \
    "$BUILD_STAGE1" \
    "$BUILD_STAGE2" \
    "$BUILD_STAGE2_ELTORITO"
do
    if [ ! -s "$stage_file" ]; then
        echo "Error: Required GRUB build output missing or empty."
        echo "File: ${stage_file}"
        exit 1
    fi
done

echo "Verified GRUB build outputs."

# ============================================================
# Stage GRUB files for the installer
# ============================================================

echo "Copying GRUB stage files to ${STAGING_DIR}..."

cp "$BUILD_STAGE1" "${STAGING_DIR}/stage1"
cp "$BUILD_STAGE2" "${STAGING_DIR}/stage2"
cp "$BUILD_STAGE2_ELTORITO" "${STAGING_DIR}/stage2_eltorito"

if [ -s "$BUILD_E2FS_STAGE1_5" ]; then
    cp "$BUILD_E2FS_STAGE1_5" "${STAGING_DIR}/e2fs_stage1_5"
fi

# ============================================================
# Verify staged files
# ============================================================

for stage_file in \
    "${STAGING_DIR}/stage1" \
    "${STAGING_DIR}/stage2" \
    "${STAGING_DIR}/stage2_eltorito"
do
    if [ ! -s "$stage_file" ]; then
        echo "Error: Required staged GRUB file missing or empty."
        echo "File: ${stage_file}"
        exit 1
    fi
done

# ============================================================
# Final report
# ============================================================

echo
echo "GRUB Legacy ${GRUB_VERSION} built successfully."
echo "Compiler: ${CC}"
echo "Objcopy: ${OBJCOPY}"
echo "Build directory: ${BUILD_DIR}"
echo "Installer stage directory: ${STAGING_DIR}"
echo "Stage 1: ${STAGING_DIR}/stage1"
echo "Stage 2: ${STAGING_DIR}/stage2"
echo "El Torito image: ${STAGING_DIR}/stage2_eltorito"
echo "Parallel jobs used: ${JOBS}"
echo "GRUB was built but not installed system-wide."
