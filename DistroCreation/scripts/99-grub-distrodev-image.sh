#!/bin/bash

# ============================================================
# LVGD486 - Install GRUB Legacy to distrodev image
# Script: 51-grub-distrodev-image.sh
# ============================================================

# ============================================================
# Configuration
# ============================================================

NAME=distrodev

IMAGE="/home/user/images/${NAME}.img"
IMAGE_MOUNT="/mnt/${NAME}"
LOOP_DEVICE="/dev/loop1"

GRUB_STAGE_SOURCE="/home/user/doc/grub-stage-files"
GRUB_STAGE_DEST="${IMAGE_MOUNT}/boot/grub"

GRUB_DEVICE_MAP="/tmp/grub-disk.map"

GRUB_DISK="(hd0)"
GRUB_ROOT="(hd0,0)"

LINUX_ROOT="/dev/hda1"
KERNEL_PATH="/boot/bzImage"
KERNEL_PARAMS="rw quiet loglevel=0"

# Must match the 86Box disk geometry.
CYLINDERS=1422
HEADS=16
SECTORS=63

# Must match the partition created by the image creation script.
EXPECTED_START=63
EXPECTED_SIZE=1433312

GRUB_LOG="/tmp/grub-install.log"
MBR_BACKUP="/tmp/grub-mbr-backup.bin"

# ============================================================
# Initialization
# ============================================================

set -e

fail()
{
    echo "ERROR: $*"
    exit 1
}

for command in grub od awk cmp dd wc tr grep cp mkdir chmod \
               mount losetup sync; do
    command -v "$command" >/dev/null 2>&1 ||
        fail "Required command not found: $command"
done

[ "$(id -u)" -eq 0 ] ||
    fail "Run this script as root."

[ -f "$IMAGE" ] || fail "Disk image not found: $IMAGE"
[ -r "$IMAGE" ] || fail "Image is not readable."
[ -w "$IMAGE" ] || fail "Image is not writable."

# ============================================================
# Verify mounted image partition
# ============================================================

echo "Checking mounted image partition..."

[ -d "$IMAGE_MOUNT" ] ||
    fail "Mount point does not exist: $IMAGE_MOUNT"

MOUNT_SOURCE=$(awk -v target="$IMAGE_MOUNT" \
    '$2 == target { print $1; exit }' /proc/mounts)

[ -n "$MOUNT_SOURCE" ] ||
    fail "No filesystem is mounted at $IMAGE_MOUNT."

LOOP_INFO=$(losetup "$LOOP_DEVICE" 2>/dev/null) ||
    fail "$LOOP_DEVICE is not configured."

echo "Mounted source: $MOUNT_SOURCE"
echo "Loop device:    $LOOP_INFO"

case "$MOUNT_SOURCE" in
    "$LOOP_DEVICE"|"$LOOP_DEVICE"p1)
        ;;
    *)
        fail "$IMAGE_MOUNT is mounted from $MOUNT_SOURCE, not $LOOP_DEVICE."
        ;;
esac

case "$LOOP_INFO" in
    *"$IMAGE"*)
        ;;
    *)
        fail "$LOOP_DEVICE does not appear to reference $IMAGE."
        ;;
esac

# ============================================================
# Copy GRUB stage files into the mounted image
# ============================================================

echo
echo "Copying GRUB stage files into the image..."

[ -d "$GRUB_STAGE_SOURCE" ] ||
    fail "GRUB stage source directory not found: $GRUB_STAGE_SOURCE"

mkdir -p "$GRUB_STAGE_DEST"

for file in stage1 stage2 e2fs_stage1_5; do
    [ -s "${GRUB_STAGE_SOURCE}/${file}" ] ||
        fail "Missing or empty source stage file: ${GRUB_STAGE_SOURCE}/${file}"

    cp "${GRUB_STAGE_SOURCE}/${file}" "${GRUB_STAGE_DEST}/${file}"
    chmod 0644 "${GRUB_STAGE_DEST}/${file}"

    cmp -s "${GRUB_STAGE_SOURCE}/${file}" \
           "${GRUB_STAGE_DEST}/${file}" ||
        fail "Stage file verification failed: $file"

    echo "Installed and verified: $file"
done

sync

echo "GRUB stage files are present in the mounted image."
echo

# ============================================================
# Validate image size and geometry
# ============================================================

IMAGE_BYTES=$(wc -c < "$IMAGE" | tr -d ' ')

[ "$IMAGE_BYTES" -eq 734003200 ] ||
    fail "Expected a 700 MiB image; found $IMAGE_BYTES bytes."

IMAGE_SECTORS=$((IMAGE_BYTES / 512))
GEOMETRY_SECTORS=$((CYLINDERS * HEADS * SECTORS))

[ "$IMAGE_SECTORS" -ge "$GEOMETRY_SECTORS" ] ||
    fail "Image is smaller than the configured disk geometry."

# ============================================================
# Validate MBR signature
# ============================================================

echo "Validating MBR..."

SIGNATURE=$(od -An -tx1 -j 510 -N 2 "$IMAGE" | tr -d ' \n')

[ "$SIGNATURE" = "55aa" ] ||
    fail "Invalid MBR signature: $SIGNATURE"

# ============================================================
# Read partition table fields
# ============================================================

PARTITION_TYPE=$(od -An -tu1 -j 450 -N 1 "$IMAGE" | tr -d ' ')

ACTUAL_START=$(
    od -An -tu1 -j 454 -N 4 "$IMAGE" |
    awk '{ print $1 + 256*$2 + 65536*$3 + 16777216*$4 }'
)

ACTUAL_SIZE=$(
    od -An -tu1 -j 458 -N 4 "$IMAGE" |
    awk '{ print $1 + 256*$2 + 65536*$3 + 16777216*$4 }'
)

[ "$PARTITION_TYPE" -eq 131 ] ||
    fail "Partition 1 is not type 0x83 (Linux)."

[ "$ACTUAL_START" -eq "$EXPECTED_START" ] ||
    fail "Partition start is $ACTUAL_START, expected $EXPECTED_START."

[ "$ACTUAL_SIZE" -eq "$EXPECTED_SIZE" ] ||
    fail "Partition size is $ACTUAL_SIZE, expected $EXPECTED_SIZE."

PARTITION_END=$((ACTUAL_START + ACTUAL_SIZE - 1))

[ "$PARTITION_END" -lt "$GEOMETRY_SECTORS" ] ||
    fail "Partition extends beyond the configured CHS geometry."

[ "$PARTITION_END" -lt "$IMAGE_SECTORS" ] ||
    fail "Partition extends beyond the image."

echo "MBR signature: valid"
echo "Partition type: Linux (0x83)"
echo "Partition start: $ACTUAL_START"
echo "Partition size: $ACTUAL_SIZE sectors"
echo "Partition end: $PARTITION_END"
echo "Image size: $IMAGE_SECTORS sectors"
echo "Geometry: ${CYLINDERS}/${HEADS}/${SECTORS}"
echo

# ============================================================
# Prepare GRUB device map
# ============================================================

echo "Creating GRUB device map..."

cat > "$GRUB_DEVICE_MAP" <<EOF
$GRUB_DISK $IMAGE
EOF

# Save the complete MBR so we can verify that GRUB preserves
# the partition table after installing its boot code.
dd if="$IMAGE" of="$MBR_BACKUP" bs=512 count=1 2>/dev/null

# ============================================================
# Install GRUB Legacy
# ============================================================

echo "Installing GRUB Legacy to image: $IMAGE"
echo "Geometry: ${CYLINDERS}/${HEADS}/${SECTORS}"
echo

if grub --batch --no-curses \
    --device-map="$GRUB_DEVICE_MAP" > "$GRUB_LOG" 2>&1 <<EOF
geometry $GRUB_DISK $CYLINDERS $HEADS $SECTORS $GEOMETRY_SECTORS
root $GRUB_ROOT
setup $GRUB_DISK
quit
EOF
then
    GRUB_EXIT=0
else
    GRUB_EXIT=$?
fi

cat "$GRUB_LOG"
echo

if [ "$GRUB_EXIT" -ne 0 ]; then
    fail "GRUB exited with status $GRUB_EXIT. See $GRUB_LOG"
fi

if grep -E 'Error [0-9]+:|Fatal error|cannot find|No such file' \
    "$GRUB_LOG" >/dev/null 2>&1; then
    fail "GRUB reported an installation error. See $GRUB_LOG"
fi

# ============================================================
# Verify the partition table survived installation
# ============================================================

echo "Verifying the MBR after installation..."

dd if="$IMAGE" of="${MBR_BACKUP}.after" bs=512 count=1 2>/dev/null

dd if="$IMAGE" of="${MBR_BACKUP}.partition" \
    bs=1 skip=446 count=66 2>/dev/null

dd if="$MBR_BACKUP" of="${MBR_BACKUP}.original-partition" \
    bs=1 skip=446 count=66 2>/dev/null

if ! cmp -s "${MBR_BACKUP}.partition" \
            "${MBR_BACKUP}.original-partition"; then
    fail "The partition table or MBR signature changed unexpectedly."
fi

# ============================================================
# Final report
# ============================================================

rm -f "${MBR_BACKUP}.after" \
      "${MBR_BACKUP}.partition" \
      "${MBR_BACKUP}.original-partition"

echo
echo "GRUB installation commands completed without reported errors."
echo "Image:       $IMAGE"
echo "Mount point: $IMAGE_MOUNT"
echo "Stage source: $GRUB_STAGE_SOURCE"
echo "Stage target: $GRUB_STAGE_DEST"
echo "GRUB disk:   $GRUB_DISK"
echo "GRUB root:   $GRUB_ROOT"
echo
echo "Expected menu.lst entries:"
echo "  root $GRUB_ROOT"
echo "  kernel $KERNEL_PATH root=$LINUX_ROOT $KERNEL_PARAMS"
echo
echo "A successful boot in 86Box is the final confirmation."