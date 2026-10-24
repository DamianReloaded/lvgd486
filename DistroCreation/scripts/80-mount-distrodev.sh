#!/bin/sh

# ============================================================
# LVGD486 - Mount Development Image
# Script: 80-mount-distrodev.sh
# ============================================================

set -e

IMAGE="/home/user/images/distrodev.img"
MOUNT_POINT="/mnt/distrodev"
LOOP_DEVICE="/dev/loop1"
PARTITION_OFFSET=32256

# Verify root privileges
if [ "$(id -u)" -ne 0 ]; then
    echo "Error: Run this script as root."
    exit 1
fi

# Verify required commands
for COMMAND in losetup mount mkdir grep; do
    if ! command -v "$COMMAND" >/dev/null 2>&1; then
        echo "Error: Required command not found: ${COMMAND}"
        exit 1
    fi
done

# Verify image exists
if [ ! -f "$IMAGE" ]; then
    echo "Error: Disk image not found: ${IMAGE}"
    exit 1
fi

# Create mount point
mkdir -p "$MOUNT_POINT"

# Check whether the mount point is already mounted
if grep -q " $MOUNT_POINT " /proc/mounts; then
    echo "Error: ${MOUNT_POINT} is already mounted."
    exit 1
fi

# Check whether the loop device is already in use
if losetup "$LOOP_DEVICE" >/dev/null 2>&1; then
    echo "Error: ${LOOP_DEVICE} is already in use."
    echo "Inspect it with: losetup ${LOOP_DEVICE}"
    exit 1
fi

# Attach the image at the start of partition 1.
echo "Attaching image..."
losetup -o "$PARTITION_OFFSET" "$LOOP_DEVICE" "$IMAGE"

# Mount the ext2 filesystem
echo "Mounting development filesystem..."
if ! mount -t ext2 "$LOOP_DEVICE" "$MOUNT_POINT"; then
    echo "Error: Mount failed. Detaching loop device."
    losetup -d "$LOOP_DEVICE" || true
    exit 1
fi

echo
echo "LVGD486 development image mounted successfully."
echo "Image:      ${IMAGE}"
echo "Loop device: ${LOOP_DEVICE}"
echo "Offset:     ${PARTITION_OFFSET} bytes"
echo "Mount point: ${MOUNT_POINT}"
