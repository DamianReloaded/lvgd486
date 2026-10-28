#!/bin/bash
# LVGD486 - Build bootable USB disk image and bootable GRUB Legacy CD ISO.
set -euo pipefail

# Adjust these paths to match the current distro build.
NAME="${NAME:-lvgd486-installer}"
IMAGE_DIR="${IMAGE_DIR:-/home/user/images}"
DISTRO_ROOT="${DISTRO_ROOT:-/home/user/distrodev}"
ARCHIVE="${ARCHIVE:-$IMAGE_DIR/distrodev.tar.gz}"
MOUNT_POINT="${MOUNT_POINT:-/mnt/installdev}"
USB_IMAGE="${USB_IMAGE:-$IMAGE_DIR/${NAME}-usb.img}"
ISO_IMAGE="${ISO_IMAGE:-$IMAGE_DIR/${NAME}.iso}"
IMAGE_SIZE_MB="${IMAGE_SIZE_MB:-1800}"
KERNEL="${KERNEL:-}"
BUSYBOX="${BUSYBOX:-/home/user/sources/build/busybox/busybox}"
GRUB_DIR="${GRUB_DIR:-/usr/lib/grub/i386-pc}"
GRUB_SHELL="${GRUB_SHELL:-grub}"
E2FSCK="${E2FSCK:-$(command -v e2fsck || true)}"
MKE2FS="${MKE2FS:-$(command -v mke2fs || true)}"
MKISOFS="${MKISOFS:-$(command -v genisoimage || command -v mkisofs || true)}"
LOOP_DEVICE="${LOOP_DEVICE:-/dev/loop1}"

fail() { echo "ERROR: $*" >&2; exit 1; }
[ "$(id -u)" -eq 0 ] || fail "Run as root."
[ -d "$IMAGE_DIR" ] || fail "Image directory does not exist: $IMAGE_DIR"
[ -d "$DISTRO_ROOT" ] || fail "Distro root not found: $DISTRO_ROOT"
[ -f "$ARCHIVE" ] || fail "Archive not found: $ARCHIVE. Run create-distrodev-archive.sh first."
[ -x "$BUSYBOX" ] || fail "BusyBox executable not found: $BUSYBOX (set BUSYBOX=...)."
command -v cpio >/dev/null 2>&1 || fail "cpio is required to build the initramfs."
command -v mke2fs >/dev/null 2>&1 || fail "mke2fs is required to build the USB filesystem."
command -v losetup >/dev/null 2>&1 || fail "losetup not found."
command -v mount >/dev/null 2>&1 || fail "mount not found."
command -v umount >/dev/null 2>&1 || fail "umount not found."
command -v python3 >/dev/null 2>&1 || fail "python3 is required to write the MBR partition entry."
[ -n "$MKISOFS" ] && [ -x "$MKISOFS" ] || fail "genisoimage or mkisofs not found."
command -v "$GRUB_SHELL" >/dev/null 2>&1 || fail "GRUB Legacy shell not found: $GRUB_SHELL"
for f in stage1 stage2 stage2_eltorito; do [ -f "$GRUB_DIR/$f" ] || fail "Missing $GRUB_DIR/$f; set GRUB_DIR to the GRUB Legacy i386 files directory."; done

if [ -z "$KERNEL" ]; then
  for candidate in "$DISTRO_ROOT"/boot/vmlinuz* "$DISTRO_ROOT"/boot/bzImage* "$DISTRO_ROOT"/boot/kernel*; do
    if [ -f "$candidate" ]; then KERNEL="$candidate"; break; fi
  done
fi
[ -n "$KERNEL" ] && [ -f "$KERNEL" ] || fail "Set KERNEL to the distrodev-compatible kernel file."
[ ! -e "$USB_IMAGE" ] || fail "USB image already exists: $USB_IMAGE"
[ ! -e "$ISO_IMAGE" ] || fail "ISO image already exists: $ISO_IMAGE"
if mount | grep -Fq " on $MOUNT_POINT "; then fail "$MOUNT_POINT is already mounted."; fi
if "$GRUB_SHELL" --version 2>&1 | grep -q '0\.9'; then :; else echo "WARNING: Could not confirm GRUB Legacy 0.9x from version output." >&2; fi

WORK="$(mktemp -d /tmp/lvgd486-media.XXXXXX)"
LOOP_ATTACHED=0; MOUNTED=0
cleanup() { status=$?; trap - EXIT; if [ "$MOUNTED" -eq 1 ]; then umount "$MOUNT_POINT" || true; fi; if [ "$LOOP_ATTACHED" -eq 1 ]; then losetup -d "$LOOP_DEVICE" || true; fi; rm -rf "$WORK"; exit "$status"; }
trap cleanup EXIT
mkdir -p "$WORK/iso/boot/grub" "$WORK/initramfs/bin" "$WORK/initramfs/sbin" "$WORK/initramfs/dev" "$WORK/initramfs/proc" "$WORK/initramfs/sys" "$WORK/initramfs/mnt" "$WORK/initramfs/install-media" "$MOUNT_POINT"
cp "$KERNEL" "$WORK/iso/boot/vmlinuz"
cp "$GRUB_DIR/stage2_eltorito" "$WORK/iso/boot/grub/stage2_eltorito"
cp "$GRUB_DIR/stage1" "$GRUB_DIR/stage2" "$WORK/iso/boot/grub/"
cp "$ARCHIVE" "$WORK/iso/distrodev.tar.gz"
cp "$(dirname "$0")/install.sh" "$WORK/initramfs/sbin/install.sh"
chmod 755 "$WORK/initramfs/sbin/install.sh"
cp "$BUSYBOX" "$WORK/initramfs/bin/busybox"
chmod 755 "$WORK/initramfs/bin/busybox"
# Include the GRUB Legacy shell and ext2 utilities in the installer environment.
mkdir -p "$WORK/initramfs/usr/sbin" "$WORK/initramfs/usr/bin" "$WORK/initramfs/usr/lib"
cp "$(command -v "$GRUB_SHELL")" "$WORK/initramfs/bin/grub"
[ -n "$E2FSCK" ] && [ -x "$E2FSCK" ] || fail "e2fsck not found; set E2FSCK=..."
[ -n "$MKE2FS" ] && [ -x "$MKE2FS" ] || fail "mke2fs not found; set MKE2FS=..."
cp "$E2FSCK" "$WORK/initramfs/sbin/e2fsck"
cp "$MKE2FS" "$WORK/initramfs/sbin/mke2fs"
# Copy shared-library dependencies for dynamically linked tools. Static BusyBox is recommended.
copy_libs() {
  local binary="$1" lib dest
  command -v ldd >/dev/null 2>&1 || return 0
  ldd "$binary" 2>/dev/null | awk '{ for (i=1;i<=NF;i++) if ($i ~ /^\//) print $i }' | while read -r lib; do
    [ -f "$lib" ] || continue
    dest="$WORK/initramfs$lib"
    mkdir -p "$(dirname "$dest")"
    [ -e "$dest" ] || cp -L "$lib" "$dest"
  done
}
copy_libs "$WORK/initramfs/bin/grub"
copy_libs "$WORK/initramfs/bin/busybox"
copy_libs "$WORK/initramfs/sbin/e2fsck"
copy_libs "$WORK/initramfs/sbin/mke2fs"

# BusyBox applet links provide a minimal shell and disk/mount utilities.
for applet in sh mount umount mkdir mknod sleep cat echo ls dmesg grep awk sed fdisk blkid mdev sync tar gzip clear; do
  ln -sf busybox "$WORK/initramfs/bin/$applet" 2>/dev/null || true
done
# initramfs init mounts the kernel pseudo-filesystems and locates installation media.
cat > "$WORK/initramfs/init" <<'INITEOF'
#!/bin/sh
export PATH=/bin:/sbin:/usr/bin:/usr/sbin
mount -t proc proc /proc 2>/dev/null || true
mount -t sysfs sysfs /sys 2>/dev/null || true
mount -t devtmpfs devtmpfs /dev 2>/dev/null || { [ -x /bin/busybox ] && /bin/busybox mdev -s 2>/dev/null || true; }
mkdir -p /install-media /mnt/cdrom /mnt/usb
# Wait briefly for USB/SCSI and optical devices to appear.
sleep 2
for dev in /dev/sr0 /dev/cdrom /dev/sda1 /dev/sdb1 /dev/sdc1 /dev/sda /dev/sdb; do
  [ -b "$dev" ] || continue
  if mount -o ro "$dev" /install-media 2>/dev/null; then
    if [ -f /install-media/distrodev.tar.gz ]; then break; fi
    umount /install-media 2>/dev/null || true
  fi
done
clear 2>/dev/null || true
echo 'LVGD486 installer environment'
echo 'Tools: BusyBox, mke2fs, e2fsck and GRUB Legacy shell (grub).'
echo 'Use fdisk (if included in BusyBox) to partition; use mke2fs -t ext2 /dev/DEVICE to format.'
echo 'The installation archive must be mounted at /install-media/distrodev.tar.gz.'
echo 'Example: /sbin/install.sh /dev/sda1'
exec /bin/sh
INITEOF
chmod 755 "$WORK/initramfs/init"
( cd "$WORK/initramfs" && find . -print0 | cpio --null -o -H newc 2>/dev/null | gzip -9 ) > "$WORK/iso/boot/initrd.gz"

# GRUB Legacy CD menu. stage2_eltorito is the El Torito boot image.
cat > "$WORK/iso/boot/grub/menu.lst" <<'GRUBEOF'
default 0
timeout 5

 title LVGD486 Installer
   kernel /boot/vmlinuz rw root=/dev/ram0 init=/init
   initrd /boot/initrd.gz
GRUBEOF

# Create a raw USB image with one MBR type-0x83 partition starting at LBA 63.
# Use a partitioned image and offset loop device, preserving the project's legacy layout.
SECTOR_SIZE=512; START=63; BLOCK_SIZE=1024
IMAGE_BYTES=$((IMAGE_SIZE_MB * 1024 * 1024)); TOTAL_SECTORS=$((IMAGE_BYTES / SECTOR_SIZE))
PART_SECTORS=$(((TOTAL_SECTORS - START) / 2 * 2)); PART_BLOCKS=$((PART_SECTORS * SECTOR_SIZE / BLOCK_SIZE)); OFFSET=$((START * SECTOR_SIZE))
[ "$PART_SECTORS" -gt 0 ] || fail "Image size too small."
dd if=/dev/zero of="$USB_IMAGE" bs=1024 count=$((IMAGE_BYTES / 1024)) status=none
# Partition entry: active Linux partition, LBA 63, remaining image size.
python3 - "$USB_IMAGE" "$START" "$PART_SECTORS" <<'PY'
import struct,sys
path,start,size=sys.argv[1],int(sys.argv[2]),int(sys.argv[3])
with open(path,'r+b') as f:
    f.seek(446); f.write(struct.pack('<B3sB3sII',0x80,b'\xfe\xff\xff',0x83,b'\xfe\xff\xff',start,size))
    f.seek(510); f.write(b'\x55\xaa')
PY
PARTFILE="$WORK/partition.img"
dd if=/dev/zero of="$PARTFILE" bs=1024 count="$PART_BLOCKS" status=none
mke2fs -F -r 0 -b "$BLOCK_SIZE" "$PARTFILE" "$PART_BLOCKS" >/dev/null
# Mount partition file through a loop device; copy media payload and archive.
losetup -o "$OFFSET" "$LOOP_DEVICE" "$USB_IMAGE"; LOOP_ATTACHED=1
# Write the ext2 partition payload to the image; this avoids assuming losetup can bind an offset before filesystem is copied.
dd if="$PARTFILE" of="$USB_IMAGE" bs=512 seek="$START" conv=notrunc status=none
losetup -d "$LOOP_DEVICE"; LOOP_ATTACHED=0
losetup -o "$OFFSET" "$LOOP_DEVICE" "$USB_IMAGE"; LOOP_ATTACHED=1
mount -t ext2 "$LOOP_DEVICE" "$MOUNT_POINT"; MOUNTED=1
mkdir -p "$MOUNT_POINT/boot/grub" "$MOUNT_POINT/boot"
cp "$WORK/iso/boot/vmlinuz" "$MOUNT_POINT/boot/vmlinuz"
cp "$WORK/iso/boot/grub/stage1" "$WORK/iso/boot/grub/stage2" "$MOUNT_POINT/boot/grub/"
cp "$WORK/iso/boot/grub/menu.lst" "$MOUNT_POINT/boot/grub/menu.lst"
cp "$ARCHIVE" "$MOUNT_POINT/distrodev.tar.gz"
# USB menu starts the installer initramfs, embedded as boot/initrd.gz.
cp "$WORK/iso/boot/initrd.gz" "$MOUNT_POINT/boot/initrd.gz"
cat > "$MOUNT_POINT/boot/grub/menu.lst" <<'GRUBEOF'
default 0
timeout 5

title LVGD486 Installer
  root (hd0,0)
  kernel /boot/vmlinuz rw root=/dev/ram0 init=/init
  initrd /boot/initrd.gz
GRUBEOF
sync
umount "$MOUNT_POINT"; MOUNTED=0
losetup -d "$LOOP_DEVICE"; LOOP_ATTACHED=0
# Install GRUB Legacy into the USB image MBR using a temporary map.
MAP="$WORK/device.map"
printf '(hd0) %s\n' "$USB_IMAGE" > "$MAP"
"$GRUB_SHELL" --batch --device-map="$MAP" <<GRUBEOF
root (hd0,0)
setup (hd0)
quit
GRUBEOF

# CD ISO: boot via GRUB Legacy's stage2_eltorito; the installer archive is at ISO root.
"$MKISOFS" -R -J -V LVGD486_INSTALLER -b boot/grub/stage2_eltorito -c boot/grub/boot.cat -no-emul-boot -boot-load-size 4 -boot-info-table -o "$ISO_IMAGE" "$WORK/iso"

echo
echo "LVGD486 installer media created."
echo "USB image: $USB_IMAGE"
echo "CD ISO:    $ISO_IMAGE"
echo "Archive:   $ARCHIVE"
echo "Kernel:    $KERNEL"
echo "NOTE: Verify your BusyBox build includes the applets needed for partitioning and ext2 formatting."
