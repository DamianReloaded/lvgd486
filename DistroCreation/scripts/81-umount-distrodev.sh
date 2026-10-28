#!/bin/sh

# ============================================================
# LVGD486 - Unmount Development Image
# ============================================================

set -e

MOUNT_POINT="/mnt/distrodev"
LOOP_DEVICE="/dev/loop1"

# Verify root privileges
if [ "$(id -u)" -ne 0 ]; then
    echo "Error: Run this script as root."
    exit 1
fi

# Verify required commands
for COMMAND in umount losetup grep; do
    if ! command -v "$COMMAND" >/dev/null 2>&1; then
        echo "Error: Required command not found: ${COMMAND}"
        exit 1
    fi
done

# Unmount filesystem if mounted
if grep -q " $MOUNT_POINT " /proc/mounts; then
    echo "Unmounting ${MOUNT_POINT}..."
    umount "$MOUNT_POINT"
else
    echo "${MOUNT_POINT} is not mounted."
fi

# Detach loop device if attached
if losetup "$LOOP_DEVICE" >/dev/null 2>&1; then
    echo "Detaching ${LOOP_DEVICE}..."
    losetup -d "$LOOP_DEVICE"
else
    echo "${LOOP_DEVICE} is not attached."
fi

echo
echo "LVGD486 development image unmounted successfully."
