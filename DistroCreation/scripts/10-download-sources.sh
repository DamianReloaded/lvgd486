#!/bin/sh

# ============================================================
# LVGD486 - Download Source Packages
# ============================================================
#
# Manual Download URLs
# --------------------
#
# Linux kernel 2.4.37.11:
# https://www.kernel.org/pub/linux/kernel/v2.4/linux-2.4.37.11.tar.bz2
#
# BusyBox 1.00:
# https://busybox.net/downloads/busybox-1.00.tar.bz2
#
# GNU Make 3.81:
# https://ftp.gnu.org/gnu/make/make-3.81.tar.gz
#
# GNU Binutils 2.15:
# https://ftp.gnu.org/gnu/binutils/binutils-2.15.tar.bz2
#
# GCC 4.0.2:
# https://ftp.gnu.org/pub/gnu/gcc/gcc-4.0.2/gcc-4.0.2.tar.bz2
#
# GCC 5.4.0:
# https://ftp.gnu.org/pub/gnu/gcc/gcc-5.4.0/gcc-5.4.0.tar.bz2
#
# GDB 6.3 (original source):
# https://archive.debian.org/debian-archive/debian/pool/main/g/gdb/gdb_6.3.orig.tar.gz
#
# GNU C Library 2.3.2:
# https://ftp.gnu.org/gnu/glibc/glibc-2.3.2.tar.gz
#
# LinuxThreads 2.3.2:
# https://ftp.gnu.org/gnu/glibc/glibc-linuxthreads-2.3.2.tar.gz
#
# NFS Utilities 1.0.6 (original source):
# https://archive.debian.org/debian-archive/debian/pool/main/n/nfs-utils/nfs-utils_1.0.6.orig.tar.gz
#
# portmap 5 (original source):
# https://archive.debian.org/debian-archive/debian/pool/main/p/portmap/portmap_5.orig.tar.gz
#
# portmap Debian patch 5-9:
# https://archive.debian.org/debian-archive/debian/pool/main/p/portmap/portmap_5-9.diff.gz
#
# ncurses 5.4 (original source):
# https://archive.debian.org/debian-archive/debian/pool/main/n/ncurses/ncurses_5.4.orig.tar.gz
#
# Bash 2.05b (original source):
# https://archive.debian.org/debian/pool/main/b/bash/bash_2.05b-2.orig.tar.gz
#
# kbd 1.12 (original source):
# https://archive.debian.org/debian/pool/main/k/kbd/kbd_1.12.orig.tar.gz
#
# Midnight Commander 4.6.1 (original source):
# https://archive.debian.org/debian/pool/main/m/mc/mc_4.6.1.orig.tar.gz
#
# GRUB Legacy (original source):
# https://archive.debian.org/debian/pool/main/g/grub/grub_0.95+cvs20040624.orig.tar.gz
#
# e2fsprogs 1.37 (original upstream source):
# https://archive.debian.org/debian-archive/debian/pool/main/e/e2fsprogs/e2fsprogs_1.37.orig.tar.gz
#
# ============================================================

set -e

# ============================================================
# Constants
# ============================================================

DOWNLOAD_DIR="/home/user/download"

KERNEL_VERSION="2.4.37.11"
BUSYBOX_VERSION="1.00"
MAKE_VERSION="3.81"
BINUTILS_VERSION="2.15"
GCC4_VERSION="4.0.2"
GCC5_VERSION="5.4.0"
GDB_VERSION="6.3"
GLIBC_VERSION="2.3.2"
NFS_UTILS_VERSION="1.0.6"
PORTMAP_VERSION="5"
PORTMAP_DEBIAN_VERSION="5-9"
NCURSES_VERSION="5.4"
BASH_VERSION="2.05b"
BASH_DEBIAN_VERSION="2-26"
KBD_VERSION="1.12"
MC_VERSION="4.6.1"
GRUB_VERSION="0.95+cvs20040624"
E2FSPROGS_VERSION="1.37"

# ============================================================
# Archive filenames
# ============================================================

KERNEL_ARCHIVE="linux-${KERNEL_VERSION}.tar.bz2"
BUSYBOX_ARCHIVE="busybox-${BUSYBOX_VERSION}.tar.bz2"
MAKE_ARCHIVE="make-${MAKE_VERSION}.tar.gz"
BINUTILS_ARCHIVE="binutils-${BINUTILS_VERSION}.tar.bz2"
GCC4_ARCHIVE="gcc-${GCC4_VERSION}.tar.bz2"
GCC5_ARCHIVE="gcc-${GCC5_VERSION}.tar.bz2"
GDB_ARCHIVE="gdb_${GDB_VERSION}.orig.tar.gz"
GLIBC_ARCHIVE="glibc-${GLIBC_VERSION}.tar.gz"
GLIBC_THREADS_ARCHIVE="glibc-linuxthreads-${GLIBC_VERSION}.tar.gz"
NFS_UTILS_ARCHIVE="nfs-utils_${NFS_UTILS_VERSION}.orig.tar.gz"
PORTMAP_ARCHIVE="portmap_${PORTMAP_VERSION}.orig.tar.gz"
PORTMAP_PATCH="portmap_${PORTMAP_DEBIAN_VERSION}.diff.gz"
NCURSES_ARCHIVE="ncurses_${NCURSES_VERSION}.orig.tar.gz"
BASH_ARCHIVE="bash_${BASH_VERSION}-2.orig.tar.gz"
KBD_ARCHIVE="kbd_${KBD_VERSION}.orig.tar.gz"
MC_ARCHIVE="mc_${MC_VERSION}.orig.tar.gz"
GRUB_ARCHIVE="grub_${GRUB_VERSION}.orig.tar.gz"
E2FSPROGS_ARCHIVE="e2fsprogs_${E2FSPROGS_VERSION}.orig.tar.gz"

# ============================================================
# Download URLs
# ============================================================

KERNEL_URL="https://www.kernel.org/pub/linux/kernel/v2.4/${KERNEL_ARCHIVE}"
BUSYBOX_URL="https://busybox.net/downloads/${BUSYBOX_ARCHIVE}"
MAKE_URL="https://ftp.gnu.org/gnu/make/${MAKE_ARCHIVE}"
BINUTILS_URL="https://ftp.gnu.org/gnu/binutils/${BINUTILS_ARCHIVE}"
GCC4_URL="https://ftp.gnu.org/pub/gnu/gcc/gcc-${GCC4_VERSION}/${GCC4_ARCHIVE}"
GCC5_URL="https://ftp.gnu.org/pub/gnu/gcc/gcc-${GCC5_VERSION}/${GCC5_ARCHIVE}"
GDB_URL="https://archive.debian.org/debian-archive/debian/pool/main/g/gdb/${GDB_ARCHIVE}"
GLIBC_URL="https://ftp.gnu.org/gnu/glibc/${GLIBC_ARCHIVE}"
GLIBC_THREADS_URL="https://ftp.gnu.org/gnu/glibc/${GLIBC_THREADS_ARCHIVE}"
NFS_UTILS_URL="https://archive.debian.org/debian-archive/debian/pool/main/n/nfs-utils/${NFS_UTILS_ARCHIVE}"
PORTMAP_URL="https://archive.debian.org/debian-archive/debian/pool/main/p/portmap/${PORTMAP_ARCHIVE}"
PORTMAP_PATCH_URL="https://archive.debian.org/debian-archive/debian/pool/main/p/portmap/${PORTMAP_PATCH}"
NCURSES_URL="https://archive.debian.org/debian-archive/debian/pool/main/n/ncurses/${NCURSES_ARCHIVE}"
BASH_URL="https://archive.debian.org/debian/pool/main/b/bash/${BASH_ARCHIVE}"
KBD_URL="https://archive.debian.org/debian/pool/main/k/kbd/${KBD_ARCHIVE}"
MC_URL="https://archive.debian.org/debian/pool/main/m/mc/${MC_ARCHIVE}"
GRUB_URL="https://archive.debian.org/debian/pool/main/g/grub/${GRUB_ARCHIVE}"
E2FSPROGS_URL="https://archive.debian.org/debian-archive/debian/pool/main/e/e2fsprogs/${E2FSPROGS_ARCHIVE}"

# ============================================================
# Create download directory
# ============================================================

mkdir -p "$DOWNLOAD_DIR" || exit 1
cd "$DOWNLOAD_DIR" || exit 1

# ============================================================
# Download source packages
# ============================================================

wget -c "$KERNEL_URL" || exit 1
wget -c "$BUSYBOX_URL" || exit 1
wget -c "$MAKE_URL" || exit 1
wget -c "$BINUTILS_URL" || exit 1
wget -c "$GCC4_URL" || exit 1
wget -c "$GCC5_URL" || exit 1
wget -c "$GDB_URL" || exit 1
wget -c "$GLIBC_URL" || exit 1
wget -c "$GLIBC_THREADS_URL" || exit 1
wget -c "$NFS_UTILS_URL" || exit 1
wget -c "$PORTMAP_URL" || exit 1
wget -c "$PORTMAP_PATCH_URL" || exit 1
wget -c "$NCURSES_URL" || exit 1
wget -c "$BASH_URL" || exit 1
wget -c "$KBD_URL" || exit 1
wget -c "$MC_URL" || exit 1
wget -c "$GRUB_URL" || exit 1
wget -c "$E2FSPROGS_URL" || exit 1

# ============================================================
# Final report
# ============================================================

echo
echo "All source archives downloaded successfully."
echo "Download directory: $DOWNLOAD_DIR"
echo "GRUB source: $DOWNLOAD_DIR/$GRUB_ARCHIVE"
echo "E2FSPROGS source: $DOWNLOAD_DIR/$E2FSPROGS_ARCHIVE"