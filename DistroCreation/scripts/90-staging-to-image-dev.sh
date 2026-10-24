#!/bin/sh

# ============================================================
# LVGD486 - Copy Staging to Development Image
# Script: 90-staging-to-image-dev.sh
# ============================================================

set -e

# Constants
STAGING_DIR="/home/user/distrodev"
IMAGE_FILE="/home/user/images/distrodev.img"
MOUNT_DIR="/mnt/distrodev"

fail()
{
    echo "Error: $*" >&2
    exit 1
}

# Require root permissions
if [ "$(id -u)" -ne 0 ]; then
    fail "Run this script as root."
fi

# Verify staging directory
if [ ! -d "$STAGING_DIR" ]; then
    fail "Staging directory not found: ${STAGING_DIR}"
fi

# Verify image file
if [ ! -f "$IMAGE_FILE" ]; then
    fail "Development image not found: ${IMAGE_FILE}"
fi

# Verify mount point
if [ ! -d "$MOUNT_DIR" ]; then
    fail "Mount directory not found: ${MOUNT_DIR}"
fi

# Verify that the destination is mounted
if ! awk -v mount="$MOUNT_DIR" '$2 == mount { found = 1 } END { exit !found }' /proc/mounts; then
    fail "Nothing is mounted at ${MOUNT_DIR}. Mount ${IMAGE_FILE} first."
fi

# Confirm staging source is not the destination
if [ "$(readlink -f "$STAGING_DIR")" = "$(readlink -f "$MOUNT_DIR")" ]; then
    fail "Staging directory and destination are the same."
fi

echo "Staging directory: ${STAGING_DIR}"
echo "Mounted image:     ${IMAGE_FILE}"
echo "Destination:       ${MOUNT_DIR}"
echo
echo "Deleting destination contents..."

# Delete all contents, including hidden files, preserving the mount point
for entry in "$MOUNT_DIR"/* "$MOUNT_DIR"/.[!.]* "$MOUNT_DIR"/..?*
do
    if [ -e "$entry" ] || [ -L "$entry" ]; then
        rm -rf "$entry"
    fi
done

echo "Copying staging contents..."

# Copy all files, including hidden files, preserving metadata and symlinks
cp -a "$STAGING_DIR"/. "$MOUNT_DIR"/

echo
echo "Development image updated successfully."