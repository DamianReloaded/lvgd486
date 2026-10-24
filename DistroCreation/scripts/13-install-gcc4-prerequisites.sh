#!/bin/sh

# ============================================================
# LVGD486 - Extract Sources and Prepare GCC Prerequisites
# Script: 11-extract-sources.sh
# ============================================================

# Constants
SOURCES_DIR="/home/user/sources"
DOWNLOAD_DIR="/home/user/download"

GCC_SOURCE="${SOURCES_DIR}/gcc"
GCC_PREREQUISITES="${GCC_SOURCE}/contrib/download_prerequisites"

# Verify GCC source tree
if [ ! -x "$GCC_PREREQUISITES" ]; then
    echo "Error: GCC prerequisite script not found."
    echo "Expected: ${GCC_PREREQUISITES}"
    echo "Run 10-download-sources.sh and extract the GCC archive first."
    exit 1
fi

# Download and prepare compatible prerequisite libraries
cd "$GCC_SOURCE" || exit 1
"$GCC_PREREQUISITES" || exit 1

echo "GCC prerequisite libraries prepared successfully."
