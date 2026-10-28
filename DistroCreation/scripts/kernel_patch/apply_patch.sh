#!/bin/sh

# ============================================================
# LVGD486 - Apply Kernel Modifications
# Script: 63-apply-kernel-modifications.sh
#
# Copy modified kernel files into the Linux source tree.
# ============================================================

PATCH_DIR="/home/user/scripts/kernel_patch/linux"
KERNEL_DIR="/home/user/sources/linux"

FILES="
arch/i386/kernel/ioport.c
drivers/char/mem.c
drivers/char/vt.c
"

fail()
{
    echo "ERROR: $1" >&2
    exit 1
}

echo "Applying LVGD486 kernel modifications..."

# Verify patch directory.
[ -d "$PATCH_DIR" ] || fail "Patch directory not found: $PATCH_DIR"

# Verify kernel source directory.
[ -d "$KERNEL_DIR" ] || fail "Kernel source directory not found: $KERNEL_DIR"

# Verify all source and destination files before copying.
for FILE in $FILES
do
    [ -f "$PATCH_DIR/$FILE" ] || fail "Modified file not found: $PATCH_DIR/$FILE"
    [ -f "$KERNEL_DIR/$FILE" ] || fail "Kernel source file not found: $KERNEL_DIR/$FILE"
done

# Copy and replace the kernel files.
for FILE in $FILES
do
    echo "Installing $FILE..."

    cp "$PATCH_DIR/$FILE" "$KERNEL_DIR/$FILE" ||
        fail "Failed to copy $FILE"
done

echo "Kernel modifications applied successfully."
