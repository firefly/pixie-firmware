# 🔥 Firefly Pixie: Firmware

This is early prototype firmware to help design and refine the Firefly SDK.

It currently amounts a clone of Space Invaders, Snake, Tetris, Pong and a simple PoC Ethereum wallet. and has many of the necessary API to implement a hardware wallet.

- [Device](https://github.com/firefly/pixie-device) Design, Schematics and PCB
- [Case](https://github.com/firefly/pixie-case)

## 🎮 Features

### Classic Games
- **🐍 Snake**: Classic snake game with scoring and right-side orientation
- **🧩 Tetris**: Full Tetris implementation with line clearing and level progression  
- **🏓 Pong**: Player vs AI paddle game with speed boost
- **👾 Le Space**: Original Space Invaders clone with explosion effects
- **🎮 Retro Aesthetics**: Authentic pixel art and classic game mechanics

### Ethereum Wallet (PoC) [NOT TO BE USED IN PRODUCTION]
- **Hierarchical Deterministic (HD) Wallet**: BIP32-like key derivation with persistent storage
- **Full-Screen QR Codes**: Large, scannable QR codes with address text overlay  
- **Address Generation**: Generate new addresses from secure master seed
- **Persistent Storage**: Addresses and keys survive reboots and reflashing
- **Hardware Security**: Private keys never leave the device

### Additional Features
- **🖼️ GIF Viewer**: Display animated GIFs
- **📋 Scrolling Menu**: Circular navigation with separator

## 🚀 Quick Start

### Prerequisites
- [Docker](https://docs.docker.com/engine/install/) installed
- USB-C cable for device connection
- `esptool` installed: `pip install esptool` or `brew install esptool`

### Build and Flash (Recommended)

```bash
# 1. Clone repository with submodules
git clone --recurse-submodules https://github.com/firefly/pixie-firmware.git
cd pixie-firmware

# 2. Auto-detect device and flash
./flash-native.sh
```

### Alternative: Docker-based Build

```bash
# Build only
./build.sh

# Find your device
./find-device.sh

# Flash with Docker (requires testing)
./flash.sh /dev/tty.your_device
```

## 🔧 Script Reference

| Script | Description | Usage |
|--------|-------------|-------|
| **`flash-native.sh`** | **Recommended** - Auto-detects device, builds firmware, flashes with native esptool | `./flash-native.sh` |
| **`build.sh`** | Builds firmware using Docker ESP-IDF environment | `./build.sh` |
| **`find-device.sh`** | Scans for ESP32 devices on common ports | `./find-device.sh` |
| **`flash.sh`** | Docker-based flashing (needs testing) | `./flash.sh /dev/ttyUSB0` |
| **`generate.sh`** | Development tool - converts PNG assets to C headers | `./generate.sh` |

### Script Details

- **flash-native.sh**: Uses system `esptool` directly, avoiding Docker device mapping issues. Automatically detects ESP32-C3 devices and handles the complete build-flash workflow.
- **build.sh**: Containerized build using `espressif/idf` Docker image. Shows build size information.
- **find-device.sh**: Scans `/dev/tty.*` ports for ESP32 devices on macOS/Linux.
- **flash.sh**: Docker-based alternative to native flashing (requires device path parameter).
- **generate.sh**: Converts PNG files in `assets/` to C header files for embedding in firmware.

## 🎯 Controls

| Button | Function | Games | Wallet |
|--------|----------|-------|--------|
| **Button 1** (top) | Primary Action | Shoot, Rotate, Generate | Generate Address |
| **Button 2** | Select/Exit | Pause (short), Exit (hold) | Exit Wallet |
| **Button 3** | Up/Navigation | Move Up/Right | Toggle QR Display |
| **Button 4** (bottom) | Down/Navigation | Move Down/Left | (unused) |

**Game-Specific Controls:**
- **🚀 Le Space**: Button 1=Shoot, Button 3/4=Ship movement, Button 2=Exit
- **🐍 Snake**: Button 1=Rotate direction, Button 3/4=Smart movement 
- **🧩 Tetris**: Button 1=Rotate piece, Button 3/4=Move piece
- **🏓 Pong**: Button 1=Speed boost, Button 3/4=Paddle movement
- **💰 Wallet**: Button 1=New address, Button 3=QR toggle

## 🖥️ Monitoring Output

```bash
# Method 1: Screen (built-in)
screen /dev/tty.your_device 115200
# Exit: Ctrl+A then K

# Method 2: macOS/Linux
minicom -D /dev/tty.your_device -b 115200

# Method 3: Docker monitor
docker run --device=/dev/tty.your_device:/dev/tty.your_device --rm -v $PWD:/project -w /project espressif/idf idf.py -p /dev/tty.your_device monitor
```

## 🛠️ Troubleshooting

### Build Issues
```bash
# Check and update submodules
git submodule status
git submodule update --init --recursive
git pull --recurse-submodules
```

### Flashing Issues
- **Recommended**: Use `./flash-native.sh` to avoid Docker device mapping issues
- **Download Mode**: Hold BOOT button while connecting USB
- **Lower Baud Rate**: Change `-b 460800` to `-b 115200` in scripts

### Device Detection
- **macOS**: `/dev/tty.usbmodem*`, `/dev/tty.usbserial-*`
- **Linux**: `/dev/ttyUSB*`, `/dev/ttyACM*`  
- **Windows**: `COM*`

## ⚠️ Known Issues

### Critical Issues
- **QR Code Scanning**: Wallet QR codes appear correct but are not scannable by most scanner apps
  - Current implementation: QR Version 3 (29x29) with ISO/IEC 18004 compliance
  - Multiple attempts made: Reed-Solomon coefficients, format information, bit placement
  - **Status**: Needs debugging - likely issue with bit encoding or mask application
  - **Workaround**: Manual address entry required

- **Graphics Corruption**: GIF viewer and Le Space graphics slightly broken
  - **Cause**: Likely due to ESP-IDF 6.0 driver updates
  - **Status**: Needs investigation of display driver changes

### Development Notes
- **Wallet Implementation**: Needs full security audit before production use
  - ✅ Persistent addresses between reboots/reflashing using NVS storage
  - ✅ BIP32-like hierarchical deterministic address generation
  - ✅ Hardware RNG for secure master seed generation
  - ⚠️ Private keys not user-accessible - backup mechanism needed
  - ⚠️ No mnemonic phrase support (planned future enhancement)

## 📱 Hardware Specifications

- **Processor**: ESP32-C3 (32-bit RISC-V @ 160MHz)
- **Memory**: 400KB RAM, 16MB Flash, 4KB eFuse
- **Display**: 240×240px IPS 1.3\" (16-bit RGB565 color)
- **Input**: 4 tactile buttons with standardized mapping
- **Output**: 4x RGB LEDs (WS2812B)
- **Connectivity**: USB-C, Bluetooth Low Energy
- **Form Factor**: Compact handheld design optimized for gaming

## 📚 Documentation

- **[Detailed Flashing Guide](FLASHING.md)**: Advanced flashing methods and troubleshooting
- **[Wallet Implementation](WALLET.md)**: Comprehensive wallet architecture and security details  
- **[Bootloader Planning](BOOTING.md)**: Secure boot roadmap (not yet implemented)
- **[Changelog](CHANGELOG.md)**: Recent development session features and updates

### External Links
- **[Device Hardware](https://github.com/firefly/pixie-device)**: Schematics and PCB design
- **[3D Printed Case](https://github.com/firefly/pixie-case)**: Enclosure design files
- **[ESP-IDF Documentation](https://docs.espressif.com/projects/esp-idf/en/v6.0/)**: ESP32-C3 development framework

## 🔐 Security Features

- **Hardware Attestation**: Secure device identity verification
- **Private Key Isolation**: Cryptographic operations stay on-device  
- **Secure Random Generation**: Hardware TRNG for key generation
- **Persistent Storage**: NVS with wear leveling for wallet data
- **Hierarchical Deterministic**: BIP32-like address derivation from master seed

## 🚀 Recent Updates

### QR Version 3 Implementation (Latest)
- ✅ **QR Version 3**: Upgraded to 29x29 modules for better capacity
- ✅ **ISO/IEC 18004 Compliance**: Full standard implementation
- ✅ **Reed-Solomon Error Correction**: 15 ECC codewords for Version 3-L
- ✅ **Proper Function Patterns**: Finder patterns, timing patterns, format info
- ⚠️ **Scanning Issues**: Still not scannable - needs debugging

### ESP-IDF 6.0 Compatibility
- ✅ **Driver Migration**: Updated to new `esp_driver_*` architecture
- ✅ **Component Dependencies**: Fixed display driver SPI dependencies
- ✅ **Timer Macros**: Replaced deprecated `portTICK_PERIOD_MS`
- ✅ **Build System**: Verified compatibility with ESP-IDF 6.0

### Wallet Persistence
- ✅ **NVS Storage**: Master seed and address index persist across reboots
- ✅ **Deterministic Keys**: Consistent address generation from master seed
- ✅ **Watchdog Management**: Proper task yields during crypto operations
- ✅ **Error Handling**: Graceful handling of storage and crypto failures

