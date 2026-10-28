#!/bin/sh

# ============================================================
# LVGD486 - Prepare Boot Directories
# ============================================================

KERNEL_IMAGE="/home/user/sources/linux/arch/i386/boot/bzImage"

DISTRODEV_ROOT="/home/user/distrodev"
DISTROGAME_ROOT="/home/user/distrogame"

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

    [ "$STATUS" -eq 0 ] ||
        fail "Command failed (exit status $STATUS): $*"
}

# ------------------------------------------------------------
# Prerequisites
# ------------------------------------------------------------

[ "$(id -u)" -eq 0 ] || fail "Run this script as root."

[ -f "$KERNEL_IMAGE" ] ||
    fail "Kernel image not found: $KERNEL_IMAGE"

[ -s "$KERNEL_IMAGE" ] ||
    fail "Kernel image is empty: $KERNEL_IMAGE"

# ------------------------------------------------------------
# Prepare boot files for one distro
# ------------------------------------------------------------

prepare_boot()
{
    ROOT="$1"
    TITLE="$2"

    echo
    echo "============================================================"
    echo "Preparing boot files for: $TITLE"
    echo "Root: $ROOT"
    echo "============================================================"

    run mkdir -p "$ROOT/boot"

    run cp "$KERNEL_IMAGE" "$ROOT/boot/bzImage"

    [ -s "$ROOT/boot/bzImage" ] ||
        fail "Kernel copy failed: $ROOT/boot/bzImage"

    echo "Boot files prepared for $TITLE"
}

# ------------------------------------------------------------
# Prepare both distros
# ------------------------------------------------------------

prepare_boot "$DISTRODEV_ROOT" "LVGD Development"
prepare_boot "$DISTROGAME_ROOT" "LVGD Gaming"

echo
echo "SUCCESS: Boot directory and kernel prepared."
exit 0