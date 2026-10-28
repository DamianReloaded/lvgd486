#!/bin/bash
# LVGD486 - Create distrodev root filesystem archive
set -eu
ROOT="${ROOT:-/home/user/distrodev}"
OUTPUT="${OUTPUT:-/home/user/images/distrodev.tar.gz}"
[ "$(id -u)" -eq 0 ] || { echo "ERROR: Run as root to preserve ownership and device nodes." >&2; exit 1; }
[ -d "$ROOT" ] || { echo "ERROR: Root filesystem not found: $ROOT" >&2; exit 1; }
mkdir -p "$(dirname "$OUTPUT")"
[ ! -e "$OUTPUT" ] || { echo "ERROR: Archive already exists: $OUTPUT (remove it before rebuilding)." >&2; exit 1; }
TMP="${OUTPUT}.tmp.$$"
trap 'rm -f "$TMP"' EXIT
printf 'Archiving %s to %s ...\n' "$ROOT" "$OUTPUT"
# Preserve ownership, permissions, links, sparse files and device nodes.
tar --numeric-owner --sparse -czpf "$TMP" -C "$ROOT" .
tar -tzf "$TMP" >/dev/null
mv "$TMP" "$OUTPUT"
trap - EXIT
printf 'Archive created: %s (%s bytes)\n' "$OUTPUT" "$(wc -c < "$OUTPUT" | tr -d ' ')"
