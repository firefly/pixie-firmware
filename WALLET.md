# Firefly Pixie Wallet Documentation

## Overview

The Firefly Pixie includes a secure Ethereum wallet with hierarchical deterministic (HD) key generation, persistent storage, and QR code address display.

## Architecture

### Persistent Storage System

The wallet uses ESP32's NVS (Non-Volatile Storage) to securely store:

- **Master Seed**: 32-byte cryptographically secure random seed
- **Address Index**: Current address derivation index
- **Storage Namespace**: `wallet` 

#### Storage Keys:
- `master_seed`: 32-byte master seed blob
- `addr_index`: 32-bit address index counter

### Key Generation Process

#### Master Seed Generation
1. **Secure Random Generation**: Uses `esp_fill_random()` for cryptographically secure randomness
2. **One-Time Creation**: Master seed is generated only once and persisted
3. **32-Byte Entropy**: Provides 256 bits of entropy for key derivation

#### Hierarchical Deterministic (HD) Address Generation

The wallet implements a simplified BIP32-like derivation scheme:

```
Private Key = DeriveKey(MasterSeed, "eth", AddressIndex)
```

**Derivation Process:**
1. Combine master seed + "eth" prefix + address index
2. Apply deterministic hashing with bit rotation
3. Mix with fresh entropy for security
4. Generate 32-byte private key

**Address Path Structure:**
- Address 0: Derived from index 0
- Address 1: Derived from index 1  
- Address N: Derived from index N

### Security Features

#### Cryptographic Components
- **Secp256k1**: Elliptic curve for public key generation
- **Keccak-256**: Ethereum address hashing
- **EIP-55**: Address checksum validation

#### Security Measures
- **Hardware Random Number Generator**: ESP32 TRNG for seed generation
- **Persistent Storage**: NVS with wear leveling and encryption support
- **Deterministic Derivation**: Reproducible address generation from master seed
- **Watchdog Management**: Prevents system lockup during crypto operations

## User Interface

### Wallet Controls
- **Key1 (Cancel)**: Generate new address (increments index)
- **Key2 (Ok)**: Exit wallet
- **Key3 (North)**: Toggle QR code display

### Display Modes

#### Address View
- Shows current Ethereum address (42 characters)
- Split across two lines for readability
- Displays current address index

#### QR Code View  
- Full-screen QR code display
- Includes address text overlay
- QR Version 2 (25x25 modules) supports 42-character addresses
- Optimal mask pattern selection for maximum scannability

## QR Code Implementation

### Technical Specifications
- **QR Version**: 2 (25x25 modules)
- **Error Correction**: Level L (Low) - ~7% data recovery
- **Encoding**: Byte mode for raw address strings
- **Capacity**: 44 bytes total (34 data + 10 error correction)

### Encoding Process
1. **Mode Indicator**: 4 bits (0100 = Byte mode)
2. **Character Count**: 8 bits (42 characters)
3. **Address Data**: 336 bits (42 × 8 bits)
4. **Terminator**: 4 bits (0000)
5. **Padding**: EC pattern (11101100 00010001)
6. **Error Correction**: Reed-Solomon 10 bytes

### Mask Pattern Optimization
- Evaluates all 8 QR mask patterns
- Calculates penalty scores for each pattern
- Selects optimal mask for best scannability
- Applies BCH(15,5) format information encoding

### Display Rendering
- **Scale Factor**: 7x (25 modules × 7 = 175 pixels)
- **Orientation**: X-axis mirrored to match display coordinate system
- **Colors**: Black modules (0x0000), White background (0xFFFF)
- **Text Overlay**: Address displayed above QR code

## Wallet Operations

### First-Time Setup
1. User opens wallet application
2. "Press Key1 to generate wallet" displayed
3. User presses Key1 to create master seed
4. First address (index 0) generated and displayed

### Subsequent Uses
1. Wallet loads existing master seed from NVS
2. Current address restored from saved index
3. Same address displayed until user generates new one

### New Address Generation
1. User presses Key1 (New Address)
2. Address index incremented
3. New private key derived from master seed + new index
4. New Ethereum address generated and displayed
5. Index saved to NVS for persistence

### QR Code Display
1. User presses Key3 (QR Code)
2. System disables watchdog temporarily
3. QR code generated with optimal mask pattern
4. Full-screen QR display activated
5. Custom renderer bypasses normal scene rendering

## Error Handling

### Crypto Operation Failures
- Public key computation errors logged and handled
- Failed operations restore watchdog and exit gracefully
- User feedback provided through display messages

### Storage Failures
- NVS read/write errors logged with specific error codes
- Graceful fallback to temporary operation mode
- User warned about persistence issues

### QR Generation Failures
- Mask evaluation timeout protection
- Failed QR generation logged and displayed
- Return to address view on failure

## Performance Considerations

### Watchdog Management
- Crypto operations temporarily disable task watchdog
- Frequent task yields during intensive computations
- Proper watchdog restoration after operations

### Memory Usage
- Master seed: 32 bytes in RAM
- QR code data: 625 bytes (25×25 modules)
- Address strings: ~100 bytes total
- Total wallet state: ~1KB RAM

### NVS Storage
- Master seed: 32 bytes flash storage
- Address index: 4 bytes flash storage
- Total persistent data: ~40 bytes

## Future Enhancements

### Potential Improvements
1. **Full BIP32 Implementation**: Complete hierarchical deterministic wallet
2. **Mnemonic Support**: BIP39 seed phrase generation and recovery
3. **Multiple Coin Support**: Bitcoin, other cryptocurrency addresses
4. **Address Book**: Store and manage multiple saved addresses
5. **Transaction Signing**: Basic transaction creation and signing
6. **QR Scanner**: Read addresses from external QR codes

### Security Enhancements
1. **Hardware Security Module**: Utilize ESP32 secure boot features
2. **PIN Protection**: User authentication for wallet access
3. **Backup/Restore**: Secure seed phrase backup mechanism
4. **Key Encryption**: Encrypt stored master seed with user PIN

## Development Notes

### Code Structure
- **panel-wallet.c**: Main wallet interface and logic
- **qr-generator.c**: QR code generation and rendering
- **firefly-crypto.h**: Cryptographic function interfaces
- **firefly-address.h**: Ethereum address utilities

### Dependencies
- **NVS Flash**: Persistent storage subsystem
- **ESP Task WDT**: Watchdog management for crypto operations
- **Firefly Crypto**: Secp256k1 and Ethereum address functions
- **Firefly Display**: Custom rendering for QR code display

### Testing Considerations
- Verify address persistence across reboots
- Test QR code scannability with multiple apps
- Validate address derivation consistency
- Check watchdog timeout prevention
- Confirm NVS storage error handling

## Troubleshooting

### Common Issues

**Wallet shows new address every time:**
- Check NVS initialization in main application
- Verify flash partition table includes NVS
- Confirm NVS namespace and key consistency

**QR code not scannable:**
- Verify optimal mask pattern selection
- Check bit encoding accuracy (Mode=0100, Count=00101010)
- Confirm display orientation and mirroring
- Test with multiple QR scanner applications

**System resets during operation:**
- Enable CONFIG_ESP_SYSTEM_USE_FRAME_POINTER for backtraces
- Check watchdog timeout values in configuration
- Verify proper watchdog delete/add sequences
- Monitor crypto operation timing

**Storage failures:**
- Initialize NVS flash in main() before wallet use
- Check available flash space for NVS partition
- Verify partition table includes appropriate NVS size
- Handle NVS errors gracefully in application code