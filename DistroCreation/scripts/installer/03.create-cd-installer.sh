#!/bin/bash

# ============================================================
# LVGD486 - Build Bootable CD Installer ISO
# Debian Sarge host / Linux 2.4 target
# GRUB Legacy El Torito boot image.
# Uses a compressed ext2 initrd for early boot.
# Searches ISO9660 CD-ROM media after the kernel starts.
# Includes BusyBox, distro utilities and static e2fsprogs.
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

# GRUB Legacy El Torito boot image.
ELTORITO_STAGE="${ELTORITO_STAGE:-/home/user/sources/build/grub/stage2/stage2_eltorito}"

# Built e2fsprogs 1.37 static i386 utilities.
E2FSPROGS_BUILD="/home/user/sources/build/e2fsprogs"

E2FS_MKE2FS="${E2FSPROGS_BUILD}/misc/mke2fs"
E2FS_E2FSCK="${E2FSPROGS_BUILD}/e2fsck/e2fsck"
E2FS_TUNE2FS="${E2FSPROGS_BUILD}/misc/tune2fs"
E2FS_DUMPE2FS="${E2FSPROGS_BUILD}/misc/dumpe2fs"
E2FS_RESIZE2FS="${E2FSPROGS_BUILD}/resize/resize2fs"

CD_ISO="${IMAGE_DIR}/lvgd486-installer-cd.iso"
MOUNT_POINT="/mnt/lvgd486-installer"
LOOP_DEVICE="/dev/loop1"

# ------------------------------------------------------------
# Image and initrd settings
# ------------------------------------------------------------

INITRD_SIZE_MB=32
INITRD_RAM_SIZE_KB=65536
BLOCK_SIZE=1024

ISO_VOLUME="LVGD486_INSTALLER"

# ------------------------------------------------------------
# Host utilities
# ------------------------------------------------------------

MKE2FS="$(command -v mke2fs || true)"
E2FSCK="$(command -v e2fsck || true)"
LOSSETUP="$(command -v losetup || true)"
FILE="$(command -v file || true)"
LDD="$(command -v ldd || true)"

MKISOFS="$(command -v mkisofs || true)"
GENISOIMAGE="$(command -v genisoimage || true)"
ISOINFO="$(command -v isoinfo || true)"

ISO_BUILDER=""

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
        rm -f "$CD_ISO"
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

[ -s "$ELTORITO_STAGE" ] ||
    fail "GRUB El Torito boot image not found: $ELTORITO_STAGE"

[ -n "$MKE2FS" ] ||
    fail "Host mke2fs is required."

[ -n "$E2FSCK" ] ||
    fail "Host e2fsck is required."

[ -n "$LOSSETUP" ] ||
    fail "losetup is required."

[ -n "$FILE" ] ||
    fail "file is required."

if [ -n "$MKISOFS" ]; then
    ISO_BUILDER="$MKISOFS"
elif [ -n "$GENISOIMAGE" ]; then
    ISO_BUILDER="$GENISOIMAGE"
else
    fail "Install mkisofs or genisoimage to build the ISO."
fi

for utility in \
    dd cp mkdir chmod ln mknod mount umount \
    tar gzip grep awk sort cmp ls cat rm \
    sync mktemp dirname id sleep
do
    command -v "$utility" >/dev/null 2>&1 ||
        fail "Required utility not found: $utility"
done

# ------------------------------------------------------------
# Validate static e2fsprogs utilities
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

echo "All e2fsprogs utilities are static 32-bit x86 executables."

# ------------------------------------------------------------
# Validate GRUB boot files
# ------------------------------------------------------------

GRUB_INFO="$("$FILE" "$GRUB_SHELL")"
echo "$GRUB_INFO"

case "$GRUB_INFO" in
    *"ELF 32-bit"*80386*) ;;
    *"ELF 32-bit"*i386*) ;;
    *)
        fail "GRUB shell must be a 32-bit x86 executable."
        ;;
esac

for stage in stage1 stage2
do
    [ -s "${GRUB_DIR}/${stage}" ] ||
        fail "GRUB stage file missing: ${GRUB_DIR}/${stage}"
done

tar -tzf "$ARCHIVE" >/dev/null ||
    fail "Installer archive validation failed."

[ ! -e "$CD_ISO" ] ||
    fail "Output already exists: $CD_ISO"

# ------------------------------------------------------------
# Validate kernel configuration
# ------------------------------------------------------------

echo "Checking kernel configuration..."

for option in \
    CONFIG_BLK_DEV_INITRD \
    CONFIG_BLK_DEV_RAM \
    CONFIG_EXT2_FS \
    CONFIG_ISO9660_FS
do
    if ! grep -qx "${option}=y" "$KERNEL_CONFIG"; then
        fail "${option} must be built into the kernel (=y)."
    fi
done

# At least one supported CD-ROM driver must be built in.
if ! grep -qx 'CONFIG_BLK_DEV_IDECD=y' "$KERNEL_CONFIG" &&
   ! grep -qx 'CONFIG_BLK_DEV_SR=y' "$KERNEL_CONFIG"; then
    fail "Enable CONFIG_BLK_DEV_IDECD=y or CONFIG_BLK_DEV_SR=y in the kernel."
fi

# A SCSI CD-ROM driver requires SCSI support as well.
if grep -qx 'CONFIG_BLK_DEV_SR=y' "$KERNEL_CONFIG" &&
   ! grep -qx 'CONFIG_SCSI=y' "$KERNEL_CONFIG"; then
    fail "CONFIG_BLK_DEV_SR=y requires CONFIG_SCSI=y."
fi

echo "Required initrd, RAM disk, ext2 and ISO9660 options are built in."
echo "A built-in CD-ROM driver is configured."

# ------------------------------------------------------------
# Prepare temporary directories
# ------------------------------------------------------------

WORK="$(mktemp -d /tmp/lvgd486-cd.XXXXXX)"

ISO_ROOT="${WORK}/iso-root"
INITRD_RAW="${WORK}/installer-initrd.img"
INITRD_GZ="${WORK}/installer-initrd.gz"

GRUB_LIBS_FILE="${WORK}/grub-libraries"
LIBRARIES_LIST="${WORK}/libraries.list"
ISO_INFO="${WORK}/iso-info.txt"

mkdir -p "$IMAGE_DIR"
mkdir -p "$MOUNT_POINT"
mkdir -p "$ISO_ROOT"

if mount | grep -F " on ${MOUNT_POINT} " >/dev/null 2>&1; then
    fail "Mount point is already mounted: $MOUNT_POINT"
fi

if [ -n "$(ls -A "$MOUNT_POINT")" ]; then
    fail "Mount point is not empty: $MOUNT_POINT"
fi

if "$LOSSETUP" "$LOOP_DEVICE" >/dev/null 2>&1; then
    fail "Loop device is already in use: $LOOP_DEVICE"
fi

echo
echo "LVGD486 CD Installer"
echo "--------------------"
echo "ISO:              $CD_ISO"
echo "ISO builder:      $ISO_BUILDER"
echo "El Torito image:  $ELTORITO_STAGE"
echo "Initrd raw size:  ${INITRD_SIZE_MB} MiB"
echo "Initrd RAM size:  ${INITRD_RAM_SIZE_KB} KiB"
echo "Filesystem:       ext2 revision 0"
echo

# ============================================================
# Build the compressed ext2 initrd
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
# Copy distro utilities
# ------------------------------------------------------------

echo "Copying distro utilities into initrd..."

cp -a "$DISTRO_BIN/." "$MOUNT_POINT/bin/"
cp -a "$DISTRO_SBIN/." "$MOUNT_POINT/sbin/"

[ -x "$MOUNT_POINT/bin/busybox" ] ||
    fail "Copied BusyBox is missing or not executable."

chmod 755 "$MOUNT_POINT/bin/busybox"

# ------------------------------------------------------------
# Copy GRUB executable and runtime libraries
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

# Include GRUB stages used by the installed system.
cp -a "$GRUB_DIR/." "$MOUNT_POINT/boot/grub/"

# ------------------------------------------------------------
# Create device nodes in initrd
# ------------------------------------------------------------

echo "Creating initrd device nodes..."

mknod -m 600 "$MOUNT_POINT/dev/console" c 5 1
mknod -m 666 "$MOUNT_POINT/dev/null" c 1 3
mknod -m 666 "$MOUNT_POINT/dev/zero" c 1 5
mknod -m 666 "$MOUNT_POINT/dev/tty" c 5 0
mknod -m 600 "$MOUNT_POINT/dev/tty0" c 4 0
mknod -m 600 "$MOUNT_POINT/dev/tty1" c 4 1
mknod -m 666 "$MOUNT_POINT/dev/ptmx" c 5 2

# RAM disk.
mknod -m 600 "$MOUNT_POINT/dev/ram0" b 1 0

# IDE disks and IDE CD-ROM devices.
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

# SCSI/USB disks.
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

# SCSI CD-ROM devices: major 11.
for cdNumber in 0 1 2 3
do
    mknod -m 660 "$MOUNT_POINT/dev/scd${cdNumber}" \
        b 11 "$cdNumber"
done

# ------------------------------------------------------------
# Linux 2.4 initrd startup script
# ------------------------------------------------------------

cat > "$MOUNT_POINT/linuxrc" <<'LINUXRC'
#!/bin/sh

export PATH=/bin:/sbin
export HOME=/

echo
echo "LVGD486: starting RAM-based CD installer"

mount -t proc proc /proc 2>/dev/null || true

echo "Searching for the installer CD-ROM..."

attempt=0
media_found=0

while [ "$attempt" -lt 30 ]
do
    # IDE optical drives are commonly hdc or hdd.
    # Scan the remaining IDE devices too, allowing different hardware.
    for device in \
        /dev/hdc /dev/hdd /dev/hdb /dev/hda \
        /dev/hde /dev/hdf /dev/hdg /dev/hdh \
        /dev/scd0 /dev/scd1 /dev/scd2 /dev/scd3
    do
        if mount -t iso9660 -o ro "$device" /install-media 2>/dev/null
        then
            if [ -f /install-media/distrodev.tar.gz ] &&
               [ -x /install-media/install.sh ]
            then
                echo "Installer CD found on $device"

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
    echo "WARNING: Installer CD-ROM was not found."
    echo "Check the CD-ROM driver and ISO9660 kernel options."
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

export PATH=/install-media/sbin:/install-media/bin:/bin:/sbin
export HOME=/
export PS1='LVGD486# '

mount -t proc proc /proc 2>/dev/null || true

echo
echo "LVGD486 CD Installer"
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
    echo "Prepare the destination disk using fdisk."
    echo "Then run the installer, for example:"
    echo "  /install.sh /dev/hda2"
else
    echo "Installer CD-ROM is unavailable."
    echo "Check the CD-ROM driver and kernel configuration."
fi

echo

exec /bin/sh
INIT

chmod 755 "$MOUNT_POINT/sbin/init"

# ------------------------------------------------------------
# Verify initrd contents
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
# Populate ISO staging directory
# ============================================================

echo "Creating ISO directory structure..."

mkdir -p "$ISO_ROOT/bin"
mkdir -p "$ISO_ROOT/sbin"
mkdir -p "$ISO_ROOT/boot/grub"

# ------------------------------------------------------------
# Copy El Torito boot image
# ------------------------------------------------------------

echo "Copying GRUB El Torito boot image..."

cp -p "$ELTORITO_STAGE" \
    "$ISO_ROOT/boot/grub/stage2_eltorito"

# mkisofs/genisoimage updates the boot-info table in this file.
chmod u+w "$ISO_ROOT/boot/grub/stage2_eltorito"

# Preserve ordinary GRUB stages for use by the installer.
cp -a "$GRUB_DIR/." "$ISO_ROOT/boot/grub/"

# Restore the dedicated El Torito image after copying the stage directory.
cp -p "$ELTORITO_STAGE" \
    "$ISO_ROOT/boot/grub/stage2_eltorito"

chmod u+w "$ISO_ROOT/boot/grub/stage2_eltorito"

# ------------------------------------------------------------
# Copy utilities to the CD
# ------------------------------------------------------------

echo "Copying BusyBox and e2fsprogs utilities..."

cp -p "$BUSYBOX" "$ISO_ROOT/bin/busybox"
chmod 755 "$ISO_ROOT/bin/busybox"

cp -p "$E2FS_MKE2FS" "$ISO_ROOT/sbin/mke2fs"
cp -p "$E2FS_E2FSCK" "$ISO_ROOT/sbin/e2fsck"
cp -p "$E2FS_TUNE2FS" "$ISO_ROOT/sbin/tune2fs"
cp -p "$E2FS_DUMPE2FS" "$ISO_ROOT/sbin/dumpe2fs"
cp -p "$E2FS_RESIZE2FS" "$ISO_ROOT/sbin/resize2fs"

chmod 755 \
    "$ISO_ROOT/sbin/mke2fs" \
    "$ISO_ROOT/sbin/e2fsck" \
    "$ISO_ROOT/sbin/tune2fs" \
    "$ISO_ROOT/sbin/dumpe2fs" \
    "$ISO_ROOT/sbin/resize2fs"

# ------------------------------------------------------------
# Copy kernel, initrd, archive and installer
# ------------------------------------------------------------

echo "Copying kernel and installer files..."

cp -p "$KERNEL" "$ISO_ROOT/boot/bzImage"
cp -p "$INITRD_GZ" "$ISO_ROOT/boot/installer-initrd.gz"
cp -p "$ARCHIVE" "$ISO_ROOT/distrodev.tar.gz"
cp -p "$INSTALL_SCRIPT" "$ISO_ROOT/install.sh"

chmod 755 "$ISO_ROOT/install.sh"

# ------------------------------------------------------------
# GRUB Legacy CD configuration
# ------------------------------------------------------------

cat > "$ISO_ROOT/boot/grub/menu.lst" <<MENU
default 0
timeout 5

title LVGD486 CD Installer
    root (cd)
    kernel /boot/bzImage root=/dev/ram0 rootfstype=ext2 rw ramdisk_size=${INITRD_RAM_SIZE_KB}
    initrd /boot/installer-initrd.gz
MENU

# ------------------------------------------------------------
# Verify staged files
# ------------------------------------------------------------

echo "Verifying ISO staging directory..."

[ -s "$ISO_ROOT/boot/grub/stage2_eltorito" ] ||
    fail "El Torito boot image missing from ISO staging directory."

[ -s "$ISO_ROOT/boot/bzImage" ] ||
    fail "Kernel missing from ISO staging directory."

[ -s "$ISO_ROOT/boot/installer-initrd.gz" ] ||
    fail "Compressed initrd missing from ISO staging directory."

[ -s "$ISO_ROOT/distrodev.tar.gz" ] ||
    fail "Installer archive missing from ISO staging directory."

[ -x "$ISO_ROOT/install.sh" ] ||
    fail "Installer script missing or not executable."

[ -s "$ISO_ROOT/boot/grub/menu.lst" ] ||
    fail "GRUB menu missing from ISO staging directory."

gzip -t "$ISO_ROOT/boot/installer-initrd.gz"

for binary in \
    mke2fs \
    e2fsck \
    tune2fs \
    dumpe2fs \
    resize2fs
do
    destination="${ISO_ROOT}/sbin/${binary}"

    [ -s "$destination" ] ||
        fail "e2fsprogs utility missing from ISO: $binary"

    [ -x "$destination" ] ||
        fail "e2fsprogs utility is not executable: $binary"

    binary_info="$("$FILE" "$destination")"

    case "$binary_info" in
        *"statically linked"*) ;;
        *)
            fail "e2fsprogs utility is not static: $destination"
            ;;
    esac

    case "$binary_info" in
        *"dynamically linked"*)
            fail "e2fsprogs utility is dynamically linked: $destination"
            ;;
    esac

    echo "Verified: /sbin/${binary}"
done

# ============================================================
# Build bootable ISO9660 image with El Torito
# ============================================================

echo
echo "Building bootable CD ISO..."

"$ISO_BUILDER" \
    -R \
    -V "$ISO_VOLUME" \
    -b boot/grub/stage2_eltorito \
    -c boot/boot.catalog \
    -no-emul-boot \
    -boot-load-size 4 \
    -boot-info-table \
    -o "$CD_ISO" \
    "$ISO_ROOT"

[ -s "$CD_ISO" ] ||
    fail "ISO builder did not produce an output image."

# ------------------------------------------------------------
# Validate ISO filesystem and El Torito boot record
# ------------------------------------------------------------

if [ -n "$ISOINFO" ]; then
    echo "Validating ISO9660 and El Torito boot records..."

    "$ISOINFO" -d -i "$CD_ISO" > "$ISO_INFO" ||
        fail "isoinfo could not read the generated ISO."

    cat "$ISO_INFO"

    grep -i "El Torito" "$ISO_INFO" >/dev/null 2>&1 ||
        fail "ISO does not report an El Torito boot record."
else
    echo "WARNING: isoinfo is not installed; skipping boot-record inspection."
    echo "Install the host ISO utilities package for additional validation."
fi

# Check the image format when the host file utility recognizes it.
ISO_FILE_INFO="$("$FILE" "$CD_ISO")"
echo "$ISO_FILE_INFO"

# ------------------------------------------------------------
# Complete
# ------------------------------------------------------------

sync
SUCCESS=1

echo
echo "CD installer ISO created successfully."
echo "ISO:   $CD_ISO"
echo "Size:  $(ls -lh "$CD_ISO" | awk '{print $5}')"
echo "Boot:  GRUB Legacy El Torito, no emulation"
echo "Linux: Linux 2.4 + compressed ext2 initrd"
echo
echo "ISO contents:"
echo "  /boot/grub/stage2_eltorito"
echo "  /boot/grub/menu.lst"
echo "  /boot/bzImage"
echo "  /boot/installer-initrd.gz"
echo "  /bin/busybox"
echo "  /sbin/mke2fs"
echo "  /sbin/e2fsck"
echo "  /sbin/tune2fs"
echo "  /sbin/dumpe2fs"
echo "  /sbin/resize2fs"
echo "  /distrodev.tar.gz"
echo "  /install.sh"
echo
echo "Test the ISO in a virtual machine before burning it."
echo "The ISO has not been written to a physical CD."