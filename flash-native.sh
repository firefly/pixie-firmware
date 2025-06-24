#!/bin/bash

# Native Flash Script (without Docker device mapping)
# This uses esptool directly from the build output

set -e

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m'

printf "${YELLOW}🔥 Firefly Pixie Native Flash${NC}\n"
printf "==============================\n"

# Auto-detect device
DEVICE_PATTERNS=("/dev/tty.usbmodem*" "/dev/tty.usbserial-*" "/dev/tty.SLAB_USBtoUART*")
DEVICE=""

for pattern in "${DEVICE_PATTERNS[@]}"; do
    devices=$(ls $pattern 2>/dev/null || true)
    if [ ! -z "$devices" ]; then
        DEVICE=$(echo $devices | head -n1)
        break
    fi
done

if [ -z "$DEVICE" ]; then
    printf "${RED}❌ No ESP32 device found${NC}\n"
    printf "Available devices:\n"
    ls /dev/tty.* 2>/dev/null | head -10
    exit 1
fi

printf "${GREEN}✅ Found device: $DEVICE${NC}\n"

# Build first
printf "\n"
printf "🔨 Building firmware...\n"
printf "   📦 Cleaning previous build...\n"

# Redirect verbose output to log file, show only important messages
BUILD_LOG="/tmp/pixie-build.log"
echo "" > "$BUILD_LOG"  # Clear log file

{
    docker run --rm -v $PWD:/project -w /project -e HOME=/tmp -e IDF_TARGET=esp32c3 espressif/idf bash -c "idf.py fullclean > /dev/null 2>&1 && idf.py build" 2>&1
} | while IFS= read -r line; do
    # Log everything to file for debugging
    echo "$line" >> "$BUILD_LOG"
    
    # Show only important build progress messages
    if [[ "$line" =~ "Generating" ]] || [[ "$line" =~ "Creating" ]] || [[ "$line" =~ "Linking" ]] || [[ "$line" =~ "Project build complete" ]] || [[ "$line" =~ "To flash" ]]; then
        printf "\r   %s\n" "$line"
    elif [[ "$line" =~ "Building" ]] && [[ "$line" =~ ".bin" ]]; then
        printf "\r   %s\n" "$line"
    fi
done

# Check if build succeeded by looking for the binary files
if [ -f "build/pixie.bin" ] && [ -f "build/bootloader/bootloader.bin" ] && [ -f "build/partition_table/partition-table.bin" ]; then
    printf "   ${GREEN}✅ Build completed successfully${NC}\n"
    printf "   📄 Build log saved to: $BUILD_LOG\n"
else
    printf "${RED}❌ Build failed${NC}\n"
    printf "📄 Check build log for details: $BUILD_LOG\n"
    exit 1
fi

# Check if esptool is available locally
ESPTOOL_CMD=""
if command -v esptool &> /dev/null; then
    ESPTOOL_CMD="esptool"
elif python -m esptool --help &> /dev/null; then
    ESPTOOL_CMD="python -m esptool"
elif python3 -m esptool --help &> /dev/null; then
    ESPTOOL_CMD="python3 -m esptool"
else
    printf "\n"
    printf "⚠️  esptool not found. Installing via pip...\n"
    printf "\n"
    printf "Run this command to install esptool:\n"
    printf "  pip install esptool\n"
    printf "\n"
    printf "Or use Homebrew:\n"
    printf "  brew install esptool\n"
    printf "\n"
    printf "Then run this script again.\n"
    exit 1
fi

printf "   🔧 Using esptool: ${GREEN}$ESPTOOL_CMD${NC}\n"

printf "\n"
printf "⚡ Flashing firmware to device...\n"
printf "   📋 Target: $DEVICE\n"
printf "   ⚙️  Chip: ESP32-C3\n"
printf "   🚀 Baud: 460800\n"

# Flash using native esptool with cleaner output
FLASH_LOG="/tmp/pixie-flash.log"
echo "" > "$FLASH_LOG"  # Clear log file

{
    $ESPTOOL_CMD --chip esp32c3 -p $DEVICE -b 460800 --before default_reset --after hard_reset write_flash \
        --flash_mode dio --flash_freq 80m --flash_size 16MB \
        0x0 build/bootloader/bootloader.bin \
        0x8000 build/partition_table/partition-table.bin \
        0x10000 build/pixie.bin 2>&1
} | while IFS= read -r line; do
    # Log everything for debugging
    echo "$line" >> "$FLASH_LOG"
    
    # Show progress for important flash steps
    if [[ "$line" =~ "Connecting" ]] || [[ "$line" =~ "Chip is" ]] || [[ "$line" =~ "Uploading stub" ]] || [[ "$line" =~ "Configuring flash" ]] || [[ "$line" =~ "Writing at" ]] || [[ "$line" =~ "Hash of data verified" ]] || [[ "$line" =~ "Leaving" ]]; then
        printf "\r   %s\n" "$line"
    elif [[ "$line" =~ "%" ]]; then
        # Show progress percentages on same line
        printf "\r   📦 %s" "$line"
    fi
done

printf "\n" # New line after progress

if [ $? -eq 0 ]; then
    printf "\n"
    printf "${GREEN}🎉 Flash successful!${NC}\n"
    printf "\n"
    printf "${YELLOW}🔧 To monitor serial output:${NC}\n"
    printf "  ${GREEN}screen $DEVICE 115200${NC}\n"
    printf "  # Press Ctrl+A then K to exit screen\n"
    printf "\n"
    printf "📄 Flash log saved to: $FLASH_LOG\n"
else
    printf "${RED}❌ Flash failed${NC}\n"
    printf "📄 Check flash log for details: $FLASH_LOG\n"
    exit 1
fi