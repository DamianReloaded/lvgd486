#!/bin/sh

# ============================================================
# LVGD486 - Install Bash 2.05b
#
# Install the statically linked Bash executable into the
# distro development staging tree.
#
# Source build:
#   /home/user/sources/bash/bash
#
# Staging root:
#   /home/user/distrodev
#
# Installation path:
#   /home/user/distrodev/bin/bash
#
# No installation into the host system is performed.
# ============================================================

set -e

# ============================================================
# Constants
# ============================================================

SOURCE_DIR="/home/user/sources/bash"
DISTRODEV_ROOT="/home/user/distrodev"

BASH_VERSION="2.05b"

SOURCE_EXECUTABLE="${SOURCE_DIR}/bash"
BIN_DIR="${DISTRODEV_ROOT}/bin"
STAGED_EXECUTABLE="${BIN_DIR}/bash"

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

[ -d "$SOURCE_DIR" ] ||
    fail "Bash source directory not found: ${SOURCE_DIR}"

[ -s "$SOURCE_EXECUTABLE" ] ||
    fail "Built Bash executable not found: ${SOURCE_EXECUTABLE}. Run 24-build-bash.sh first."

[ -x "$SOURCE_EXECUTABLE" ] ||
    fail "Built Bash executable is not executable: ${SOURCE_EXECUTABLE}"

echo "Bash version: ${BASH_VERSION}"
echo "Source: ${SOURCE_EXECUTABLE}"
echo "Staging root: ${DISTRODEV_ROOT}"
echo "Installation path: ${STAGED_EXECUTABLE}"
echo "Linkage requirement: static"
echo

# ============================================================
# Verify source executable version
# ============================================================

echo
echo "============================================================"
echo "Verifying source Bash executable"
echo "============================================================"

SOURCE_VERSION_OUTPUT=$("$SOURCE_EXECUTABLE" --version 2>&1) ||
    fail "Could not execute the source Bash binary."

echo "$SOURCE_VERSION_OUTPUT"

echo "$SOURCE_VERSION_OUTPUT" |
    grep -q "version ${BASH_VERSION}" ||
    fail "Source Bash version does not match ${BASH_VERSION}."

# ============================================================
# Verify source executable is statically linked
# ============================================================

echo
echo "============================================================"
echo "Checking source executable linkage"
echo "============================================================"

if command -v file >/dev/null 2>&1; then
    SOURCE_FILE_OUTPUT=$(file "$SOURCE_EXECUTABLE" 2>&1)
    echo "$SOURCE_FILE_OUTPUT"

    echo "$SOURCE_FILE_OUTPUT" |
        grep -q 'statically linked' ||
        fail "Source Bash executable is not confirmed statically linked."
else
    fail "The file utility is required to verify static linkage."
fi

if command -v ldd >/dev/null 2>&1; then
    SOURCE_LDD_OUTPUT=$(ldd "$SOURCE_EXECUTABLE" 2>&1 || true)
    echo "$SOURCE_LDD_OUTPUT"

    case "$SOURCE_LDD_OUTPUT" in
        *"not a dynamic executable"*|*"statically linked"*)
            echo "Source static linkage confirmed."
            ;;
        *)
            fail "Could not confirm static linkage using ldd."
            ;;
    esac
fi

# ============================================================
# Create staging directories
# ============================================================

echo
echo "============================================================"
echo "Preparing staging directories"
echo "============================================================"

run mkdir -p "$BIN_DIR"

# ============================================================
# Install Bash executable
# ============================================================

echo
echo "============================================================"
echo "Installing Bash executable"
echo "============================================================"

run install -m 755 "$SOURCE_EXECUTABLE" "$STAGED_EXECUTABLE"

# ============================================================
# Verify installed executable
# ============================================================

echo
echo "============================================================"
echo "Verifying installed Bash executable"
echo "============================================================"

[ -s "$STAGED_EXECUTABLE" ] ||
    fail "Staged Bash executable is missing or empty."

[ -x "$STAGED_EXECUTABLE" ] ||
    fail "Staged Bash executable is not executable."

STAGED_VERSION_OUTPUT=$("$STAGED_EXECUTABLE" --version 2>&1) ||
    fail "Staged Bash executable could not be run."

echo "$STAGED_VERSION_OUTPUT"

echo "$STAGED_VERSION_OUTPUT" |
    grep -q "version ${BASH_VERSION}" ||
    fail "Staged Bash version does not match ${BASH_VERSION}."

# ============================================================
# Verify installed executable runs correctly
# ============================================================

echo
echo "============================================================"
echo "Testing staged Bash executable"
echo "============================================================"

TEST_OUTPUT=$(
    "$STAGED_EXECUTABLE" -c 'echo LVGD486_BASH_TEST'
) || fail "Staged Bash execution test failed."

[ "$TEST_OUTPUT" = "LVGD486_BASH_TEST" ] ||
    fail "Unexpected output from staged Bash: ${TEST_OUTPUT}"

echo "$TEST_OUTPUT"
echo "Execution test passed."

# ============================================================
# Verify installed executable linkage
# ============================================================

echo
echo "============================================================"
echo "Verifying staged executable linkage"
echo "============================================================"

STAGED_FILE_OUTPUT=$(file "$STAGED_EXECUTABLE" 2>&1)
echo "$STAGED_FILE_OUTPUT"

echo "$STAGED_FILE_OUTPUT" |
    grep -q 'statically linked' ||
    fail "Staged Bash executable is not confirmed statically linked."

if command -v ldd >/dev/null 2>&1; then
    STAGED_LDD_OUTPUT=$(ldd "$STAGED_EXECUTABLE" 2>&1 || true)
    echo "$STAGED_LDD_OUTPUT"

    case "$STAGED_LDD_OUTPUT" in
        *"not a dynamic executable"*|*"statically linked"*)
            echo "Staged static linkage confirmed."
            ;;
        *)
            fail "Could not confirm static linkage for staged Bash."
            ;;
    esac
fi

# ============================================================
# Inspect installed executable
# ============================================================

echo
echo "============================================================"
echo "Inspecting staged Bash executable"
echo "============================================================"

ls -l "$STAGED_EXECUTABLE"
size "$STAGED_EXECUTABLE"

# ============================================================
# Installation summary
# ============================================================

echo
echo "============================================================"
echo "Installed Bash ${BASH_VERSION}"
echo "============================================================"

echo
echo "Executable:"
ls -l "$STAGED_EXECUTABLE"

echo
echo "Staging directory:"
ls -ld "$BIN_DIR"

echo
echo "SUCCESS: Bash ${BASH_VERSION} staged in ${DISTRODEV_ROOT}"
echo "Installed executable: ${STAGED_EXECUTABLE}"
echo "Static linkage verified."
echo "Host system files were not modified."

exit 0
