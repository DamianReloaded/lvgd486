#!/bin/sh

# ============================================================
# LVGD486 - Install distrodev filesystem and GRUB Legacy
# ============================================================

set -eu

# ------------------------------------------------------------
# Environment
# ------------------------------------------------------------

PATH=/bin:/sbin:/usr/bin:/usr/sbin
export PATH

ARCHIVE="${ARCHIVE:-/install-media/distrodev.tar.gz}"
GRUB="${GRUB:-/bin/grub}"
MOUNT_POINT="/mnt/lvgd486-target"
LOCAL_ARCHIVE="${MOUNT_POINT}/.distrodev.tar.gz"

# Optional override for the GRUB BIOS disk number.
# If unset, devfs /dev/ide/hostN paths use N.
GRUB_DISK="${GRUB_DISK:-}"

MOUNT=""
UMOUNT=""
TAR=""
GREP=""
MKDIR=""
CP=""
RM=""
SYNC=""
ID=""

MOUNTED=0
MAP=""

# ------------------------------------------------------------
# Error handling and cleanup
# ------------------------------------------------------------

fail()
{
    echo "ERROR: $*" >&2
    exit 1
}

cleanup()
{
    status=$?

    trap - 0 HUP INT TERM

    if [ "$MOUNTED" -eq 1 ] && [ -n "$UMOUNT" ]; then
        "$UMOUNT" "$MOUNT_POINT" ||
            echo "WARNING: Could not unmount $MOUNT_POINT" >&2
        MOUNTED=0
    fi

    if [ -n "$MAP" ] && [ -n "$RM" ]; then
        "$RM" -f "$MAP" ||
            echo "WARNING: Could not remove $MAP" >&2
    fi

    exit "$status"
}

trap cleanup 0
trap 'exit 129' HUP
trap 'exit 130' INT
trap 'exit 143' TERM

# ------------------------------------------------------------
# Locate required commands
# ------------------------------------------------------------

find_command()
{
    command_name=$1
    shift

    for candidate
    do
        if [ -x "$candidate" ] && [ ! -d "$candidate" ]; then
            printf '%s\n' "$candidate"
            return 0
        fi
    done

    return 1
}

MOUNT=$(find_command mount /bin/mount /sbin/mount) ||
    fail "mount is missing or not executable."

UMOUNT=$(find_command umount /bin/umount /sbin/umount) ||
    fail "umount is missing or not executable."

TAR=$(find_command tar /bin/tar /usr/bin/tar) ||
    fail "tar is missing or not executable."

GREP=$(find_command grep /bin/grep /usr/bin/grep) ||
    fail "grep is missing or not executable."

MKDIR=$(find_command mkdir /bin/mkdir /usr/bin/mkdir) ||
    fail "mkdir is missing or not executable."

CP=$(find_command cp /bin/cp /usr/bin/cp) ||
    fail "cp is missing or not executable."

RM=$(find_command rm /bin/rm /usr/bin/rm) ||
    fail "rm is missing or not executable."

SYNC=$(find_command sync /bin/sync /usr/bin/sync) ||
    fail "sync is missing or not executable."

ID=$(find_command id /bin/id /usr/bin/id) ||
    fail "id is missing; cannot verify root privileges."

# sleep is normally provided by BusyBox as an applet.
SLEEP=$(find_command sleep /bin/sleep /usr/bin/sleep) ||
    fail "sleep is missing; required for unmount retries."

# ------------------------------------------------------------
# Locate GRUB Legacy
# ------------------------------------------------------------

case "$GRUB" in
    */*)
        [ -x "$GRUB" ] ||
            fail "GRUB Legacy shell is missing or not executable: $GRUB"
        ;;

    *)
        GRUB_PATH=$(find_command "$GRUB" \
            "/bin/$GRUB" \
            "/sbin/$GRUB" \
            "/usr/bin/$GRUB" \
            "/usr/sbin/$GRUB") ||
            fail "GRUB Legacy shell not found: $GRUB"

        GRUB=$GRUB_PATH
        ;;
esac

# ------------------------------------------------------------
# Validate privileges and arguments
# ------------------------------------------------------------

[ "$("$ID" -u)" = "0" ] ||
    fail "Run this installer as root."

[ "$#" -eq 1 ] ||
    fail "Usage: install.sh /dev/PARTITION"

TARGET_PART=$1

[ -b "$TARGET_PART" ] ||
    fail "Not a block device: $TARGET_PART"

[ -f "$ARCHIVE" ] ||
    fail "Filesystem archive not found: $ARCHIVE"

[ -r "$ARCHIVE" ] ||
    fail "Filesystem archive is not readable: $ARCHIVE"

[ -r /proc/mounts ] ||
    fail "/proc/mounts is unavailable; mount proc before installing."

# ------------------------------------------------------------
# Determine parent disk and partition number
#
# Supported naming schemes:
#   /dev/sda2
#   /dev/hda2
#   /dev/vda2
#   /dev/xvda2
#   /dev/mmcblk0p2
#   /dev/nvme0n1p2
#   /dev/ide/host0/bus0/target0/lun0/part2
#
# For devfs, the host number is the default GRUB disk number.
# GRUB_DISK can override the default for BIOS disk ordering.
# ------------------------------------------------------------

case "$TARGET_PART" in
    /dev/ide/host[0-9]*/bus[0-9]*/target[0-9]*/lun[0-9]*/part[0-9]*)
        HOST_NUM=${TARGET_PART#/dev/ide/host}
        HOST_NUM=${HOST_NUM%%/*}

        case "$HOST_NUM" in
            ''|*[!0-9]*)
                fail "Cannot determine IDE host number from $TARGET_PART."
                ;;
        esac

        TARGET_DISK="${TARGET_PART%/part[0-9]*}/disc"
        PART_NUM="${TARGET_PART##*/part}"

        if [ -z "$GRUB_DISK" ]; then
            GRUB_DISK=$HOST_NUM
        fi
        ;;

    /dev/mmcblk[0-9]*p[0-9]*)
        TARGET_DISK="${TARGET_PART%p[0-9]*}"
        PART_NUM="${TARGET_PART##*p}"
        ;;

    /dev/nvme*n[0-9]p[0-9]*)
        TARGET_DISK="${TARGET_PART%p[0-9]*}"
        PART_NUM="${TARGET_PART##*p}"
        ;;

    /dev/xvd[a-z][0-9]*)
        TARGET_DISK="${TARGET_PART%[0-9]*}"
        PART_NUM="${TARGET_PART##*[!0-9]}"
        ;;

    /dev/[shv]d[a-z][0-9]*)
        TARGET_DISK="${TARGET_PART%[0-9]*}"
        PART_NUM="${TARGET_PART##*[!0-9]}"
        ;;

    *)
        fail "Unsupported partition device naming scheme: $TARGET_PART"
        ;;
esac

# Non-devfs naming schemes default to BIOS disk zero.
if [ -z "$GRUB_DISK" ]; then
    GRUB_DISK=0
fi

case "$GRUB_DISK" in
    ''|*[!0-9]*)
        fail "GRUB_DISK must be a non-negative integer: $GRUB_DISK"
        ;;
esac

case "$PART_NUM" in
    ''|*[!0-9]*)
        fail "Cannot determine a valid partition number from $TARGET_PART."
        ;;
esac

[ "$PART_NUM" -gt 0 ] ||
    fail "Invalid target partition number: $PART_NUM"

[ -b "$TARGET_DISK" ] ||
    fail "Parent disk is not a block device: $TARGET_DISK"

# GRUB Legacy partition numbers start at zero.
GRUB_PART=$((PART_NUM - 1))

GRUB_ROOT="(hd${GRUB_DISK},${GRUB_PART})"
GRUB_DISK_ROOT="(hd${GRUB_DISK})"

MENU_FILE="${MOUNT_POINT}/boot/grub/menu.lst"
LEGACY_MENU_FILE="${MOUNT_POINT}/boot/menu.lst"

# ------------------------------------------------------------
# Mount safety checks
# ------------------------------------------------------------

if "$GREP" -F " $TARGET_PART " /proc/mounts >/dev/null 2>&1; then
    fail "$TARGET_PART is already mounted."
fi

"$MKDIR" -p "$MOUNT_POINT"

if "$GREP" -F " $MOUNT_POINT " /proc/mounts >/dev/null 2>&1; then
    fail "$MOUNT_POINT is already mounted."
fi

# ------------------------------------------------------------
# Confirmation
# ------------------------------------------------------------

echo
echo "LVGD486 installation"
echo "--------------------"
echo "Target partition:     $TARGET_PART"
echo "Parent disk:          $TARGET_DISK"
echo "GRUB BIOS disk:       $GRUB_DISK"
echo "GRUB partition:       $GRUB_ROOT"
echo "GRUB MBR destination: $GRUB_DISK_ROOT"
echo "Source archive:       $ARCHIVE"
echo "Local archive:        $LOCAL_ARCHIVE"
echo
echo "Installation procedure:"
echo "  1. Mount the destination partition."
echo "  2. Copy the archive to the destination partition."
echo "  3. Extract the local archive."
echo "  4. Generate GRUB menu.lst for the target partition."
echo "  5. Install GRUB Legacy to the selected disk."
echo
echo "WARNING: Existing files may be overwritten."
echo "WARNING: GRUB will modify the MBR of $TARGET_DISK."
echo
echo "NOTE: The default BIOS disk number is inferred from"
echo "      the IDE host number for devfs device paths."
echo "      Override it with GRUB_DISK if BIOS ordering differs."
echo

if [ "${LVGD486_CONFIRM:-}" != "YES" ]; then
    printf 'Type INSTALL to continue: '
    IFS= read -r answer || fail "Could not read confirmation."

    [ "$answer" = "INSTALL" ] ||
        fail "Installation cancelled."
fi

# ------------------------------------------------------------
# Mount target filesystem
# ------------------------------------------------------------

echo
echo "Mounting target partition..."

"$MOUNT" -t ext2 "$TARGET_PART" "$MOUNT_POINT" ||
    fail "Could not mount $TARGET_PART as ext2."

MOUNTED=1

[ -d "$MOUNT_POINT" ] ||
    fail "Target mount point is not a directory."

[ -w "$MOUNT_POINT" ] ||
    fail "Target filesystem is not writable."

# ------------------------------------------------------------
# Copy archive to destination filesystem
# ------------------------------------------------------------

echo
echo "Copying filesystem archive to destination..."
echo "Source:      $ARCHIVE"
echo "Destination: $LOCAL_ARCHIVE"

"$CP" "$ARCHIVE" "$LOCAL_ARCHIVE" ||
    fail "Could not copy filesystem archive to the destination."

[ -f "$LOCAL_ARCHIVE" ] ||
    fail "The local filesystem archive was not created."

[ -s "$LOCAL_ARCHIVE" ] ||
    fail "The local filesystem archive is empty."

echo "Archive copy completed."

# ------------------------------------------------------------
# Extract local archive
# ------------------------------------------------------------

echo
echo "Extracting local filesystem archive..."
echo "Reading from: $LOCAL_ARCHIVE"

"$TAR" -xzpf "$LOCAL_ARCHIVE" -C "$MOUNT_POINT" ||
    fail "Filesystem archive extraction failed."

echo "Filesystem archive extraction completed."

# ------------------------------------------------------------
# Remove temporary archive after successful extraction
# ------------------------------------------------------------

echo
echo "Removing temporary archive..."

"$RM" -f "$LOCAL_ARCHIVE" ||
    fail "Could not remove temporary archive: $LOCAL_ARCHIVE"

[ ! -f "$LOCAL_ARCHIVE" ] ||
    fail "Temporary archive still exists: $LOCAL_ARCHIVE"

echo "Temporary archive removed."

# ------------------------------------------------------------
# Verify kernel image
# ------------------------------------------------------------

[ -s "$MOUNT_POINT/boot/bzImage" ] ||
    fail "Kernel image is missing or empty: /boot/bzImage"

# ------------------------------------------------------------
# Install GRUB Legacy stage files
# ------------------------------------------------------------

"$MKDIR" -p "$MOUNT_POINT/boot/grub"

if [ ! -f "$MOUNT_POINT/boot/grub/stage1" ] ||
   [ ! -f "$MOUNT_POINT/boot/grub/stage2" ]; then

    [ -s /install-media/boot/grub/stage1 ] ||
        fail "GRUB stage1 is missing from target and installer media."

    [ -s /install-media/boot/grub/stage2 ] ||
        fail "GRUB stage2 is missing from target and installer media."

    echo
    echo "Copying GRUB Legacy stage files from installer media..."

    "$CP" /install-media/boot/grub/stage1 \
          /install-media/boot/grub/stage2 \
          "$MOUNT_POINT/boot/grub/" ||
        fail "Could not copy GRUB Legacy stage files."
fi

[ -s "$MOUNT_POINT/boot/grub/stage1" ] ||
    fail "GRUB stage1 is empty."

[ -s "$MOUNT_POINT/boot/grub/stage2" ] ||
    fail "GRUB stage2 is empty."

# ------------------------------------------------------------
# Generate GRUB Legacy menu.lst
# ------------------------------------------------------------

echo
echo "Generating GRUB Legacy menu..."

{
    printf '%s\n' "default 0"
    printf '%s\n' "timeout 5"
    printf '\n'
    printf '%s\n' "title LVGD486 Linux"
    printf 'root %s\n' "$GRUB_ROOT"
    printf 'kernel %s/boot/bzImage root=%s rw sb=0x220,5,1,5,0x330 \n' \
        "$GRUB_ROOT" "$TARGET_PART"
} > "$MENU_FILE" ||
    fail "Could not generate $MENU_FILE."

# Keep the legacy path in sync for compatibility.
"$CP" "$MENU_FILE" "$LEGACY_MENU_FILE" ||
    fail "Could not write compatibility menu: $LEGACY_MENU_FILE"

[ -s "$MENU_FILE" ] ||
    fail "Generated GRUB menu is empty."

echo
echo "Generated GRUB menu:"
echo "--------------------"
cat "$MENU_FILE"
echo "--------------------"

# ------------------------------------------------------------
# Prepare GRUB Legacy device map
#
# This maps the selected GRUB BIOS disk number to the Linux
# parent disk. Do not map it to hd0 unconditionally.
# ------------------------------------------------------------

MAP="/tmp/lvgd486-device-map.$$"

printf '(hd%s) %s\n' "$GRUB_DISK" "$TARGET_DISK" > "$MAP" ||
    fail "Could not create GRUB device map."

# ------------------------------------------------------------
# Install GRUB Legacy
# ------------------------------------------------------------

echo
echo "Installing GRUB Legacy..."
echo "GRUB executable: $GRUB"
echo "Device map:      $MAP"
echo "Root partition:  $GRUB_ROOT"
echo "Target disk:     $TARGET_DISK"
echo

"$GRUB" --batch --device-map="$MAP" <<GRUBEOF
root $GRUB_ROOT
setup $GRUB_DISK_ROOT
quit
GRUBEOF

echo
echo "GRUB Legacy installation command completed."

# ------------------------------------------------------------
# Flush writes and unmount
# ------------------------------------------------------------

echo
echo "Flushing filesystem writes..."

"$SYNC"

echo "Unmounting target filesystem..."

UMOUNT_TIMEOUT=30
UMOUNT_ELAPSED=0

while ! "$UMOUNT" "$MOUNT_POINT"; do
    if [ "$UMOUNT_ELAPSED" -ge "$UMOUNT_TIMEOUT" ]; then
        fail "Could not unmount $MOUNT_POINT after ${UMOUNT_TIMEOUT} seconds."
    fi

    UMOUNT_ELAPSED=$((UMOUNT_ELAPSED + 1))

    echo "Filesystem is busy; retrying in 1 second (${UMOUNT_ELAPSED}/${UMOUNT_TIMEOUT})..."

    "$SLEEP" 1
done

MOUNTED=0

echo "Target filesystem unmounted successfully."

# ------------------------------------------------------------
# Remove temporary GRUB device map
# ------------------------------------------------------------

"$RM" -f "$MAP" ||
    fail "Could not remove temporary GRUB device map."

MAP=""

# ------------------------------------------------------------
# Completion
# ------------------------------------------------------------

echo
echo "============================================================"
echo "LVGD486 installation complete."
echo "============================================================"
echo "System files installed on: $TARGET_PART"
echo "GRUB Legacy target disk:   $TARGET_DISK"
echo "GRUB BIOS disk:            $GRUB_DISK"
echo "GRUB partition:            $GRUB_ROOT"
echo "Generated menu:            /boot/grub/menu.lst"
echo "Temporary archive:         removed"
echo
echo "Remove the installation media and reboot."