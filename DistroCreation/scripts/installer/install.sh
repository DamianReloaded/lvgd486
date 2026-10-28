#!/bin/sh
# LVGD486 - Install the distrodev filesystem and GRUB Legacy
set -eu

ARCHIVE="${ARCHIVE:-/install-media/distrodev.tar.gz}"
GRUB="${GRUB:-grub}"

fail() { echo "ERROR: $*" >&2; exit 1; }
[ "$(id -u)" -eq 0 ] || fail "Run as root."
[ "$#" -eq 1 ] || fail "Usage: install.sh /dev/PARTITION"
TARGET_PART="$1"
[ -b "$TARGET_PART" ] || fail "Not a block device: $TARGET_PART"
[ -f "$ARCHIVE" ] || fail "Filesystem archive not found: $ARCHIVE"
command -v mount >/dev/null 2>&1 || fail "mount not found"
command -v umount >/dev/null 2>&1 || fail "umount not found"
command -v tar >/dev/null 2>&1 || fail "tar not found"
command -v "$GRUB" >/dev/null 2>&1 || fail "GRUB Legacy shell not found: $GRUB"

# Resolve the containing disk. Linux names handled: /dev/sdXN, /dev/hdXN,
# /dev/vdXN, /dev/xvdXN and /dev/mmcblkNpN. Extend for other device schemes.
case "$TARGET_PART" in
  /dev/mmcblk[0-9]*p[0-9]*) TARGET_DISK="${TARGET_PART%p[0-9]*}" ;;
  /dev/nvme*n[0-9]p[0-9]*) TARGET_DISK="${TARGET_PART%p[0-9]*}" ;;
  /dev/[shv]d[a-z][0-9]*) TARGET_DISK="${TARGET_PART%[0-9]*}" ;;
  /dev/xvd[a-z][0-9]*) TARGET_DISK="${TARGET_PART%[0-9]*}" ;;
  *) fail "Cannot determine parent disk for $TARGET_PART; edit install.sh for this device naming scheme." ;;
esac
[ -b "$TARGET_DISK" ] || fail "Parent disk is not a block device: $TARGET_DISK"

# Do not mount an already mounted target; this avoids overwriting a live system.
if grep -Fq " $TARGET_PART " /proc/mounts; then fail "$TARGET_PART is already mounted."; fi

MOUNT_POINT="/mnt/lvgd486-target"
mkdir -p "$MOUNT_POINT"
if grep -Fq " $MOUNT_POINT " /proc/mounts; then fail "$MOUNT_POINT is already mounted."; fi
MOUNTED=0
cleanup() { if [ "$MOUNTED" -eq 1 ]; then umount "$MOUNT_POINT" || echo "WARNING: Could not unmount $MOUNT_POINT" >&2; fi; }
trap cleanup EXIT HUP INT TERM

echo "Target partition: $TARGET_PART"
echo "GRUB destination disk MBR: $TARGET_DISK"
echo "WARNING: GRUB will modify the MBR of $TARGET_DISK."
echo "The target partition's existing contents will be replaced."
if [ "${LVGD486_CONFIRM:-}" != YES ]; then
  printf 'Type INSTALL to continue: '
  read answer
  [ "$answer" = INSTALL ] || fail "Installation cancelled."
fi

mount -t ext2 "$TARGET_PART" "$MOUNT_POINT"
MOUNTED=1
# Ensure target is an ext2 filesystem and the mount is writable.
[ -w "$MOUNT_POINT" ] || fail "Target filesystem is not writable."
echo "Extracting filesystem archive..."
tar --numeric-owner -xzpf "$ARCHIVE" -C "$MOUNT_POINT"
[ -f "$MOUNT_POINT/boot/menu.lst" ] || fail "Installed /boot/menu.lst is missing."
[ -d "$MOUNT_POINT/boot/grub" ] || mkdir -p "$MOUNT_POINT/boot/grub"
# The distro archive need not contain GRUB stage files; seed them from the installer media.
if [ ! -f "$MOUNT_POINT/boot/grub/stage1" ] || [ ! -f "$MOUNT_POINT/boot/grub/stage2" ]; then
  [ -f /install-media/boot/grub/stage1 ] && [ -f /install-media/boot/grub/stage2 ] || fail "GRUB stage files are missing from both target and installer media."
  cp /install-media/boot/grub/stage1 /install-media/boot/grub/stage2 "$MOUNT_POINT/boot/grub/"
fi

# Create a temporary device map so GRUB Legacy knows the actual disk and partition.
MAP="/tmp/lvgd486-device-map.$$"
trap 'rm -f "$MAP"; cleanup' EXIT HUP INT TERM
printf '(hd0) %s\n' "$TARGET_DISK" > "$MAP"
# The GRUB shell uses the map above; root selects the target partition and setup
# installs GRUB to the disk MBR. The partition index is determined from Linux naming.
case "$TARGET_PART" in
  /dev/mmcblk[0-9]*p[0-9]*) PART_NUM="${TARGET_PART##*p}" ;;
  /dev/nvme*n[0-9]p[0-9]*) PART_NUM="${TARGET_PART##*p}" ;;
  *) PART_NUM="${TARGET_PART##*[!0-9]}" ;;
esac
[ -n "$PART_NUM" ] || fail "Cannot determine target partition number."
GRUB_PART=$((PART_NUM - 1))
[ "$GRUB_PART" -ge 0 ] || fail "Invalid target partition number."

# Run from the installed root so GRUB can find stage files.
# --batch and the explicit map avoid relying on the installer's host device map.
"$GRUB" --batch --device-map="$MAP" <<GRUBEOF
root (hd0,$GRUB_PART)
setup (hd0)
quit
GRUBEOF
sync
umount "$MOUNT_POINT"
MOUNTED=0
rm -f "$MAP"
trap - EXIT HUP INT TERM
echo "Installation complete. Remove the media and reboot."
echo "GRUB was installed to $TARGET_DISK; system files are on $TARGET_PART."
