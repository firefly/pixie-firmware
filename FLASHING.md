# 🔥 Advanced Flashing Guide

Detailed flashing instructions and troubleshooting for Firefly Pixie firmware.

> **Quick Start**: For most users, simply run `./flash-native.sh` - see [README.md](README.md#quick-start) for basic instructions.

## 🚀 Flashing Methods

### Method 1: Native esptool (Recommended)

**Why this method?** Avoids Docker device mapping issues that are common with ESP32-C3.

```bash
# Install esptool (if not already installed)
pip install esptool
# or on macOS
brew install esptool

# Auto-detect and flash
./flash-native.sh
```

**What it does:**
1. Detects ESP32-C3 device automatically
2. Builds firmware using Docker
3. Flashes using native esptool
4. Shows detailed progress and error messages

### Method 2: Docker-based flashing

```bash
# Find your device first
./find-device.sh

# Flash with specific device path
./flash.sh /dev/tty.your_device
```

**Note:** Docker device mapping can be unreliable with ESP32-C3. Use Method 1 if you encounter issues.

### Method 3: Manual commands

```bash
# Build firmware
docker run --rm -v $PWD:/project -w /project -e HOME=/tmp espressif/idf idf.py build

# Flash manually
esptool --chip esp32c3 -p /dev/tty.your_device -b 460800 \
  --before default_reset --after hard_reset write_flash \
  --flash_mode dio --flash_freq 80m --flash_size 16MB \
  0x0 build/bootloader/bootloader.bin \
  0x8000 build/partition_table/partition-table.bin \
  0x10000 build/pixie.bin
```

## 🛠️ Troubleshooting

### Device Not Detected

```bash
# Scan for devices
./find-device.sh

# Manual check
ls /dev/tty.usb*    # macOS
ls /dev/ttyUSB*     # Linux
```

**Common device names:**
- **macOS**: `/dev/tty.usbmodem*`, `/dev/tty.usbserial-*`
- **Linux**: `/dev/ttyUSB*`, `/dev/ttyACM*`
- **Windows**: `COM*`

### Permission Issues

```bash
# Make scripts executable
chmod +x *.sh

# Linux: Add user to dialout group
sudo usermod -a -G dialout $USER
# Then logout and login again
```

### Flash Failures

1. **Enter Download Mode**: Hold BOOT button while connecting USB
2. **Lower Baud Rate**: Edit script to change `-b 460800` to `-b 115200`
3. **Reset Device**: Press RESET button after connecting
4. **Try Different USB Port**: Some ports provide better power/signal

### Device Disappears During Flash

This is a known issue with ESP32-C3 and Docker device mapping.

**Solutions:**
1. Use `./flash-native.sh` instead of Docker method
2. Unplug device, plug back in, retry immediately
3. Use shorter USB cable (reduces signal issues)

### macOS Specific Issues

```bash
# Install CH340 drivers if device not recognized
# Download from: https://github.com/adrianmihalko/ch340g-ch34g-ch34x-mac-os-x-driver

# Check system information
system_profiler SPUSBDataType | grep -A 5 -B 5 "ESP32\|CH340\|CP210"
```

## 📡 Monitoring Serial Output

### Method 1: screen (built-in)
```bash
screen /dev/tty.your_device 115200
# Exit: Ctrl+A then K
```

### Method 2: minicom
```bash
# Install minicom
brew install minicom          # macOS
sudo apt install minicom      # Ubuntu

# Connect
minicom -D /dev/tty.your_device -b 115200
```

### Method 3: Docker monitor
```bash
docker run --device=/dev/tty.your_device:/dev/tty.your_device --rm -v $PWD:/project -w /project espressif/idf idf.py -p /dev/tty.your_device monitor
```

## ⚙️ Advanced Configuration

### Custom Flash Parameters

Edit scripts to modify flash behavior:

```bash
# In flash-native.sh, modify these parameters:
BAUD_RATE=460800        # Try 115200 for problematic connections
FLASH_MODE=dio          # Flash mode (dio/dout/qio/qout)
FLASH_FREQ=80m          # Flash frequency (80m/40m/26m/20m)
FLASH_SIZE=16MB         # Flash size (must match hardware)
```

### Partition Table

The firmware uses this partition layout:
```csv
# Name,   Type, SubType, Offset,  Size, Flags
nvs,      data, nvs,     0x9000,  0x6000,
phy_init, data, phy,     0xf000,  0x1000,
factory,  app,  factory, 0x10000, 0xF00000,
```

### Build Configuration

```bash
# Open menuconfig for advanced settings
docker run -it --rm -v $PWD:/project -w /project espressif/idf idf.py menuconfig

# Key settings:
# - Component config → ESP32C3-Specific → CPU frequency (160MHz)
# - Component config → FreeRTOS → Tick rate (1000 Hz)
# - Component config → ESP System Settings → Task watchdog timeout (60s)
```

## 🔍 Debug Information

### Enable Verbose Logging

```bash
# Add to sdkconfig:
CONFIG_LOG_DEFAULT_LEVEL_DEBUG=y
CONFIG_LOG_MAXIMUM_LEVEL_DEBUG=y

# Or via menuconfig:
# Component config → Log output → Default log verbosity → Debug
```

### Core Dump Analysis

```bash
# Enable core dumps in menuconfig:
# Component config → ESP System Settings → Core dump → Flash

# Extract core dump after crash:
docker run --rm -v $PWD:/project -w /project espressif/idf idf.py coredump-debug
```

## 📋 Verification

After successful flash:

```bash
# Check device info
screen /dev/tty.your_device 115200
# Device should boot and show main menu

# Verify features:
# 1. Navigate menu with buttons 3/4
# 2. Select wallet with button 2
# 3. Generate address with button 1
# 4. Toggle QR with button 3
# 5. Test games (Snake, Tetris, Pong, Le Space)
```

## 🔧 Factory Reset

```bash
# Erase entire flash (removes all settings and wallet data)
esptool --chip esp32c3 -p /dev/tty.your_device erase_flash

# Then reflash firmware
./flash-native.sh
```

**Warning:** This will permanently erase the wallet master seed. Only use if device is unrecoverable.

---

**Need help?** Check the [README.md](README.md) for basic instructions or [WALLET.md](WALLET.md) for wallet-specific issues.