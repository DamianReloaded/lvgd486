#!/bin/bash

# ============================================================
# LVGD486 - Build Bootable USB Installer
# Debian Sarge host / Linux 2.4 target
# Uses a compressed ext2 initrd for early boot.
# Copies distro utilities, BusyBox and static e2fsprogs tools.
# ============================================================

set -eu

# ------------------------------------------------------------
# Paths
# ------------------------------------------------------------

IMAGE_DIR="/home/user/images"
DISTRO_ROOT="/home/user/distrodev"

ARCHIVE="${IMAGE_DIR}/distrodev.tar.gz"
INSTALL_SCRIPT="/home/user/scripts/installer/install.sh"

BUSYBOX="${DISTRO_ROOT}/bin/busybox"
DISTRO_BIN="${DISTRO_ROOT}/bin"
DISTRO_SBIN="${DISTRO_ROOT}/sbin"

KERNEL="${DISTRO_ROOT}/boot/bzImage"
KERNEL_CONFIG="/home/user/sources/linux/.config"

GRUB_SHELL="/home/user/sources/build/grub/grub/grub"
GRUB_DIR="/home/user/doc/grub-stage-files"

# Built e2fsprogs 1.37 utilities.
# These are the static i386 binaries produced by our build script.
E2FSPROGS_BUILD="/home/user/sources/build/e2fsprogs"

E2FS_MKE2FS="${E2FSPROGS_BUILD}/misc/mke2fs"
E2FS_E2FSCK="${E2FSPROGS_BUILD}/e2fsck/e2fsck"
E2FS_TUNE2FS="${E2FSPROGS_BUILD}/misc/tune2fs"
E2FS_DUMPE2FS="${E2FSPROGS_BUILD}/misc/dumpe2fs"
E2FS_RESIZE2FS="${E2FSPROGS_BUILD}/resize/resize2fs"

USB_IMAGE="${IMAGE_DIR}/lvgd486-installer-usb.img"
MOUNT_POINT="/mnt/lvgd486-installer"
LOOP_DEVICE="/dev/loop1"

# ------------------------------------------------------------
# Disk and initrd settings
# ------------------------------------------------------------

IMAGE_SIZE_MB=650
SECTOR_SIZE=512
PARTITION_START=63
BLOCK_SIZE=1024
HEADS=16
SECTORS_PER_TRACK=63

INITRD_SIZE_MB=32
INITRD_RAM_SIZE_KB=65536

# ------------------------------------------------------------
# Host utilities
# ------------------------------------------------------------

MKE2FS="$(command -v mke2fs || true)"
E2FSCK="$(command -v e2fsck || true)"
LOSSETUP="$(command -v losetup || true)"
PERL="$(command -v perl || true)"
FILE="$(command -v file || true)"
LDD="$(command -v ldd || true)"

WORK=""
MOUNTED=0
LOOP_ATTACHED=0
SUCCESS=0

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
    if [ "$MOUNTED" -eq 1 ]; then
        umount "$MOUNT_POINT" >/dev/null 2>&1 || true
        MOUNTED=0
    fi

    if [ "$LOOP_ATTACHED" -eq 1 ]; then
        "$LOSSETUP" -d "$LOOP_DEVICE" >/dev/null 2>&1 || true
        LOOP_ATTACHED=0
    fi

    if [ "$SUCCESS" -ne 1 ]; then
        rm -f "$USB_IMAGE"
    fi

    if [ -n "$WORK" ] && [ -d "$WORK" ]; then
        rm -rf "$WORK"
    fi
}

trap cleanup EXIT
trap 'exit 1' HUP INT TERM

# ------------------------------------------------------------
# Preflight checks
# ------------------------------------------------------------

[ "$(id -u)" -eq 0 ] ||
    fail "Run this script as root."

[ -x "$BUSYBOX" ] ||
    fail "BusyBox not found or not executable: $BUSYBOX"

[ -d "$DISTRO_BIN" ] ||
    fail "Distro bin directory not found: $DISTRO_BIN"

[ -d "$DISTRO_SBIN" ] ||
    fail "Distro sbin directory not found: $DISTRO_SBIN"

[ -f "$KERNEL" ] ||
    fail "Kernel not found: $KERNEL"

[ -f "$KERNEL_CONFIG" ] ||
    fail "Kernel configuration not found: $KERNEL_CONFIG"

[ -f "$ARCHIVE" ] ||
    fail "Installer archive not found: $ARCHIVE"

[ -f "$INSTALL_SCRIPT" ] ||
    fail "Installer script not found: $INSTALL_SCRIPT"

[ -x "$GRUB_SHELL" ] ||
    fail "Built GRUB executable not found: $GRUB_SHELL"

[ -d "$GRUB_DIR" ] ||
    fail "GRUB stage directory not found: $GRUB_DIR"

[ -n "$MKE2FS" ] ||
    fail "Host mke2fs is required to create the installer filesystems."

[ -n "$E2FSCK" ] ||
    fail "Host e2fsck is required to check the installer filesystems."

[ -n "$LOSSETUP" ] ||
    fail "losetup is required."

[ -n "$PERL" ] ||
    fail "Perl is required."

[ -n "$FILE" ] ||
    fail "file is required to validate executable architectures."

for utility in dd cp mkdir chmod ln mknod mount umount \
               tar gzip grep awk sort cmp ls cat rm \
               sync mktemp dirname id
do
    command -v "$utility" >/dev/null 2>&1 ||
        fail "Required utility not found: $utility"
done

# ------------------------------------------------------------
# Validate built e2fsprogs utilities
# ------------------------------------------------------------

echo "Checking statically linked e2fsprogs utilities..."

for binary in \
    "$E2FS_MKE2FS" \
    "$E2FS_E2FSCK" \
    "$E2FS_TUNE2FS" \
    "$E2FS_DUMPE2FS" \
    "$E2FS_RESIZE2FS"
do
    [ -f "$binary" ] ||
        fail "Required e2fsprogs binary not found: $binary"

    [ -x "$binary" ] ||
        fail "Required e2fsprogs binary is not executable: $binary"

    binary_info="$("$FILE" "$binary")"
    echo "$binary_info"

    case "$binary_info" in
        *"ELF 32-bit"*80386*) ;;
        *"ELF 32-bit"*i386*) ;;
        *)
            fail "e2fsprogs binary is not a 32-bit x86 ELF executable: $binary"
            ;;
    esac

    case "$binary_info" in
        *"statically linked"*) ;;
        *)
            fail "e2fsprogs binary is not statically linked: $binary"
            ;;
    esac

    case "$binary_info" in
        *"dynamically linked"*)
            fail "e2fsprogs binary has dynamic linkage: $binary"
            ;;
    esac
done

echo "All required e2fsprogs binaries are static 32-bit x86 executables."

# ------------------------------------------------------------
# Validate GRUB stage files and output path
# ------------------------------------------------------------

for stage in stage1 stage2
do
    [ -s "${GRUB_DIR}/${stage}" ] ||
        fail "GRUB stage file missing: ${GRUB_DIR}/${stage}"
done

[ ! -e "$USB_IMAGE" ] ||
    fail "Output already exists: $USB_IMAGE"

# ------------------------------------------------------------
# Validate required built-in kernel options
# ------------------------------------------------------------

echo "Checking kernel configuration..."

for option in \
    CONFIG_BLK_DEV_INITRD \
    CONFIG_BLK_DEV_RAM \
    CONFIG_EXT2_FS \
    CONFIG_USB \
    CONFIG_USB_STORAGE \
    CONFIG_SCSI \
    CONFIG_BLK_DEV_SD
do
    if ! grep -qx "${option}=y" "$KERNEL_CONFIG"; then
        fail "${option} must be built into the kernel (=y). Reconfigure and rebuild the kernel."
    fi
done

echo "Required initrd, RAM disk, ext2 and USB-storage options are built in."
echo "Ensure the USB host-controller driver for the target hardware is also built in."

# ------------------------------------------------------------
# Validate GRUB architecture and installer archive
# ------------------------------------------------------------

GRUB_INFO="$("$FILE" "$GRUB_SHELL")"
echo "$GRUB_INFO"

case "$GRUB_INFO" in
    *"ELF 32-bit"*80386*) ;;
    *"ELF 32-bit"*i386*) ;;
    *)
        fail "GRUB must be a 32-bit x86 executable."
        ;;
esac

tar -tzf "$ARCHIVE" >/dev/null ||
    fail "Installer archive validation failed."

WORK="$(mktemp -d /tmp/lvgd486-usb.XXXXXX)"

PARTITION_FILE="${WORK}/partition.img"
INITRD_RAW="${WORK}/installer-initrd.img"
INITRD_GZ="${WORK}/installer-initrd.gz"

DEVICE_MAP="${WORK}/device.map"
MBR_BACKUP="${WORK}/mbr.backup"
TABLE_BEFORE="${WORK}/table.before"
TABLE_AFTER="${WORK}/table.after"
GRUB_LOG="${WORK}/grub.log"
GRUB_LIBS_FILE="${WORK}/grub-libraries"
LIBRARIES_LIST="${WORK}/libraries.list"

# ------------------------------------------------------------
# Calculate disk geometry
# ------------------------------------------------------------

IMAGE_BYTES=$((IMAGE_SIZE_MB * 1024 * 1024))
TOTAL_SECTORS=$((IMAGE_BYTES / SECTOR_SIZE))

CYLINDER_SIZE=$((HEADS * SECTORS_PER_TRACK))
CYLINDERS=$(((TOTAL_SECTORS + CYLINDER_SIZE - 1) / CYLINDER_SIZE))
GEOMETRY_SECTORS=$((CYLINDERS * CYLINDER_SIZE))

PARTITION_SECTORS=$((TOTAL_SECTORS - PARTITION_START))
PARTITION_SECTORS=$((PARTITION_SECTORS / 2 * 2))

PARTITION_END=$((PARTITION_START + PARTITION_SECTORS - 1))
PARTITION_BYTES=$((PARTITION_SECTORS * SECTOR_SIZE))
PARTITION_BLOCKS=$((PARTITION_BYTES / BLOCK_SIZE))
PARTITION_OFFSET=$((PARTITION_START * SECTOR_SIZE))

[ "$PARTITION_SECTORS" -gt 0 ] ||
    fail "Invalid partition size."

[ "$PARTITION_BLOCKS" -gt 0 ] ||
    fail "Invalid filesystem size."

echo
echo "LVGD486 USB Installer"
echo "--------------------"
echo "Image:             $USB_IMAGE"
echo "Image size:        ${IMAGE_SIZE_MB} MiB"
echo "Partition start:   $PARTITION_START"
echo "Partition end:     $PARTITION_END"
echo "Partition blocks:  $PARTITION_BLOCKS"
echo "Filesystem:        ext2 revision 0"
echo "Filesystem block:  $BLOCK_SIZE bytes"
echo "Initrd raw size:   ${INITRD_SIZE_MB} MiB"
echo "Initrd RAM size:   ${INITRD_RAM_SIZE_KB} KiB"
echo "e2fsprogs build:   $E2FSPROGS_BUILD"
echo

mkdir -p "$IMAGE_DIR"
mkdir -p "$MOUNT_POINT"

if mount | grep -F " on ${MOUNT_POINT} " >/dev/null 2>&1; then
    fail "Mount point is already mounted: $MOUNT_POINT"
fi

if [ -n "$(ls -A "$MOUNT_POINT")" ]; then
    fail "Mount point is not empty: $MOUNT_POINT"
fi

if "$LOSSETUP" "$LOOP_DEVICE" >/dev/null 2>&1; then
    fail "Loop device is already in use: $LOOP_DEVICE"
fi

# ============================================================
# Build the initrd first
# ============================================================

echo "Creating initrd filesystem..."

dd if=/dev/zero of="$INITRD_RAW" \
    bs=1048576 count="$INITRD_SIZE_MB"

"$MKE2FS" -F -r 0 -b "$BLOCK_SIZE" "$INITRD_RAW"

"$LOSSETUP" "$LOOP_DEVICE" "$INITRD_RAW"
LOOP_ATTACHED=1

mount -t ext2 "$LOOP_DEVICE" "$MOUNT_POINT"
MOUNTED=1

# ------------------------------------------------------------
# Create initrd directory structure
# ------------------------------------------------------------

echo "Creating initrd directory structure..."

mkdir -p "$MOUNT_POINT/bin"
mkdir -p "$MOUNT_POINT/boot/grub"
mkdir -p "$MOUNT_POINT/dev"
mkdir -p "$MOUNT_POINT/install-media"
mkdir -p "$MOUNT_POINT/lib"
mkdir -p "$MOUNT_POINT/proc"
mkdir -p "$MOUNT_POINT/sbin"
mkdir -p "$MOUNT_POINT/tmp"
mkdir -p "$MOUNT_POINT/usr/lib"

# ------------------------------------------------------------
# Copy distro utilities into the initrd
# ------------------------------------------------------------

echo "Copying distro /bin and /sbin into initrd..."

cp -a "$DISTRO_BIN/." "$MOUNT_POINT/bin/"
cp -a "$DISTRO_SBIN/." "$MOUNT_POINT/sbin/"

[ -x "$MOUNT_POINT/bin/busybox" ] ||
    fail "Copied BusyBox is missing or not executable."

chmod 755 "$MOUNT_POINT/bin/busybox"

# Verify that the applet used by install.sh is present.
if [ ! -x "$MOUNT_POINT/bin/id" ] &&
   [ ! -x "$MOUNT_POINT/sbin/id" ]; then
    echo "WARNING: The distro does not provide an id command."
    echo "The installer root check must be changed or the id applet added."
fi

# ------------------------------------------------------------
# Copy GRUB shell and runtime libraries into initrd
# ------------------------------------------------------------

echo "Copying GRUB executable into initrd..."

cp -p "$GRUB_SHELL" "$MOUNT_POINT/bin/grub"
chmod 755 "$MOUNT_POINT/bin/grub"

if [ -n "$LDD" ]; then
    "$LDD" "$GRUB_SHELL" > "$GRUB_LIBS_FILE" 2>&1 || true

    if grep "not found" "$GRUB_LIBS_FILE" >/dev/null 2>&1; then
        cat "$GRUB_LIBS_FILE" >&2
        fail "GRUB has unresolved runtime libraries."
    fi

    awk '
        /=> \// { print $3 }
        /^\//   { print $1 }
    ' "$GRUB_LIBS_FILE" | sort -u > "$LIBRARIES_LIST"

    while IFS= read -r library
    do
        [ -f "$library" ] || continue

        destination="${MOUNT_POINT}${library}"
        mkdir -p "$(dirname "$destination")"
        cp -L "$library" "$destination"
    done < "$LIBRARIES_LIST"
fi

# ------------------------------------------------------------
# Copy GRUB stage files into initrd
# ------------------------------------------------------------

cp -a "$GRUB_DIR/." "$MOUNT_POINT/boot/grub/"

# ------------------------------------------------------------
# Create static device nodes in initrd
# ------------------------------------------------------------

echo "Creating initrd device nodes..."

mknod -m 600 "$MOUNT_POINT/dev/console" c 5 1
mknod -m 666 "$MOUNT_POINT/dev/null" c 1 3
mknod -m 666 "$MOUNT_POINT/dev/zero" c 1 5
mknod -m 666 "$MOUNT_POINT/dev/tty" c 5 0
mknod -m 600 "$MOUNT_POINT/dev/tty0" c 4 0
mknod -m 600 "$MOUNT_POINT/dev/tty1" c 4 1
mknod -m 666 "$MOUNT_POINT/dev/ptmx" c 5 2

# RAM disk root device.
mknod -m 600 "$MOUNT_POINT/dev/ram0" b 1 0

# IDE devices.
# Linux IDE major/minor mappings:
# hda/hdb = major 3, hdc/hdd = major 22,
# hde/hdf = major 33, hdg/hdh = major 34.
for disk in a b c d e f g h
do
    case "$disk" in
        a) ideMajor=3;  ideBase=0 ;;
        b) ideMajor=3;  ideBase=64 ;;
        c) ideMajor=22; ideBase=0 ;;
        d) ideMajor=22; ideBase=64 ;;
        e) ideMajor=33; ideBase=0 ;;
        f) ideMajor=33; ideBase=64 ;;
        g) ideMajor=34; ideBase=0 ;;
        h) ideMajor=34; ideBase=64 ;;
    esac

    mknod -m 660 "$MOUNT_POINT/dev/hd${disk}" \
        b "$ideMajor" "$ideBase"

    for partition in 1 2 3 4 5 6 7 8
    do
        mknod -m 660 "$MOUNT_POINT/dev/hd${disk}${partition}" \
            b "$ideMajor" "$((ideBase + partition))"
    done
done

# SCSI and USB storage devices.
# Linux 2.4 uses major 8 with 16 minor numbers per disk.
for diskNumber in 0 1 2 3
do
    diskLetter=$(printf "\\$(printf '%03o' $((97 + diskNumber)))")
    diskMinor=$((diskNumber * 16))

    mknod -m 660 "$MOUNT_POINT/dev/sd${diskLetter}" \
        b 8 "$diskMinor"

    for partition in 1 2 3 4 5 6 7 8
    do
        mknod -m 660 "$MOUNT_POINT/dev/sd${diskLetter}${partition}" \
            b 8 "$((diskMinor + partition))"
    done
done

# ------------------------------------------------------------
# Linux 2.4 initrd startup script
# ------------------------------------------------------------

cat > "$MOUNT_POINT/linuxrc" <<'LINUXRC'
#!/bin/sh

export PATH=/bin:/sbin
export HOME=/

echo
echo "LVGD486: starting RAM-based installer"
echo "Searching for the USB installer partition..."

mount -t proc proc /proc 2>/dev/null || true

attempt=0
media_found=0

while [ "$attempt" -lt 30 ]
do
    for device in /dev/sda1 /dev/sdb1 /dev/sdc1 /dev/sdd1
    do
        if mount -t ext2 "$device" /install-media 2>/dev/null
        then
            if [ -f /install-media/distrodev.tar.gz ] &&
               [ -x /install-media/install.sh ]
            then
                echo "Installer media found on $device"

                ln -sf /install-media/install.sh /install.sh
                ln -sf /install-media/distrodev.tar.gz /distrodev.tar.gz

                media_found=1
                break
            fi

            umount /install-media 2>/dev/null || true
        fi
    done

    [ "$media_found" -eq 1 ] && break

    attempt=$((attempt + 1))
    sleep 1
done

if [ "$media_found" -ne 1 ]
then
    echo
    echo "WARNING: USB installer media was not found."
    echo "Check built-in USB host-controller and USB-storage support."
    echo "A shell will be started for troubleshooting."
fi

exec /sbin/init
LINUXRC

chmod 755 "$MOUNT_POINT/linuxrc"

# ------------------------------------------------------------
# Installer init process
# ------------------------------------------------------------

cat > "$MOUNT_POINT/sbin/init" <<'INIT'
#!/bin/sh

# Put the USB media utilities first so the installer can find
# the static e2fsprogs binaries after the media is mounted.
export PATH=/install-media/sbin:/install-media/bin:/bin:/sbin
export HOME=/
export PS1='LVGD486# '

mount -t proc proc /proc 2>/dev/null || true

echo
echo "LVGD486 Installer"
echo "Linux 2.4 RAM-based installation environment"
echo

if [ -f /install-media/distrodev.tar.gz ]; then
    echo "Installer media: /install-media"
    echo "Installer script: /install.sh"
    echo "Archive: /distrodev.tar.gz"
    echo
    echo "Filesystem utilities available:"
    echo "  mke2fs, e2fsck, tune2fs, dumpe2fs, resize2fs"
    echo
    echo "Use fdisk to prepare the destination disk."
    echo "Then run: /install.sh /dev/sda2"
else
    echo "Installer media is unavailable."
    echo "Check USB detection and kernel driver configuration."
fi

echo

exec /bin/sh
INIT

chmod 755 "$MOUNT_POINT/sbin/init"

# ------------------------------------------------------------
# Verify initrd files
# ------------------------------------------------------------

[ -s "$MOUNT_POINT/bin/busybox" ] ||
    fail "BusyBox missing from initrd."

[ -s "$MOUNT_POINT/bin/grub" ] ||
    fail "GRUB executable missing from initrd."

[ -x "$MOUNT_POINT/bin/mount" ] ||
    [ -x "$MOUNT_POINT/sbin/mount" ] ||
    fail "mount command missing from initrd."

[ -s "$MOUNT_POINT/linuxrc" ] ||
    fail "linuxrc missing from initrd."

[ -s "$MOUNT_POINT/sbin/init" ] ||
    fail "init missing from initrd."

[ -s "$MOUNT_POINT/boot/grub/stage1" ] ||
    fail "GRUB stage1 missing from initrd."

[ -s "$MOUNT_POINT/boot/grub/stage2" ] ||
    fail "GRUB stage2 missing from initrd."

sync

umount "$MOUNT_POINT"
MOUNTED=0

"$E2FSCK" -fn "$LOOP_DEVICE"

"$LOSSETUP" -d "$LOOP_DEVICE"
LOOP_ATTACHED=0

echo "Compressing initrd..."

gzip -9 -c "$INITRD_RAW" > "$INITRD_GZ"
gzip -t "$INITRD_GZ"

# ============================================================
# Create the USB disk image
# ============================================================

echo "Creating USB disk image..."

dd if=/dev/zero of="$USB_IMAGE" \
    bs=1048576 count="$IMAGE_SIZE_MB"

# ------------------------------------------------------------
# Write MBR partition table
# ------------------------------------------------------------

echo "Writing MBR partition table..."

"$PERL" - "$USB_IMAGE" "$PARTITION_START" \
    "$PARTITION_SECTORS" "$HEADS" "$SECTORS_PER_TRACK" <<'PERL'
use strict;
use warnings;

my ($image, $start, $count, $heads, $spt) = @ARGV;

sub chs
{
    my ($lba, $heads, $spt) = @_;

    my $cylinder = int($lba / ($heads * $spt));
    my $head = int(($lba / $spt) % $heads);
    my $sector = ($lba % $spt) + 1;

    if ($cylinder > 1023)
    {
        $cylinder = 1023;
        $head = 254;
        $sector = 63;
    }

    return (
        $head,
        ($sector & 0x3f) | (($cylinder >> 2) & 0xc0),
        $cylinder & 0xff
    );
}

my @start_chs = chs($start, $heads, $spt);
my @end_chs = chs($start + $count - 1, $heads, $spt);

open(my $fh, "+<", $image) or die "Cannot open image: $!";
binmode($fh);

seek($fh, 446, 0) or die "Cannot seek to partition table";

my $entry = pack(
    "C C C C C C C C V V",
    0x80,
    @start_chs,
    0x83,
    @end_chs,
    $start,
    $count
);

print {$fh} $entry or die "Cannot write partition entry";

seek($fh, 510, 0) or die "Cannot seek to MBR signature";
print {$fh} "\x55\xaa" or die "Cannot write MBR signature";

close($fh) or die "Cannot close image";
PERL

# Verify partition entry and MBR signature.
"$PERL" - "$USB_IMAGE" "$PARTITION_START" \
    "$PARTITION_SECTORS" <<'PERL'
use strict;
use warnings;

my ($image, $expected_start, $expected_count) = @ARGV;

open(my $fh, "<", $image) or die "Cannot open image";
binmode($fh);

seek($fh, 446, 0) or die "Cannot seek to partition entry";
read($fh, my $entry, 16) == 16 or die "Cannot read partition entry";

my ($boot, $start_chs, $type, $end_chs, $start, $count) =
    unpack("C a3 C a3 V V", $entry);

seek($fh, 510, 0) or die "Cannot seek to signature";
read($fh, my $signature, 2) == 2 or die "Cannot read signature";

die "Partition is not active\n" unless $boot == 0x80;
die "Incorrect partition type\n" unless $type == 0x83;
die "Incorrect partition start\n" unless $start == $expected_start;
die "Incorrect partition size\n" unless $count == $expected_count;
die "Invalid MBR signature\n" unless $signature eq "\x55\xaa";

close($fh);

print "MBR partition table validated.\n";
PERL

# ------------------------------------------------------------
# Create USB ext2 partition
# ------------------------------------------------------------

echo "Creating USB ext2 filesystem..."

dd if=/dev/zero of="$PARTITION_FILE" \
    bs="$BLOCK_SIZE" count="$PARTITION_BLOCKS"

"$MKE2FS" -F -r 0 -b "$BLOCK_SIZE" "$PARTITION_FILE"

"$E2FSCK" -fn "$PARTITION_FILE"

dd if="$PARTITION_FILE" of="$USB_IMAGE" \
    bs="$SECTOR_SIZE" seek="$PARTITION_START" conv=notrunc

# ------------------------------------------------------------
# Attach and mount USB partition
# ------------------------------------------------------------

echo "Attaching USB partition..."

"$LOSSETUP" -o "$PARTITION_OFFSET" "$LOOP_DEVICE" "$USB_IMAGE"
LOOP_ATTACHED=1

mount -t ext2 "$LOOP_DEVICE" "$MOUNT_POINT"
MOUNTED=1

# ------------------------------------------------------------
# Populate USB installer media
# ------------------------------------------------------------

echo "Copying installer media files..."

mkdir -p "$MOUNT_POINT/bin"
mkdir -p "$MOUNT_POINT/sbin"
mkdir -p "$MOUNT_POINT/boot/grub"

# BusyBox remains available on the USB media.
cp -p "$BUSYBOX" "$MOUNT_POINT/bin/busybox"
chmod 755 "$MOUNT_POINT/bin/busybox"

# ------------------------------------------------------------
# Copy static e2fsprogs binaries to USB media
# ------------------------------------------------------------

echo "Copying static e2fsprogs utilities to USB media..."

cp -p "$E2FS_MKE2FS" "$MOUNT_POINT/sbin/mke2fs"
cp -p "$E2FS_E2FSCK" "$MOUNT_POINT/sbin/e2fsck"
cp -p "$E2FS_TUNE2FS" "$MOUNT_POINT/sbin/tune2fs"
cp -p "$E2FS_DUMPE2FS" "$MOUNT_POINT/sbin/dumpe2fs"
cp -p "$E2FS_RESIZE2FS" "$MOUNT_POINT/sbin/resize2fs"

chmod 755 \
    "$MOUNT_POINT/sbin/mke2fs" \
    "$MOUNT_POINT/sbin/e2fsck" \
    "$MOUNT_POINT/sbin/tune2fs" \
    "$MOUNT_POINT/sbin/dumpe2fs" \
    "$MOUNT_POINT/sbin/resize2fs"

# ------------------------------------------------------------
# Copy kernel, initrd, archive and installer
# ------------------------------------------------------------

cp -p "$KERNEL" "$MOUNT_POINT/boot/bzImage"
cp -p "$INITRD_GZ" "$MOUNT_POINT/boot/installer-initrd.gz"
cp -p "$ARCHIVE" "$MOUNT_POINT/distrodev.tar.gz"
cp -p "$INSTALL_SCRIPT" "$MOUNT_POINT/install.sh"
chmod 755 "$MOUNT_POINT/install.sh"

cp -a "$GRUB_DIR/." "$MOUNT_POINT/boot/grub/"

# ------------------------------------------------------------
# GRUB Legacy configuration
# ------------------------------------------------------------

cat > "$MOUNT_POINT/boot/grub/menu.lst" <<MENU
default 0
timeout 5

title LVGD486 Installer
    root (hd0,0)
    kernel /boot/bzImage root=/dev/ram0 rootfstype=ext2 rw ramdisk_size=${INITRD_RAM_SIZE_KB}
    initrd /boot/installer-initrd.gz
MENU

# ------------------------------------------------------------
# Verify USB filesystem contents
# ------------------------------------------------------------

echo "Verifying USB installer files..."

[ -s "$MOUNT_POINT/bin/busybox" ] ||
    fail "BusyBox missing from USB filesystem."

for binary in \
    mke2fs \
    e2fsck \
    tune2fs \
    dumpe2fs \
    resize2fs
do
    destination="${MOUNT_POINT}/sbin/${binary}"

    [ -s "$destination" ] ||
        fail "e2fsprogs utility missing from USB filesystem: $binary"

    [ -x "$destination" ] ||
        fail "e2fsprogs utility is not executable on USB filesystem: $binary"

    binary_info="$("$FILE" "$destination")"

    case "$binary_info" in
        *"statically linked"*) ;;
        *)
            fail "Copied e2fsprogs utility is not static: $destination"
            ;;
    esac

    case "$binary_info" in
        *"dynamically linked"*)
            fail "Copied e2fsprogs utility is dynamically linked: $destination"
            ;;
    esac

    echo "Verified: /sbin/${binary}"
done

[ -s "$MOUNT_POINT/boot/bzImage" ] ||
    fail "Kernel missing from USB filesystem."

[ -s "$MOUNT_POINT/boot/installer-initrd.gz" ] ||
    fail "Compressed initrd missing from USB filesystem."

[ -s "$MOUNT_POINT/distrodev.tar.gz" ] ||
    fail "Installer archive missing from USB filesystem."

[ -x "$MOUNT_POINT/install.sh" ] ||
    fail "Installer script missing or not executable."

[ -s "$MOUNT_POINT/boot/grub/stage1" ] ||
    fail "GRUB stage1 missing from USB filesystem."

[ -s "$MOUNT_POINT/boot/grub/stage2" ] ||
    fail "GRUB stage2 missing from USB filesystem."

[ -s "$MOUNT_POINT/boot/grub/menu.lst" ] ||
    fail "GRUB menu missing from USB filesystem."

gzip -t "$MOUNT_POINT/boot/installer-initrd.gz"

sync

umount "$MOUNT_POINT"
MOUNTED=0

"$E2FSCK" -fn "$LOOP_DEVICE"

"$LOSSETUP" -d "$LOOP_DEVICE"
LOOP_ATTACHED=0

# ============================================================
# Install GRUB Legacy into the USB image MBR
# ============================================================

echo "Installing GRUB Legacy..."

printf '(hd0) %s\n' "$USB_IMAGE" > "$DEVICE_MAP"

dd if="$USB_IMAGE" of="$MBR_BACKUP" \
    bs="$SECTOR_SIZE" count=1

dd if="$USB_IMAGE" of="$TABLE_BEFORE" \
    bs=1 skip=446 count=66

if ! "$GRUB_SHELL" \
    --batch \
    --no-curses \
    --device-map="$DEVICE_MAP" \
    > "$GRUB_LOG" 2>&1 <<GRUB
geometry (hd0) ${CYLINDERS} ${HEADS} ${SECTORS_PER_TRACK} ${GEOMETRY_SECTORS}
root (hd0,0)
setup (hd0)
quit
GRUB
then
    cat "$GRUB_LOG" >&2

    dd if="$MBR_BACKUP" of="$USB_IMAGE" \
        bs="$SECTOR_SIZE" count=1 conv=notrunc

    fail "GRUB installation failed; original MBR restored."
fi

cat "$GRUB_LOG"

dd if="$USB_IMAGE" of="$TABLE_AFTER" \
    bs=1 skip=446 count=66

if ! cmp -s "$TABLE_BEFORE" "$TABLE_AFTER"; then
    dd if="$MBR_BACKUP" of="$USB_IMAGE" \
        bs="$SECTOR_SIZE" count=1 conv=notrunc

    fail "GRUB changed the partition table; original MBR restored."
fi

"$PERL" - "$USB_IMAGE" <<'PERL'
use strict;
use warnings;

my ($image) = @ARGV;

open(my $fh, "<", $image) or die "Cannot open image";
binmode($fh);

seek($fh, 510, 0) or die "Cannot seek to MBR signature";
read($fh, my $signature, 2) == 2 or die "Cannot read MBR signature";

die "Invalid MBR signature after GRUB installation\n"
    unless $signature eq "\x55\xaa";

close($fh);

print "Final MBR signature validated.\n";
PERL

# ------------------------------------------------------------
# Complete
# ------------------------------------------------------------

SUCCESS=1

echo
echo "USB installer image created successfully."
echo "Image: $USB_IMAGE"
echo "Size:  ${IMAGE_SIZE_MB} MiB"
echo "Boot:  Linux 2.4 kernel + compressed ext2 initrd"
echo
echo "USB media includes:"
echo "  /bin/busybox"
echo "  /sbin/mke2fs"
echo "  /sbin/e2fsck"
echo "  /sbin/tune2fs"
echo "  /sbin/dumpe2fs"
echo "  /sbin/resize2fs"
echo
echo "The installer init process adds /install-media/sbin to PATH."
echo "The image has not been written to a physical USB device."
echo "Test it in a virtual machine before deployment."