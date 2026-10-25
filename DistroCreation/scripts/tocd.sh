#!/bin/sh

# ============================================================
# LVGD486 - Package a file into a CD-ROM ISO image
# Script: tocd.sh
#
# Usage:
#   sh tocd.sh <filename> [output.iso]
#
# Defaults:
#   Output: ./<filename>.iso
# ============================================================

set -eu

# ============================================================
# Constants
# ============================================================

VOLUME_ID="LVGD486_TOOLS"

# ============================================================
# Error handling
# ============================================================

fail()
{
    echo "ERROR: $*" >&2
    exit 1
}

# ============================================================
# Validate arguments
# ============================================================

[ "$#" -ge 1 ] ||
    fail "Usage: sh tocd.sh <filename> [output.iso]"

[ "$#" -le 2 ] ||
    fail "Too many arguments."

INPUT=$1
INPUT_BASE=$(basename -- "$INPUT")
OUTPUT=${2:-"${INPUT_BASE}.iso"}

[ -f "$INPUT" ] ||
    fail "Input file not found: $INPUT"

# ============================================================
# Locate ISO creation utility
# ============================================================

if command -v genisoimage >/dev/null 2>&1; then
    ISO_TOOL=$(command -v genisoimage)
elif command -v mkisofs >/dev/null 2>&1; then
    ISO_TOOL=$(command -v mkisofs)
else
    fail "Neither genisoimage nor mkisofs is installed."
fi

# ============================================================
# Resolve input and output paths
# ============================================================

INPUT_DIR=$(CDPATH= cd -- "$(dirname -- "$INPUT")" && pwd)
INPUT_PATH="${INPUT_DIR}/${INPUT_BASE}"

case "$OUTPUT" in
    /*) OUTPUT_PATH=$OUTPUT ;;
    *)  OUTPUT_PATH="$(pwd)/$OUTPUT" ;;
esac

OUTPUT_DIR=$(dirname -- "$OUTPUT_PATH")

[ -d "$OUTPUT_DIR" ] ||
    fail "Output directory does not exist: $OUTPUT_DIR"

[ "$INPUT_PATH" != "$OUTPUT_PATH" ] ||
    fail "Output path must differ from input path."

# ============================================================
# Display build information
# ============================================================

echo "Input:  $INPUT_PATH"
echo "Inside ISO: $INPUT_BASE"
echo "Output: $OUTPUT_PATH"
echo "Tool:   $ISO_TOOL"
echo

# ============================================================
# Create temporary staging directory
# ============================================================

TMP_DIR=$(mktemp -d "${TMPDIR:-/tmp}/lvgd486-iso.XXXXXX") ||
    fail "Could not create temporary directory."

trap 'rm -rf "$TMP_DIR"' 0
trap 'exit 1' 1 2 3 15

# ============================================================
# Stage input file using its original filename
# ============================================================

cp "$INPUT_PATH" "${TMP_DIR}/${INPUT_BASE}" ||
    fail "Could not stage input file."

# Preserve executable permissions when the input is executable.
if [ -x "$INPUT_PATH" ]; then
    chmod 755 "${TMP_DIR}/${INPUT_BASE}"
else
    chmod 644 "${TMP_DIR}/${INPUT_BASE}"
fi

# ============================================================
# Create ISO image
# ============================================================

echo "Creating ISO image..."

"$ISO_TOOL" \
    -V "$VOLUME_ID" \
    -R \
    -J \
    -o "$OUTPUT_PATH" \
    "$TMP_DIR" ||
    fail "ISO creation failed."

[ -s "$OUTPUT_PATH" ] ||
    fail "ISO was not created correctly."

# ============================================================
# Summary
# ============================================================

echo
echo "ISO created successfully:"
ls -lh "$OUTPUT_PATH"

echo
echo "File inside ISO: ${INPUT_BASE}"
echo "Volume label: ${VOLUME_ID}"

exit 0