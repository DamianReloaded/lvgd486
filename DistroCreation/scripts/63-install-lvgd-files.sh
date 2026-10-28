#!/bin/sh

# ============================================================
# LVGD486 - Install LVGD Files and Demos
#
# Copy the LVGD directory into the staged game user's home:
#   /home/user/distrodev/home/game/lvgd
#
# Copy the contents of the LVGD source directory into:
#   /home/user/distrodev/games/lvgd/demos
#
# Source directory:
#   /home/user/lvgd
#
# Staging root:
#   /home/user/distrodev
#
# No installation into the host system is performed.
# ============================================================

set -e

# ============================================================
# Constants
# ============================================================

SOURCE_DIR="/home/user/lvgd"
SOURCE_DIR_BIN="/home/user/lvgd/bin"
DISTRODEV_ROOT="/home/user/distrodev"

USER_NAME="game"

USER_HOME="${DISTRODEV_ROOT}/home/${USER_NAME}"
STAGED_USER_LVGD="${USER_HOME}/lvgd"

GAMES_DIR="${DISTRODEV_ROOT}/games"
LVGD_GAMES_DIR="${GAMES_DIR}/lvgd"
DEMOS_DIR="${LVGD_GAMES_DIR}/demos"

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
    fail "LVGD source directory not found: ${SOURCE_DIR}"

[ -d "$USER_HOME" ] ||
    fail "Staged user home directory not found: ${USER_HOME}. Run 70-configure-staging.sh first."

echo "Source directory: ${SOURCE_DIR}"
echo "Staging root: ${DISTRODEV_ROOT}"
echo "Staged user directory: ${STAGED_USER_LVGD}"
echo "Demos directory: ${DEMOS_DIR}"
echo

# ============================================================
# Prepare staging directories
# ============================================================

echo
echo "============================================================"
echo "Preparing staging directories"
echo "============================================================"

run mkdir -p "$STAGED_USER_LVGD"
run mkdir -p "$DEMOS_DIR"

# ============================================================
# Copy LVGD directory into the game user's home
# ============================================================

echo
echo "============================================================"
echo "Copying LVGD directory into user home"
echo "============================================================"

run cp -a "${SOURCE_DIR}/." "$STAGED_USER_LVGD/"
run rm -r "$STAGED_USER_LVGD/bin"

# ============================================================
# Copy LVGD contents into the demos directory
# ============================================================

echo
echo "============================================================"
echo "Copying LVGD contents into demos directory"
echo "============================================================"

run cp -a "${SOURCE_DIR_BIN}/." "$DEMOS_DIR/"

# ============================================================
# Set ownership
# ============================================================

echo
echo "============================================================"
echo "Setting ownership"
echo "============================================================"

if id "$USER_NAME" >/dev/null 2>&1; then
    run chown -R "$USER_NAME" "$STAGED_USER_LVGD"
    run chown -R "$USER_NAME" "$DEMOS_DIR"
else
    echo "WARNING: Host account '${USER_NAME}' not found."
    echo "Copied files retain their existing ownership."
fi

# ============================================================
# Verify installation
# ============================================================

echo
echo "============================================================"
echo "Verifying copied directories"
echo "============================================================"

[ -d "$STAGED_USER_LVGD" ] ||
    fail "Staged LVGD directory was not created."

[ -d "$DEMOS_DIR" ] ||
    fail "Staged demos directory was not created."

[ -n "$(ls -A "$STAGED_USER_LVGD" 2>/dev/null)" ] ||
    fail "Staged LVGD directory is empty."

[ -n "$(ls -A "$DEMOS_DIR" 2>/dev/null)" ] ||
    fail "Staged demos directory is empty."

echo "User home LVGD directory:"
ls -ld "$STAGED_USER_LVGD"
ls -la "$STAGED_USER_LVGD"

echo
echo "Demos directory:"
ls -ld "$DEMOS_DIR"
ls -la "$DEMOS_DIR"

# ============================================================
# Installation summary
# ============================================================

echo
echo "============================================================"
echo "LVGD Files and Demos Installation Complete"
echo "============================================================"

echo
echo "Source:"
echo "  ${SOURCE_DIR}"

echo
echo "Recursive directory copy:"
echo "  ${STAGED_USER_LVGD}"

echo
echo "Contents copied to demos:"
echo "  ${DEMOS_DIR}"

echo
echo "SUCCESS: LVGD files copied into the staging tree."
echo "Host system files outside the staging tree were not modified."

exit 0