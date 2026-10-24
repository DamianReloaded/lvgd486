#!/bin/bash

# ============================================================
# Configuration
# ============================================================

# Image size in MiB. Change this value as required.
# The MBR partition format imposes a practical limit of
# approximately 2 TiB with 512-byte sectors.
IMAGE_SIZE_MB=700

HEADS=16
SECTORS_PER_TRACK=63

NAME=distrodev

IMAGE="/home/user/images/${NAME}.img"
MOUNT_POINT="/mnt/${NAME}"
LOOP_DEVICE="/dev/loop1"

SECTOR_SIZE=512
PARTITION_START=63
BLOCK_SIZE=1024

PARTITION_FILE="/tmp/${NAME}-partition.$$"
ENTRY_FILE="/tmp/${NAME}-mbr-entry.$$"

# ============================================================
# Initialization
# ============================================================

set -e

IMAGE_CREATED=0
LOOP_ATTACHED=0
FILESYSTEM_MOUNTED=0

fail()
{
    echo "ERROR: $*" >&2
    exit 1
}

cleanup()
{
    status=$?
    trap - EXIT

    if [ "$status" -ne 0 ]; then
        echo
        echo "Creation failed; cleaning up..."

        if [ "$FILESYSTEM_MOUNTED" -eq 1 ]; then
            if umount "$MOUNT_POINT"; then
                FILESYSTEM_MOUNTED=0
            else
                echo "WARNING: could not unmount $MOUNT_POINT" >&2
            fi
        fi

        if [ "$LOOP_ATTACHED" -eq 1 ] &&
           [ "$FILESYSTEM_MOUNTED" -eq 0 ]; then
            if losetup -d "$LOOP_DEVICE"; then
                LOOP_ATTACHED=0
            else
                echo "WARNING: could not detach $LOOP_DEVICE" >&2
            fi
        fi

        # Never remove the image while it is still attached or mounted.
        if [ "$IMAGE_CREATED" -eq 1 ] &&
           [ "$LOOP_ATTACHED" -eq 0 ] &&
           [ "$FILESYSTEM_MOUNTED" -eq 0 ]; then
            rm -f "$IMAGE"
        fi
    fi

    rm -f "$PARTITION_FILE" "$ENTRY_FILE"
    exit "$status"
}

trap cleanup EXIT

# ============================================================
# Validate configuration and prerequisites
# ============================================================

[ "$(id -u)" -eq 0 ] ||
    fail "Run this script as root."

case "$IMAGE_SIZE_MB" in
    ''|*[!0-9]*)
        fail "IMAGE_SIZE_MB must be a positive whole number."
        ;;
esac

awk -v n="$IMAGE_SIZE_MB" '
BEGIN {
    if (n <= 0 || n != int(n) || n > 2097152)
        exit 1
}
' ||
    fail "IMAGE_SIZE_MB must be between 1 and 2097152 MiB."

case "$HEADS" in
    ''|*[!0-9]*)
        fail "HEADS must be a positive whole number."
        ;;
esac

case "$SECTORS_PER_TRACK" in
    ''|*[!0-9]*)
        fail "SECTORS_PER_TRACK must be a positive whole number."
        ;;
esac

[ "$HEADS" -gt 0 ] ||
    fail "HEADS must be greater than zero."

[ "$HEADS" -le 255 ] ||
    fail "HEADS must not exceed 255."

[ "$SECTORS_PER_TRACK" -gt 0 ] &&
[ "$SECTORS_PER_TRACK" -le 63 ] ||
    fail "SECTORS_PER_TRACK must be between 1 and 63."

[ "$SECTOR_SIZE" -eq 512 ] ||
    fail "This MBR layout requires 512-byte sectors."

[ "$BLOCK_SIZE" -eq 1024 ] ||
    fail "This script expects 1024-byte filesystem blocks."

[ "$PARTITION_START" -eq 63 ] ||
    fail "This MBR layout expects partition start sector 63."

for command in \
    id dirname dd od awk cmp wc tr grep losetup mke2fs e2fsck \
    mount umount mkdir rm printf perl
do
    command -v "$command" >/dev/null 2>&1 ||
        fail "Required command not found: $command"
done

[ -d "$(dirname "$IMAGE")" ] ||
    fail "Image directory does not exist: $(dirname "$IMAGE")"

# ============================================================
# Calculate image geometry and partition dimensions
#
# AWK is used for large-number calculations to avoid relying
# on shell arithmetic for potentially large LBA values.
#
# The geometry preserves the original 16 heads, 63 sectors
# per track, whole-cylinder layout and partition start at LBA 63.
# ============================================================

set -- $(awk \
    -v mb="$IMAGE_SIZE_MB" \
    -v sector_size="$SECTOR_SIZE" \
    -v heads="$HEADS" \
    -v spt="$SECTORS_PER_TRACK" \
    -v part_start="$PARTITION_START" \
    -v block_size="$BLOCK_SIZE" '
BEGIN {
    image_bytes = mb * 1024 * 1024
    image_sectors = image_bytes / sector_size

    sectors_per_cylinder = heads * spt

    cylinders = int(image_sectors / sectors_per_cylinder)
    geometry_sectors = cylinders * sectors_per_cylinder

    # Round down to an even number of sectors so the partition
    # size is aligned to 1024-byte filesystem blocks.
    partition_size = int((geometry_sectors - part_start) / 2) * 2

    partition_end = part_start + partition_size - 1
    partition_bytes = partition_size * sector_size
    partition_blocks = partition_bytes / block_size
    offset_bytes = part_start * sector_size
    image_kib = image_bytes / 1024
    fs_magic_offset = offset_bytes + 1080

    printf "%.0f\n", image_bytes
    printf "%.0f\n", image_sectors
    printf "%.0f\n", cylinders
    printf "%.0f\n", geometry_sectors
    printf "%.0f\n", partition_size
    printf "%.0f\n", partition_end
    printf "%.0f\n", partition_bytes
    printf "%.0f\n", partition_blocks
    printf "%.0f\n", offset_bytes
    printf "%.0f\n", image_kib
    printf "%.0f\n", fs_magic_offset
}')

IMAGE_BYTES=$1
IMAGE_SECTORS=$2
CYLINDERS=$3
GEOMETRY_SECTORS=$4
PARTITION_SIZE=$5
PARTITION_END=$6
PARTITION_BYTES=$7
PARTITION_BLOCKS=$8
OFFSET_BYTES=$9
shift 9
IMAGE_KIB=$1
FS_MAGIC_OFFSET=$2

# Validate calculated dimensions using AWK rather than potentially
# overflowing shell integer comparisons.

awk \
    -v image_sectors="$IMAGE_SECTORS" \
    -v geometry_sectors="$GEOMETRY_SECTORS" \
    -v part_start="$PARTITION_START" \
    -v part_size="$PARTITION_SIZE" \
    -v part_end="$PARTITION_END" \
    -v part_bytes="$PARTITION_BYTES" \
    -v part_blocks="$PARTITION_BLOCKS" \
    -v block_size="$BLOCK_SIZE" '
BEGIN {
    if (part_size <= 0) {
        print "ERROR: Image is too small to contain a partition." > "/dev/stderr"
        exit 1
    }

    if (part_end >= image_sectors || part_end >= geometry_sectors) {
        print "ERROR: Calculated partition extends beyond the image geometry." > "/dev/stderr"
        exit 1
    }

    if (part_start < 1 || part_start > 4294967295) {
        print "ERROR: Partition start exceeds the MBR LBA limit." > "/dev/stderr"
        exit 1
    }

    if (part_size > 4294967295) {
        print "ERROR: Partition size exceeds the MBR 32-bit sector-count limit." > "/dev/stderr"
        exit 1
    }

    if (part_bytes % block_size != 0 ||
        part_blocks != part_bytes / block_size) {
        print "ERROR: Partition is not aligned to the filesystem block size." > "/dev/stderr"
        exit 1
    }
}
' || fail "Calculated disk geometry is invalid."

# ============================================================
# Calculate CHS values
# ============================================================

# MBR CHS fields support cylinders 0 through 1023.
# For larger addresses, use saturated CHS values.
# The LBA fields still contain the actual partition boundaries.

calculate_chs()
{
    set -- $(awk \
        -v lba="$1" \
        -v heads="$HEADS" \
        -v spt="$SECTORS_PER_TRACK" '
    BEGIN {
        spc = heads * spt

        cylinder = int(lba / spc)
        remainder = lba % spc

        head = int(remainder / spt)
        sector = (remainder % spt) + 1

        if (cylinder > 1023) {
            cylinder = 1023
            head = 254
            sector = 63
        }

        printf "%.0f\n", cylinder
        printf "%.0f\n", head
        printf "%.0f\n", sector
    }')

    CHS_CYLINDER=$1
    CHS_HEAD=$2
    CHS_SECTOR=$3
}

calculate_chs "$PARTITION_START"

START_CYLINDER=$CHS_CYLINDER
START_HEAD=$CHS_HEAD
START_SECTOR=$CHS_SECTOR

calculate_chs "$PARTITION_END"

END_CYLINDER=$CHS_CYLINDER
END_HEAD=$CHS_HEAD
END_SECTOR=$CHS_SECTOR

# The sector byte contains the upper two cylinder bits.
START_SECTOR_BYTE=$((START_SECTOR | ((START_CYLINDER >> 2) & 192)))
END_SECTOR_BYTE=$((END_SECTOR | ((END_CYLINDER >> 2) & 192)))

# ============================================================
# Calculate MBR LBA bytes
# ============================================================

# Generate individual little-endian bytes using AWK, avoiding
# large Bash bit shifts on a 32-bit system.

lba_byte()
{
    awk -v n="$1" -v shift="$2" '
    BEGIN {
        divisor = 2 ^ shift
        printf "%.0f\n", int(n / divisor) % 256
    }'
}

START_LBA_0=$(lba_byte "$PARTITION_START" 0)
START_LBA_1=$(lba_byte "$PARTITION_START" 8)
START_LBA_2=$(lba_byte "$PARTITION_START" 16)
START_LBA_3=$(lba_byte "$PARTITION_START" 24)

SIZE_LBA_0=$(lba_byte "$PARTITION_SIZE" 0)
SIZE_LBA_1=$(lba_byte "$PARTITION_SIZE" 8)
SIZE_LBA_2=$(lba_byte "$PARTITION_SIZE" 16)
SIZE_LBA_3=$(lba_byte "$PARTITION_SIZE" 24)

# ============================================================
# Validate environment
# ============================================================

[ ! -e "$IMAGE" ] ||
    fail "$IMAGE already exists. Remove the old image before retrying."

if mount | grep -Fq " on $MOUNT_POINT "; then
    fail "$MOUNT_POINT is already mounted."
fi

if losetup "$LOOP_DEVICE" >/dev/null 2>&1; then
    fail "$LOOP_DEVICE is already in use."
fi

echo "Image size:       ${IMAGE_SIZE_MB} MiB"
echo "Image sectors:    $IMAGE_SECTORS"
echo "CHS geometry:     $CYLINDERS/$HEADS/$SECTORS_PER_TRACK"
echo "Geometry sectors: $GEOMETRY_SECTORS"
echo "Partition start:  $PARTITION_START"
echo "Partition size:   $PARTITION_SIZE sectors"
echo "Partition end:    $PARTITION_END"
echo "Filesystem size:  $PARTITION_BLOCKS blocks of ${BLOCK_SIZE} bytes"
echo "Partition offset: $OFFSET_BYTES bytes"
echo

# ============================================================
# Create the disk image
# ============================================================

echo "Creating blank disk image..."

# Mark the image as ours before starting dd so cleanup also removes
# a partially created image if dd fails.
IMAGE_CREATED=1

dd if=/dev/zero \
   of="$IMAGE" \
   bs=1024 \
   count="$IMAGE_KIB"

ACTUAL_BYTES=$(wc -c < "$IMAGE" | tr -d ' ')

[ "$ACTUAL_BYTES" = "$IMAGE_BYTES" ] ||
    fail "Incorrect disk image size."

# ============================================================
# Build the MBR partition entry
# ============================================================

# MBR entry layout:
#   1 byte  boot flag
#   3 bytes start CHS
#   1 byte  partition type
#   3 bytes end CHS
#   4 bytes start LBA (little-endian)
#   4 bytes partition length (little-endian)
#
# Perl pack("C*", ...) writes exactly one byte for every argument.
# This avoids shell printf octal-escape ambiguities.

echo "Writing MBR partition entry..."

perl -e '
    die "Expected exactly 16 byte values\n" unless @ARGV == 16;

    for (@ARGV) {
        die "Invalid byte value: $_\n"
            unless /^\d+$/ && $_ >= 0 && $_ <= 255;
    }

    print pack("C*", @ARGV);
' \
    128 \
    "$START_HEAD" \
    "$START_SECTOR_BYTE" \
    "$((START_CYLINDER & 255))" \
    131 \
    "$END_HEAD" \
    "$END_SECTOR_BYTE" \
    "$((END_CYLINDER & 255))" \
    "$START_LBA_0" \
    "$START_LBA_1" \
    "$START_LBA_2" \
    "$START_LBA_3" \
    "$SIZE_LBA_0" \
    "$SIZE_LBA_1" \
    "$SIZE_LBA_2" \
    "$SIZE_LBA_3" \
    > "$ENTRY_FILE" ||
    fail "Failed to generate MBR partition entry."

ENTRY_BYTES=$(wc -c < "$ENTRY_FILE" | tr -d ' ')

if [ "$ENTRY_BYTES" != "16" ]; then
    echo "Generated entry contents:" >&2
    od -An -tx1 "$ENTRY_FILE" >&2
    fail "MBR partition entry is $ENTRY_BYTES bytes; expected 16."
fi

# Write the verified 16-byte entry to the MBR.
dd if="$ENTRY_FILE" \
   of="$IMAGE" \
   bs=1 \
   seek=446 \
   conv=notrunc

# Write the MBR signature 0x55AA at offsets 510-511.
printf '\125\252' |
    dd of="$IMAGE" bs=1 seek=510 conv=notrunc

# ============================================================
# Validate the MBR
# ============================================================

echo "Validating MBR..."

SIGNATURE=$(od -An -tx1 -j 510 -N 2 "$IMAGE" | tr -d ' \n')

[ "$SIGNATURE" = "55aa" ] ||
    fail "Invalid MBR signature."

dd if="$IMAGE" bs=1 skip=446 count=16 2>/dev/null |
    cmp - "$ENTRY_FILE" ||
    fail "MBR partition entry does not match the expected bytes."

ACTUAL_TYPE=$(od -An -tu1 -j 450 -N 1 "$IMAGE" | tr -d ' ')

ACTUAL_START=$(
    od -An -tu1 -j 454 -N 4 "$IMAGE" |
    awk '{ printf "%.0f\n", $1 + 256*$2 + 65536*$3 + 16777216*$4 }'
)

ACTUAL_SIZE=$(
    od -An -tu1 -j 458 -N 4 "$IMAGE" |
    awk '{ printf "%.0f\n", $1 + 256*$2 + 65536*$3 + 16777216*$4 }'
)

[ "$ACTUAL_TYPE" = "131" ] ||
    fail "Partition type is not Linux (0x83)."

[ "$ACTUAL_START" = "$PARTITION_START" ] ||
    fail "Incorrect partition start LBA."

[ "$ACTUAL_SIZE" = "$PARTITION_SIZE" ] ||
    fail "Incorrect partition size."

echo "MBR validation passed."

# ============================================================
# Create ext2 on an exact-size partition file
# ============================================================

echo "Creating partition-sized temporary file..."

dd if=/dev/zero \
   of="$PARTITION_FILE" \
   bs="$BLOCK_SIZE" \
   count="$PARTITION_BLOCKS"

TEMP_BYTES=$(wc -c < "$PARTITION_FILE" | tr -d ' ')

[ "$TEMP_BYTES" = "$PARTITION_BYTES" ] ||
    fail "Temporary partition file has an incorrect size."

echo "Creating revision-0 ext2 filesystem..."

mke2fs -F -r 0 -b "$BLOCK_SIZE" \
    "$PARTITION_FILE" "$PARTITION_BLOCKS"

# ============================================================
# Validate ext2 superblock
# ============================================================

echo "Validating ext2 superblock..."

FS_MAGIC=$(
    od -An -tx1 -j 1080 -N 2 "$PARTITION_FILE" |
    tr -d ' \n'
)

[ "$FS_MAGIC" = "53ef" ] ||
    fail "ext2 magic is missing from the newly formatted partition file."

e2fsck -fn "$PARTITION_FILE" ||
    fail "Filesystem consistency check failed."

echo "Temporary ext2 filesystem validated."

# ============================================================
# Copy filesystem into the disk image
# ============================================================

echo "Copying filesystem into the disk image at LBA $PARTITION_START..."

dd if="$PARTITION_FILE" \
   of="$IMAGE" \
   bs="$SECTOR_SIZE" \
   seek="$PARTITION_START" \
   conv=notrunc

IMAGE_FS_MAGIC=$(
    od -An -tx1 -j "$FS_MAGIC_OFFSET" -N 2 "$IMAGE" |
    tr -d ' \n'
)

[ "$IMAGE_FS_MAGIC" = "53ef" ] ||
    fail "ext2 magic is missing at the partition offset in the image."

echo "Filesystem is present at the correct image offset."

# ============================================================
# Attach partition offset
# ============================================================

echo "Attaching image partition to $LOOP_DEVICE..."

losetup -o "$OFFSET_BYTES" "$LOOP_DEVICE" "$IMAGE"
LOOP_ATTACHED=1

# ============================================================
# Validate filesystem through loop device
# ============================================================

echo "Checking filesystem through loop device..."

e2fsck -fn "$LOOP_DEVICE" ||
    fail "Filesystem validation through loop device failed."

# ============================================================
# Mount filesystem
# ============================================================

mkdir -p "$MOUNT_POINT"

echo "Mounting ext2 filesystem..."

mount -t ext2 "$LOOP_DEVICE" "$MOUNT_POINT"
FILESYSTEM_MOUNTED=1

if ! mount | grep -Fq " on $MOUNT_POINT "; then
    fail "Filesystem did not mount at $MOUNT_POINT."
fi

# ============================================================
# Final report
# ============================================================

echo
echo "Disk image created and validated successfully."
echo "Image:          $IMAGE"
echo "Image size:     $ACTUAL_BYTES bytes"
echo "CHS geometry:   $CYLINDERS/$HEADS/$SECTORS_PER_TRACK"
echo "Partition:      ${PARTITION_START}-${PARTITION_END}"
echo "Partition size: $PARTITION_SIZE sectors"
echo "Filesystem:     ext2 revision 0"
echo "Ext2 magic:     53 ef"
echo "Loop device:    $LOOP_DEVICE"
echo "Mount point:    $MOUNT_POINT"
echo
echo "The filesystem is mounted and ready for copying files."
echo "For example:"
echo "  cp -a /home/user/distrodev/. $MOUNT_POINT/"
echo "  sync"