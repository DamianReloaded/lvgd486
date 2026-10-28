#!/bin/sh

# ============================================================
# LVGD486 - Install KBD 1.12
#
# Install KBD utilities and keyboard data into the distro
# development staging tree.
#
# Source build:
#   /home/user/sources/build/kbd
#
# Staging root:
#   /home/user/distrodev
#
# Installation paths:
#   /home/user/distrodev/bin/loadkeys
#   /home/user/distrodev/usr/bin/*
#   /home/user/distrodev/usr/share/*
#
# No installation into the host system is performed.
# ============================================================

set -e

# ============================================================
# Constants
# ============================================================

SOURCES_DIR="/home/user/sources"
BUILD_DIR="${SOURCES_DIR}/build/kbd"
DISTRODEV_ROOT="/home/user/distrodev"

KBD_VERSION="1.12"

LOADKEYS_SOURCE="${BUILD_DIR}/src/loadkeys"
DUMPKEYS_SOURCE="${BUILD_DIR}/src/dumpkeys"

BIN_DIR="${DISTRODEV_ROOT}/bin"
USR_BIN_DIR="${DISTRODEV_ROOT}/usr/bin"
USR_SHARE_DIR="${DISTRODEV_ROOT}/usr/share"

STAGED_LOADKEYS="${BIN_DIR}/loadkeys"
STAGED_DUMPKEYS="${USR_BIN_DIR}/dumpkeys"

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

[ -d "$DISTRODEV_ROOT" ] ||
    fail "Development staging root does not exist: ${DISTRODEV_ROOT}"

[ -d "$BUILD_DIR" ] ||
    fail "KBD build directory not found: ${BUILD_DIR}. Run 25-build-kbd.sh first."

[ -s "$LOADKEYS_SOURCE" ] ||
    fail "Built loadkeys executable not found: ${LOADKEYS_SOURCE}"

[ -x "$LOADKEYS_SOURCE" ] ||
    fail "Built loadkeys executable is not executable."

[ -s "$DUMPKEYS_SOURCE" ] ||
    fail "Built dumpkeys executable not found: ${DUMPKEYS_SOURCE}"

[ -x "$DUMPKEYS_SOURCE" ] ||
    fail "Built dumpkeys executable is not executable."

for tool in install find file grep readelf
do
    command -v "$tool" >/dev/null 2>&1 ||
        fail "Required installation tool not found: ${tool}"
done

echo "KBD version: ${KBD_VERSION}"
echo "Build directory: ${BUILD_DIR}"
echo "Staging root: ${DISTRODEV_ROOT}"
echo "loadkeys destination: ${STAGED_LOADKEYS}"
echo "dumpkeys destination: ${STAGED_DUMPKEYS}"
echo "Linkage requirement: static"
echo

# ============================================================
# Verify source executable linkage
# ============================================================

verify_static()
{
    executable="$1"

    [ -s "$executable" ] ||
        fail "Executable is missing or empty: ${executable}"

    [ -x "$executable" ] ||
        fail "Executable is not executable: ${executable}"

    echo
    echo "Checking static linkage: ${executable}"

    FILE_OUTPUT=$(file "$executable" 2>&1) ||
        fail "Could not inspect executable: ${executable}"

    echo "$FILE_OUTPUT"

    echo "$FILE_OUTPUT" |
        grep -q 'statically linked' ||
        fail "Executable is not confirmed statically linked: ${executable}"

    if readelf -l "$executable" 2>/dev/null |
        grep -q 'INTERP'; then
        fail "Executable has a dynamic ELF interpreter: ${executable}"
    fi

    if readelf -d "$executable" 2>/dev/null |
        grep -q 'NEEDED'; then
        fail "Executable has dynamic library dependencies: ${executable}"
    fi

    echo "Static linkage confirmed."
}

echo "============================================================"
echo "Verifying source executables"
echo "============================================================"

verify_static "$LOADKEYS_SOURCE"
verify_static "$DUMPKEYS_SOURCE"

# ============================================================
# Create staging directories
# ============================================================

echo
echo "============================================================"
echo "Preparing staging directories"
echo "============================================================"

run mkdir -p "$BIN_DIR"
run mkdir -p "$USR_BIN_DIR"
run mkdir -p "$USR_SHARE_DIR"

# ============================================================
# Install loadkeys
# ============================================================

echo
echo "============================================================"
echo "Installing loadkeys"
echo "============================================================"

run install -m 755 "$LOADKEYS_SOURCE" "$STAGED_LOADKEYS"

# ============================================================
# Install remaining KBD utilities
# ============================================================

echo
echo "============================================================"
echo "Installing KBD utilities"
echo "============================================================"

EXECUTABLE_LIST="${BUILD_DIR}/.install-kbd-executables"

find "${BUILD_DIR}/src" -type f -perm -111 -print |
while IFS= read -r executable
do
    FILE_OUTPUT=$(file "$executable" 2>/dev/null || true)

    case "$FILE_OUTPUT" in
        *"ELF "*)
            echo "$executable"
            ;;
    esac
done > "$EXECUTABLE_LIST"

[ -s "$EXECUTABLE_LIST" ] ||
    fail "No executable ELF files found in the KBD build."

while IFS= read -r executable
do
    [ -n "$executable" ] || continue

    executable_name=${executable##*/}

    # loadkeys belongs in /bin for this distro.
    if [ "$executable_name" = "loadkeys" ]; then
        continue
    fi

    # KBD utilities are installed into /usr/bin.
    destination="${USR_BIN_DIR}/${executable_name}"

    echo "Installing ${executable_name}"
    run install -m 755 "$executable" "$destination"

done < "$EXECUTABLE_LIST"

rm -f "$EXECUTABLE_LIST"

# ============================================================
# Install KBD data files
# ============================================================

echo
echo "============================================================"
echo "Installing KBD data files"
echo "============================================================"

# KBD 1.12's configured data files are normally under data/.
# Copy the directory contents into the staged /usr/share.
if [ -d "${BUILD_DIR}/data" ]; then
    find "${BUILD_DIR}/data" -type f -print |
    while IFS= read -r source_file
    do
        relative_path=${source_file#"${BUILD_DIR}/data/"}
        destination="${USR_SHARE_DIR}/${relative_path}"
        destination_dir=${destination%/*}

        run mkdir -p "$destination_dir"
        run install -m 644 "$source_file" "$destination"
    done
else
    echo "WARNING: No data directory found in the KBD build."
fi

# Install any compiled or generated keyboard data directories
# that are present in the build tree.
for data_dir in keymaps consolefonts unimaps
do
    if [ -d "${BUILD_DIR}/${data_dir}" ]; then
        echo "Installing ${data_dir} data"

        find "${BUILD_DIR}/${data_dir}" -type f -print |
        while IFS= read -r source_file
        do
            relative_path=${source_file#"${BUILD_DIR}/"}
            destination="${USR_SHARE_DIR}/${relative_path}"
            destination_dir=${destination%/*}

            run mkdir -p "$destination_dir"
            run install -m 644 "$source_file" "$destination"
        done
    fi
done

# ============================================================
# Verify required staged executables
# ============================================================

echo
echo "============================================================"
echo "Verifying staged executables"
echo "============================================================"

[ -s "$STAGED_LOADKEYS" ] ||
    fail "Staged loadkeys executable is missing."

[ -x "$STAGED_LOADKEYS" ] ||
    fail "Staged loadkeys executable is not executable."

[ -s "$STAGED_DUMPKEYS" ] ||
    fail "Staged dumpkeys executable is missing."

[ -x "$STAGED_DUMPKEYS" ] ||
    fail "Staged dumpkeys executable is not executable."

# ============================================================
# Verify staged executable linkage
# ============================================================

echo
echo "============================================================"
echo "Verifying staged executable linkage"
echo "============================================================"

verify_static "$STAGED_LOADKEYS"
verify_static "$STAGED_DUMPKEYS"

# Verify all staged KBD ELF utilities.
find "$USR_BIN_DIR" -maxdepth 1 -type f -perm -111 -print |
while IFS= read -r executable
do
    FILE_OUTPUT=$(file "$executable" 2>/dev/null || true)

    case "$FILE_OUTPUT" in
        *"ELF "*)
            verify_static "$executable"
            ;;
    esac
done

# ============================================================
# Verify basic command execution
# ============================================================

echo
echo "============================================================"
echo "Testing staged KBD utilities"
echo "============================================================"

if "$STAGED_LOADKEYS" --help >/dev/null 2>&1; then
    echo "loadkeys execution test passed."
else
    echo "WARNING: loadkeys --help returned a non-zero status."
    echo "This may be normal for this version of KBD."
fi

if "$STAGED_DUMPKEYS" --help >/dev/null 2>&1; then
    echo "dumpkeys execution test passed."
else
    echo "WARNING: dumpkeys --help returned a non-zero status."
    echo "This may be normal for this version of KBD."
fi

# ============================================================
# Inspect staged files
# ============================================================

echo
echo "============================================================"
echo "Inspecting staged KBD files"
echo "============================================================"

echo
echo "loadkeys:"
ls -l "$STAGED_LOADKEYS"

echo
echo "dumpkeys:"
ls -l "$STAGED_DUMPKEYS"

echo
echo "Staged KBD utilities:"
find "$USR_BIN_DIR" -maxdepth 1 -type f -perm -111 -print

echo
echo "Staged KBD data:"
find "$USR_SHARE_DIR" -type f -print |
    grep -E '/(keymaps|consolefonts|unimaps|kbd)/|kbd' ||
    true

# ============================================================
# Installation summary
# ============================================================

echo
echo "============================================================"
echo "Installed KBD ${KBD_VERSION}"
echo "============================================================"

echo
echo "loadkeys:"
ls -l "$STAGED_LOADKEYS"

echo
echo "dumpkeys:"
ls -l "$STAGED_DUMPKEYS"

echo
echo "Staging root:"
ls -ld "$DISTRODEV_ROOT"

echo
echo "SUCCESS: KBD ${KBD_VERSION} staged in ${DISTRODEV_ROOT}"
echo "KBD utilities installed."
echo "Static linkage verified for the required executables."
echo "Host system files were not modified."

exit 0