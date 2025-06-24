#include "qr-generator.h"
#include <string.h>
#include <stdio.h>
#include <math.h>
#include <limits.h>
#include "esp_task_wdt.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

// QR Code galois field operations for error correction
static uint8_t gf_mul(uint8_t a, uint8_t b) {
    if (a == 0 || b == 0) return 0;
    static const uint8_t gf_log[256] = {
        0, 0, 1, 25, 2, 50, 26, 198, 3, 223, 51, 238, 27, 104, 199, 75,
        4, 100, 224, 14, 52, 141, 239, 129, 28, 193, 105, 248, 200, 8, 76, 113,
        5, 138, 101, 47, 225, 36, 15, 33, 53, 147, 142, 218, 240, 18, 130, 69,
        29, 181, 194, 125, 106, 39, 249, 185, 201, 154, 9, 120, 77, 228, 114, 166,
        6, 191, 139, 98, 102, 221, 48, 253, 226, 152, 37, 179, 16, 145, 34, 136,
        54, 208, 148, 206, 143, 150, 219, 189, 241, 210, 19, 92, 131, 56, 70, 64,
        30, 66, 182, 163, 195, 72, 126, 110, 107, 58, 40, 84, 250, 133, 186, 61,
        202, 94, 155, 159, 10, 21, 121, 43, 78, 212, 229, 172, 115, 243, 167, 87,
        7, 112, 192, 247, 140, 128, 99, 13, 103, 74, 222, 237, 49, 197, 254, 24,
        227, 165, 153, 119, 38, 184, 180, 124, 17, 68, 146, 217, 35, 32, 137, 46,
        55, 63, 209, 91, 149, 188, 207, 205, 144, 135, 151, 178, 220, 252, 190, 97,
        242, 86, 211, 171, 20, 42, 93, 158, 132, 60, 57, 83, 71, 109, 65, 162,
        31, 45, 67, 216, 183, 123, 164, 118, 196, 23, 73, 236, 127, 12, 111, 246,
        108, 161, 59, 82, 41, 157, 85, 170, 251, 96, 134, 177, 187, 204, 62, 90,
        203, 89, 95, 176, 156, 169, 160, 81, 11, 245, 22, 235, 122, 117, 44, 215,
        79, 174, 213, 233, 230, 231, 173, 232, 116, 214, 244, 234, 168, 80, 88, 175
    };
    static const uint8_t gf_exp[256] = {
        1, 2, 4, 8, 16, 32, 64, 128, 29, 58, 116, 232, 205, 135, 19, 38,
        76, 152, 45, 90, 180, 117, 234, 201, 143, 3, 6, 12, 24, 48, 96, 192,
        157, 39, 78, 156, 37, 74, 148, 53, 106, 212, 181, 119, 238, 193, 159, 35,
        70, 140, 5, 10, 20, 40, 80, 160, 93, 186, 105, 210, 185, 111, 222, 161,
        95, 190, 97, 194, 153, 47, 94, 188, 101, 202, 137, 15, 30, 60, 120, 240,
        253, 231, 211, 187, 107, 214, 177, 127, 254, 225, 223, 163, 91, 182, 113, 226,
        217, 175, 67, 134, 17, 34, 68, 136, 13, 26, 52, 104, 208, 189, 103, 206,
        129, 31, 62, 124, 248, 237, 199, 147, 59, 118, 236, 197, 151, 51, 102, 204,
        133, 23, 46, 92, 184, 109, 218, 169, 79, 158, 33, 66, 132, 21, 42, 84,
        168, 77, 154, 41, 82, 164, 85, 170, 73, 146, 57, 114, 228, 213, 183, 115,
        230, 209, 191, 99, 198, 145, 63, 126, 252, 229, 215, 179, 123, 246, 241, 255,
        227, 219, 171, 75, 150, 49, 98, 196, 149, 55, 110, 220, 165, 87, 174, 65,
        130, 25, 50, 100, 200, 141, 7, 14, 28, 56, 112, 224, 221, 167, 83, 166,
        81, 162, 89, 178, 121, 242, 249, 239, 195, 155, 43, 86, 172, 69, 138, 9,
        18, 36, 72, 144, 61, 122, 244, 245, 247, 243, 251, 235, 203, 139, 11, 22,
        44, 88, 176, 125, 250, 233, 207, 131, 27, 54, 108, 216, 173, 71, 142, 1
    };
    return gf_exp[(gf_log[a] + gf_log[b]) % 255];
}

// Calculate penalty score for mask pattern (ISO/IEC 18004 Section 7.8.3)
static int calculateMaskPenalty(QRCode *qr, int mask) {
    int penalty = 0;
    
    // Rule 1: Adjacent modules in row/column with same color (N1 = 3)
    // Horizontal runs
    for (int row = 0; row < qr->size; row++) {
        int count = 1;
        bool lastColor = qr_getModule(qr, 0, row);
        for (int col = 1; col < qr->size; col++) {
            bool color = qr_getModule(qr, col, row);
            if (color == lastColor) {
                count++;
            } else {
                if (count >= 5) penalty += (count - 2);
                count = 1;
                lastColor = color;
            }
        }
        if (count >= 5) penalty += (count - 2);
    }
    
    // Vertical runs
    for (int col = 0; col < qr->size; col++) {
        int count = 1;
        bool lastColor = qr_getModule(qr, col, 0);
        for (int row = 1; row < qr->size; row++) {
            bool color = qr_getModule(qr, col, row);
            if (color == lastColor) {
                count++;
            } else {
                if (count >= 5) penalty += (count - 2);
                count = 1;
                lastColor = color;
            }
        }
        if (count >= 5) penalty += (count - 2);
    }
    
    // Rule 2: Block of modules in same color (N2 = 3)
    for (int row = 0; row < qr->size - 1; row++) {
        for (int col = 0; col < qr->size - 1; col++) {
            bool color = qr_getModule(qr, col, row);
            if (qr_getModule(qr, col + 1, row) == color &&
                qr_getModule(qr, col, row + 1) == color &&
                qr_getModule(qr, col + 1, row + 1) == color) {
                penalty += 3;
            }
        }
    }
    
    // Rule 3: 1:1:3:1:1 ratio pattern in horizontal/vertical (N3 = 40)
    // Pattern: 10111010000 or 00001011101
    for (int row = 0; row < qr->size; row++) {
        for (int col = 0; col <= qr->size - 11; col++) {
            // Check for 10111010000 pattern
            if (qr_getModule(qr, col, row) &&
                !qr_getModule(qr, col + 1, row) &&
                qr_getModule(qr, col + 2, row) &&
                qr_getModule(qr, col + 3, row) &&
                qr_getModule(qr, col + 4, row) &&
                !qr_getModule(qr, col + 5, row) &&
                qr_getModule(qr, col + 6, row) &&
                !qr_getModule(qr, col + 7, row) &&
                !qr_getModule(qr, col + 8, row) &&
                !qr_getModule(qr, col + 9, row) &&
                !qr_getModule(qr, col + 10, row)) {
                penalty += 40;
            }
            
            // Check for 00001011101 pattern
            if (!qr_getModule(qr, col, row) &&
                !qr_getModule(qr, col + 1, row) &&
                !qr_getModule(qr, col + 2, row) &&
                !qr_getModule(qr, col + 3, row) &&
                qr_getModule(qr, col + 4, row) &&
                !qr_getModule(qr, col + 5, row) &&
                qr_getModule(qr, col + 6, row) &&
                qr_getModule(qr, col + 7, row) &&
                qr_getModule(qr, col + 8, row) &&
                !qr_getModule(qr, col + 9, row) &&
                qr_getModule(qr, col + 10, row)) {
                penalty += 40;
            }
        }
    }
    
    // Vertical patterns
    for (int col = 0; col < qr->size; col++) {
        for (int row = 0; row <= qr->size - 11; row++) {
            // Check for 10111010000 pattern
            if (qr_getModule(qr, col, row) &&
                !qr_getModule(qr, col, row + 1) &&
                qr_getModule(qr, col, row + 2) &&
                qr_getModule(qr, col, row + 3) &&
                qr_getModule(qr, col, row + 4) &&
                !qr_getModule(qr, col, row + 5) &&
                qr_getModule(qr, col, row + 6) &&
                !qr_getModule(qr, col, row + 7) &&
                !qr_getModule(qr, col, row + 8) &&
                !qr_getModule(qr, col, row + 9) &&
                !qr_getModule(qr, col, row + 10)) {
                penalty += 40;
            }
            
            // Check for 00001011101 pattern
            if (!qr_getModule(qr, col, row) &&
                !qr_getModule(qr, col, row + 1) &&
                !qr_getModule(qr, col, row + 2) &&
                !qr_getModule(qr, col, row + 3) &&
                qr_getModule(qr, col, row + 4) &&
                !qr_getModule(qr, col, row + 5) &&
                qr_getModule(qr, col, row + 6) &&
                qr_getModule(qr, col, row + 7) &&
                qr_getModule(qr, col, row + 8) &&
                !qr_getModule(qr, col, row + 9) &&
                qr_getModule(qr, col, row + 10)) {
                penalty += 40;
            }
        }
    }
    
    // Rule 4: Proportion of dark modules (N4 = 10)
    int darkCount = 0;
    int totalModules = qr->size * qr->size;
    for (int i = 0; i < totalModules; i++) {
        if (qr->modules[i]) darkCount++;
    }
    
    int percentage = (darkCount * 100) / totalModules;
    int deviation = (percentage < 50) ? (50 - percentage) : (percentage - 50);
    penalty += (deviation / 5) * 10;
    
    return penalty;
}

// Apply mask pattern to data modules
static bool applyMask(int mask, int row, int col) {
    switch (mask) {
        case 0: return (row + col) % 2 == 0;
        case 1: return row % 2 == 0;
        case 2: return col % 3 == 0;
        case 3: return (row + col) % 3 == 0;
        case 4: return (row / 2 + col / 3) % 2 == 0;
        case 5: return ((row * col) % 2) + ((row * col) % 3) == 0;
        case 6: return (((row * col) % 2) + ((row * col) % 3)) % 2 == 0;
        case 7: return (((row + col) % 2) + ((row * col) % 3)) % 2 == 0;
        default: return false;
    }
}

// Generate Reed-Solomon error correction codewords for QR Version 3-L
static void generateErrorCorrection(uint8_t *data, int dataLen, uint8_t *ecc, int eccLen) {
    // Correct generator polynomial for 15 error correction codewords (Version 3-L)
    // G(x) = (x-α^0)(x-α^1)...(x-α^14) in GF(256) with primitive α=2
    uint8_t generator[] = {
        1, 15, 54, 120, 64, 204, 45, 164, 236, 69, 84, 82, 229, 57, 109, 168
    }; // Correct coefficients for 15 ECC codewords (Version 3-L)
    
    // Initialize ECC array
    memset(ecc, 0, eccLen);
    
    // Polynomial division
    for (int i = 0; i < dataLen; i++) {
        uint8_t feedback = data[i] ^ ecc[0];
        
        // Shift ECC array
        for (int j = 0; j < eccLen - 1; j++) {
            ecc[j] = ecc[j + 1] ^ (feedback ? gf_mul(generator[j], feedback) : 0);
        }
        ecc[eccLen - 1] = feedback ? gf_mul(generator[eccLen - 1], feedback) : 0;
    }
}

static void setModule(QRCode *qr, int x, int y, bool value) {
    if (x >= 0 && x < qr->size && y >= 0 && y < qr->size) {
        qr->modules[y * qr->size + x] = value ? 1 : 0;
    }
}

// Generate format info for QR Version 3-L with given mask pattern (ISO/IEC 18004 Section 7.9)
static uint16_t generateFormatInfo(int mask) {
    // Error correction level L (01) + mask pattern (3 bits) = 5 bits total
    uint16_t data = (0x01 << 3) | (mask & 0x07);
    
    // BCH(15,5) error correction for format information
    // Generator polynomial: x^10 + x^8 + x^5 + x^4 + x^2 + x + 1 = 10100110111 (binary) = 0x537
    uint16_t generator = 0x537;
    
    // Shift data to high-order bits for polynomial division
    uint16_t remainder = data << 10;
    
    // Perform polynomial division
    for (int i = 4; i >= 0; i--) {
        if (remainder & (1 << (i + 10))) {
            remainder ^= (generator << i);
        }
    }
    
    // Combine data and remainder
    uint16_t formatInfo = (data << 10) | remainder;
    
    // Apply masking pattern 101010000010010 (0x5412) for format information
    formatInfo ^= 0x5412;
    
    return formatInfo;
}

// Choose optimal mask pattern by evaluating penalty scores
static int chooseBestMask(QRCode *qr, const uint8_t *fullStream) {
    int bestMask = 0;
    int bestPenalty = INT_MAX;
    
    printf("[qr] Evaluating mask patterns (this may take a few seconds)...\n");
    
    for (int mask = 0; mask < 8; mask++) {
        // Yield every 2 masks to prevent watchdog timeout
        if ((mask % 2) == 0) {
            vTaskDelay(pdMS_TO_TICKS(50));
        }
        
        // Create a copy of the QR code for testing this mask
        QRCode testQr;
        memcpy(&testQr, qr, sizeof(QRCode));
        
        // Apply data with this mask pattern
        int byteIndex = 0;
        int bitIndex = 0;
        bool upward = true;
        
        // Place data with this mask pattern
        for (int colPair = qr->size - 1; colPair > 0; colPair -= 2) {
            if (colPair == 6) colPair--;
            
            for (int i = 0; i < qr->size; i++) {
                int row = upward ? (qr->size - 1 - i) : i;
                
                for (int colOffset = 0; colOffset >= -1; colOffset--) {
                    int col = colPair + colOffset;
                    
                    // Skip function patterns
                    if ((col < 9 && row < 9) ||
                        (col >= qr->size - 8 && row < 9) ||
                        (col < 9 && row >= qr->size - 8) ||
                        (col == 6 || row == 6) ||
                        (col == 8) || (row == 8)) {
                        continue;
                    }
                    
                    // Get data bit
                    bool dataBit = false;
                    if (byteIndex < QR_TOTAL_CODEWORDS) {
                        dataBit = (fullStream[byteIndex] >> (7 - bitIndex)) & 1;
                        bitIndex++;
                        if (bitIndex >= 8) {
                            bitIndex = 0;
                            byteIndex++;
                        }
                    }
                    
                    // Apply this mask pattern
                    bool maskBit = applyMask(mask, row, col);
                    bool finalBit = dataBit ^ maskBit;
                    
                    setModule(&testQr, col, row, finalBit);
                }
            }
            upward = !upward;
        }
        
        // Calculate penalty for this mask
        int penalty = calculateMaskPenalty(&testQr, mask);
        printf("[qr] Mask %d penalty: %d\n", mask, penalty);
        
        if (penalty < bestPenalty) {
            bestPenalty = penalty;
            bestMask = mask;
        }
        
        // Yield after each mask evaluation to prevent system stalling
        vTaskDelay(pdMS_TO_TICKS(25));
        
        // Reset for next iteration
        byteIndex = 0;
        bitIndex = 0;
        upward = true;
    }
    
    printf("[qr] Selected mask %d with penalty %d\n", bestMask, bestPenalty);
    return bestMask;
}

// Large QR code pattern generation for full-screen display
// Creates a QR-like pattern that encodes ETH wallet addresses
// Optimized for 240x240 display with text overlay

static void drawFinderPattern(QRCode *qr, int x, int y) {
    // Draw standard 7x7 finder pattern (ISO/IEC 18004 Section 7.3.2)
    for (int dy = -3; dy <= 3; dy++) {
        for (int dx = -3; dx <= 3; dx++) {
            int px = x + dx;
            int py = y + dy;
            
            // Skip if outside QR bounds
            if (px < 0 || px >= qr->size || py < 0 || py >= qr->size) continue;
            
            // Outer border (7x7) - always black
            if (abs(dx) == 3 || abs(dy) == 3) {
                setModule(qr, px, py, true);
            }
            // Inner square (3x3) - always black  
            else if (abs(dx) <= 1 && abs(dy) <= 1) {
                setModule(qr, px, py, true);
            }
            // White ring between outer and inner
            else {
                setModule(qr, px, py, false);
            }
        }
    }
}

static void drawSeparators(QRCode *qr) {
    // Add white separators around finder patterns (ISO/IEC 18004 Section 7.3.3)
    
    // Top-left separator
    for (int i = 0; i < 8; i++) {
        if (i < qr->size) setModule(qr, i, 7, false);  // Horizontal
        if (i < qr->size) setModule(qr, 7, i, false);  // Vertical
    }
    
    // Top-right separator  
    for (int i = 0; i < 8; i++) {
        if (qr->size - 8 + i >= 0 && qr->size - 8 + i < qr->size) {
            setModule(qr, qr->size - 8 + i, 7, false);  // Horizontal
        }
        if (i < qr->size) {
            setModule(qr, qr->size - 8, i, false);  // Vertical
        }
    }
    
    // Bottom-left separator
    for (int i = 0; i < 8; i++) {
        if (i < qr->size) {
            setModule(qr, i, qr->size - 8, false);  // Horizontal
        }
        if (qr->size - 8 + i >= 0 && qr->size - 8 + i < qr->size) {
            setModule(qr, 7, qr->size - 8 + i, false);  // Vertical
        }
    }
}

static void drawTimingPattern(QRCode *qr) {
    // Standard timing patterns at row/col 6 (ISO/IEC 18004 Section 7.3.5)
    // Alternating black/white modules: black for even positions, white for odd
    
    // Horizontal timing pattern (row 6)
    for (int x = 8; x < qr->size - 8; x++) {
        setModule(qr, x, 6, (x % 2) == 0);
    }
    
    // Vertical timing pattern (column 6)
    for (int y = 8; y < qr->size - 8; y++) {
        setModule(qr, 6, y, (y % 2) == 0);
    }
}

static void drawDarkModule(QRCode *qr) {
    // Dark module for Version 3: always at (4*version + 9, 8) = (21, 8)
    // (ISO/IEC 18004 Section 7.3.6)
    int x = 4 * QR_VERSION + 9;  // 4*3 + 9 = 21
    int y = 8;
    setModule(qr, x, y, true);  // Always black
    printf("[qr] Dark module placed at (%d, %d)\n", x, y);
}

// Check if a module is reserved for function patterns
static bool isReservedModule(QRCode *qr, int col, int row) {
    // Finder patterns (7x7 each) + separators (8x8 each)
    if ((col < 9 && row < 9) ||                           // Top-left
        (col >= qr->size - 8 && row < 9) ||              // Top-right 
        (col < 9 && row >= qr->size - 8)) {              // Bottom-left
        return true;
    }
    
    // Timing patterns (row 6 and column 6)
    if (col == 6 || row == 6) {
        return true;
    }
    
    // Format information areas
    if ((col < 9 && row == 8) ||                         // Horizontal format info
        (col == 8 && row < 9) ||                         // Vertical format info (top-left)
        (col == 8 && row >= qr->size - 7) ||             // Vertical format info (bottom)
        (col >= qr->size - 8 && row == 8)) {             // Horizontal format info (top-right)
        return true;
    }
    
    // Dark module at (4*version + 9, 8) = (21, 8) for Version 3
    if (col == 4 * QR_VERSION + 9 && row == 8) {
        return true;
    }
    
    return false;
}

static void encodeData(QRCode *qr, const char *data) {
    int dataLen = strlen(data);
    printf("[qr] Encoding data: %s (length=%d)\n", data, dataLen);
    
    // Create QR data stream for Version 3-L (53 data + 15 error correction = 68 total)
    uint8_t dataBytes[QR_DATA_CODEWORDS];
    uint8_t eccBytes[QR_ECC_CODEWORDS];
    uint8_t fullStream[QR_TOTAL_CODEWORDS];
    
    memset(dataBytes, 0, sizeof(dataBytes));
    
    // Create proper QR bit stream using explicit bit placement
    // Mode indicator: 0100 (4 bits for Byte mode)  
    // Character count: 42 = 00101010 (8 bits)
    // Total header: 12 bits
    
    int bitPos = 0;
    
    // Write mode indicator: 0100 (Byte mode)
    for (int i = 3; i >= 0; i--) {
        bool bit = (0x4 >> i) & 1;
        int byteIndex = bitPos / 8;
        int bitIndex = 7 - (bitPos % 8);
        
        if (bit) {
            dataBytes[byteIndex] |= (1 << bitIndex);
        }
        bitPos++;
    }
    
    // Write character count: 42 = 00101010 (8 bits)
    for (int i = 7; i >= 0; i--) {
        bool bit = (dataLen >> i) & 1;
        int byteIndex = bitPos / 8;
        int bitIndex = 7 - (bitPos % 8);
        
        if (bit) {
            dataBytes[byteIndex] |= (1 << bitIndex);
        }
        bitPos++;
    }
    
    // Data: Add ETH address characters byte by byte
    for (int i = 0; i < dataLen; i++) {
        uint8_t dataByte = (uint8_t)data[i];
        
        // Write each bit of the data byte
        for (int j = 7; j >= 0; j--) {
            bool bit = (dataByte >> j) & 1;
            int byteIndex = bitPos / 8;
            int bitIndex = 7 - (bitPos % 8);
            
            if (byteIndex < QR_DATA_CODEWORDS && bit) {
                dataBytes[byteIndex] |= (1 << bitIndex);
            }
            bitPos++;
        }
    }
    
    // Add terminator 0000 (4 bits) if we have space
    int currentByteIndex = bitPos / 8;
    if (currentByteIndex < QR_DATA_CODEWORDS) {
        // Only add terminator if we have at least 4 bits of space
        int remainingBits = (QR_DATA_CODEWORDS * 8) - bitPos;
        if (remainingBits >= 4) {
            bitPos += 4; // Skip 4 bits for terminator (already 0 from memset)
        }
    }
    
    // Pad to byte boundary if needed
    int bitsToNextByte = (8 - (bitPos % 8)) % 8;
    bitPos += bitsToNextByte;
    
    // Pad remaining space with 11101100 00010001 pattern
    int padByteIndex = bitPos / 8;
    uint8_t padPattern[] = {0xEC, 0x11};
    int padIndex = 0;
    while (padByteIndex < QR_DATA_CODEWORDS) {
        dataBytes[padByteIndex++] = padPattern[padIndex % 2];
        padIndex++;
    }
    
    // Generate error correction codewords
    generateErrorCorrection(dataBytes, QR_DATA_CODEWORDS, eccBytes, QR_ECC_CODEWORDS);
    
    // Combine data and error correction
    memcpy(fullStream, dataBytes, QR_DATA_CODEWORDS);
    memcpy(fullStream + QR_DATA_CODEWORDS, eccBytes, QR_ECC_CODEWORDS);
    
    printf("[qr] Data encoding: Byte0=0x%02X Byte1=0x%02X Byte2=0x%02X\n", 
           dataBytes[0], dataBytes[1], dataBytes[2]);
    
    // Debug: Print first few bytes of address data to verify encoding
    printf("[qr] Address bytes: ");
    for (int i = 0; i < 8 && i < dataLen; i++) {
        printf("0x%02X('%c') ", (uint8_t)data[i], data[i]);
    }
    printf("...\n");
    
    // Debug: Show bit stream structure
    printf("[qr] Bit structure: Mode(4)=0x%X Count(8)=0x%02X FirstChar(8)=0x%02X\n", 
           0x4, dataLen, (uint8_t)data[0]);
    printf("[qr] Address data: %.*s...\n", dataLen > 10 ? 10 : dataLen, data);
    printf("[qr] Generated error correction, total stream: %d bytes\n", QR_TOTAL_CODEWORDS);
    
    // Choose optimal mask pattern by evaluating all 8 patterns
    int selectedMask = chooseBestMask(qr, fullStream);
    
    // Generate format info for selected mask pattern
    uint16_t formatInfo = generateFormatInfo(selectedMask);
    printf("[qr] Using mask %d, format info: 0x%04x\n", selectedMask, formatInfo);
    
    // Place format information around finder patterns (ISO/IEC 18004 Section 7.9.1)
    // 15-bit format info is placed in specific locations around finder patterns
    
    // Top-left finder pattern area
    // Horizontal strip: bits 0-5 at (8,0) to (8,5), bit 6 at (8,7), bit 7 at (8,8)
    for (int i = 0; i < 6; i++) {
        bool bit = (formatInfo >> i) & 1;
        setModule(qr, 8, i, bit);
    }
    setModule(qr, 8, 7, (formatInfo >> 6) & 1);  // Skip (8,6) - timing pattern
    setModule(qr, 8, 8, (formatInfo >> 7) & 1);
    
    // Vertical strip: bit 8 at (7,8), bits 9-14 at (5,8) down to (0,8)
    setModule(qr, 7, 8, (formatInfo >> 8) & 1);
    for (int i = 0; i < 6; i++) {
        bool bit = (formatInfo >> (14 - i)) & 1;  // bits 14,13,12,11,10,9
        setModule(qr, 5 - i, 8, bit);  // positions (5,8) to (0,8)
    }
    
    // Top-right finder pattern area  
    // Horizontal strip: bits 0-7 at (size-1,8) to (size-8,8)
    for (int i = 0; i < 8; i++) {
        bool bit = (formatInfo >> i) & 1;
        setModule(qr, qr->size - 1 - i, 8, bit);
    }
    
    // Bottom-left finder pattern area
    // Vertical strip: bits 0-6 at (8,size-7) to (8,size-1)  
    for (int i = 0; i < 7; i++) {
        bool bit = (formatInfo >> i) & 1;
        setModule(qr, 8, qr->size - 7 + i, bit);
    }
    
    // Note: Dark module is already placed by drawDarkModule() at (21, 8) for Version 3
    
    // Place data in proper QR zigzag pattern starting from bottom-right
    int byteIndex = 0;
    int bitIndex = 0;
    bool upward = true; // Direction flag for zigzag
    
    // Start from rightmost column pair and work leftward
    for (int colPair = qr->size - 1; colPair > 0; colPair -= 2) {
        // Skip timing column (column 6)
        if (colPair == 6) {
            colPair--;
        }
        
        // Process this column pair (right column first, then left)
        for (int i = 0; i < qr->size; i++) {
            // Calculate row based on direction
            int row = upward ? (qr->size - 1 - i) : i;
            
            // Process right column then left column of the pair
            for (int colOffset = 0; colOffset >= -1; colOffset--) {
                int col = colPair + colOffset;
                
                // Skip if this is a function pattern area (ISO/IEC 18004 Section 7.7.3)
                if (isReservedModule(qr, col, row)) {
                    continue;
                }
                
                // Get data bit
                bool dataBit = false;
                if (byteIndex < QR_TOTAL_CODEWORDS) {
                    dataBit = (fullStream[byteIndex] >> (7 - bitIndex)) & 1;
                    bitIndex++;
                    if (bitIndex >= 8) {
                        bitIndex = 0;
                        byteIndex++;
                    }
                }
                
                // Apply selected mask pattern
                bool mask = applyMask(selectedMask, row, col);
                bool finalBit = dataBit ^ mask;
                
                setModule(qr, col, row, finalBit);
            }
        }
        
        // Flip direction for next column pair
        upward = !upward;
    }
    
    printf("[qr] Placed data with error correction and mask pattern %d, used %d bytes\n", selectedMask, byteIndex);
    printf("[qr] QR Version 3 (29x29) generated successfully\n");
    
    // Debug: Verify QR structure
    printf("[qr] QR Structure: Size=%d, Finder patterns at (3,3), (%d,3), (3,%d)\n", 
           qr->size, qr->size-4, qr->size-4);
    printf("[qr] Timing patterns at row/col 6, Format info around finders\n");
}

bool qr_generate(QRCode *qr, const char *data) {
    if (!qr || !data) {
        return false;
    }
    
    qr->size = QR_SIZE;
    
    // Clear all modules
    memset(qr->modules, 0, QR_MODULES);
    
    // Draw finder patterns at corners (ISO/IEC 18004 Section 7.3.2)
    drawFinderPattern(qr, 3, 3);                    // Top-left
    drawFinderPattern(qr, qr->size - 4, 3);        // Top-right  
    drawFinderPattern(qr, 3, qr->size - 4);        // Bottom-left
    
    // Draw separators around finder patterns
    drawSeparators(qr);
    
    // Draw timing patterns
    drawTimingPattern(qr);
    
    // Draw dark module
    drawDarkModule(qr);
    
    // Encode data
    encodeData(qr, data);
    
    return true;
}

bool qr_getModule(const QRCode *qr, int x, int y) {
    if (!qr || x < 0 || x >= qr->size || y < 0 || y >= qr->size) {
        return false;
    }
    
    return qr->modules[y * qr->size + x] == 1;
}

// Note: Font removed since QR area now has NO text overlay for proper scanning

void qr_renderToDisplay(uint8_t *buffer, uint32_t y0, const char *ethAddress, const QRCode *qr) {
    const uint32_t DISPLAY_WIDTH = 240;
    const uint32_t FRAGMENT_HEIGHT = 24;
    
    // Defensive check to prevent corruption
    if (!buffer || !ethAddress || !qr) {
        return;
    }
    
    // Clear buffer with white background (RGB565: 0xFFFF)
    uint16_t *pixelBuffer = (uint16_t*)buffer;
    for (int i = 0; i < DISPLAY_WIDTH * FRAGMENT_HEIGHT; i++) {
        pixelBuffer[i] = 0xFFFF; // White background
    }
    
    // **FULL PAGE QR**: Maximum size with proper quiet zone for optimal scanning
    const int MODULE_SIZE = 7;  // Larger scaling: each QR module = 7x7 pixels
    const int QUIET_ZONE = 4 * MODULE_SIZE;  // 4 modules = 28 pixels on each side
    
    // Total QR size with quiet zone: (29 + 8) * 7 = 259 pixels (larger than 240px)
    // Scale down slightly to fit: use MODULE_SIZE = 6
    const int ACTUAL_MODULE_SIZE = 6;  // 6x6 pixels per module
    const int ACTUAL_QUIET_ZONE = 4 * ACTUAL_MODULE_SIZE;  // 24 pixels each side
    const int QR_WITH_BORDER = (qr->size + 8) * ACTUAL_MODULE_SIZE;  // (29+8)*6 = 222 pixels
    const int QR_START_X = (DISPLAY_WIDTH - QR_WITH_BORDER) / 2;  // Center horizontally  
    const int QR_START_Y = 10;  // Start near top for full page effect
    
    // Only print address once per QR generation (not per fragment)
    static char last_address[50] = "";
    char safe_address[50];
    strncpy(safe_address, ethAddress, 42);
    safe_address[42] = '\0';
    
    if (strcmp(safe_address, last_address) != 0) {
        printf("[qr] Rendering FULL PAGE QR: %dx%d modules, %dx%d pixels, quiet zone=%d\n", 
               qr->size, qr->size, qr->size * ACTUAL_MODULE_SIZE, qr->size * ACTUAL_MODULE_SIZE, ACTUAL_QUIET_ZONE);
        strncpy(last_address, safe_address, sizeof(last_address) - 1);
        last_address[sizeof(last_address) - 1] = '\0';
    }
    
    // **NO TEXT OVERLAY** - QR code area is completely clean for scanning
    
    // Render QR code with proper quiet zone and integer scaling
    for (int y = 0; y < FRAGMENT_HEIGHT; y++) {
        int display_y = y0 + y;
        
        // Check if we're in the QR area (including quiet zone)
        if (display_y < QR_START_Y || display_y >= QR_START_Y + QR_WITH_BORDER) {
            continue; // Outside QR area - leave white
        }
        
        int qr_area_y = display_y - QR_START_Y;
        
        for (int x = 0; x < DISPLAY_WIDTH; x++) {
            // Check if we're in the QR area horizontally
            if (x < QR_START_X || x >= QR_START_X + QR_WITH_BORDER) {
                continue; // Outside QR area - leave white
            }
            
            int qr_area_x = x - QR_START_X;
            
            bool is_black = false;
            
            // Check if we're in the quiet zone (always white)
            if (qr_area_x < ACTUAL_QUIET_ZONE || qr_area_x >= ACTUAL_QUIET_ZONE + (qr->size * ACTUAL_MODULE_SIZE) ||
                qr_area_y < ACTUAL_QUIET_ZONE || qr_area_y >= ACTUAL_QUIET_ZONE + (qr->size * ACTUAL_MODULE_SIZE)) {
                // In quiet zone - leave white
                is_black = false;
            } else {
                // In actual QR code area - determine module
                int qr_pixel_x = qr_area_x - ACTUAL_QUIET_ZONE;
                int qr_pixel_y = qr_area_y - ACTUAL_QUIET_ZONE;
                
                int qr_module_x = qr_pixel_x / ACTUAL_MODULE_SIZE;
                int qr_module_y = qr_pixel_y / ACTUAL_MODULE_SIZE;
                
                if (qr_module_x >= 0 && qr_module_x < qr->size && 
                    qr_module_y >= 0 && qr_module_y < qr->size) {
                    is_black = qr_getModule(qr, qr_module_x, qr_module_y);
                }
            }
            
            // **PURE BLACK/WHITE PIXELS** - no anti-aliasing
            if (is_black) {
                // Mirror X coordinate to fix display orientation
                int mirrored_x = DISPLAY_WIDTH - 1 - x;
                int pixel_pos = y * DISPLAY_WIDTH + mirrored_x;
                if (pixel_pos >= 0 && pixel_pos < DISPLAY_WIDTH * FRAGMENT_HEIGHT) {
                    pixelBuffer[pixel_pos] = 0x0000; // Pure black in RGB565
                }
            }
            // White pixels already set in buffer initialization
        }
    }
}

// LCD interface for QR rendering - integrates with existing display system
void lcd_fillRect(int x, int y, int w, int h, uint16_t color) {
    // This is a simplified implementation for the QR rendering
    // In a real system, this would interface with the actual LCD hardware
    // For now, we'll use the existing display buffer approach
    printf("[lcd] fillRect(%d,%d,%dx%d) color=0x%04X\n", x, y, w, h, color);
}

void render_qr(const QRCode *qr) {
    printf("[qr] Rendering QR code using render_qr API\n");
    for (int y = 0; y < QR_SIZE; y++) {
        for (int x = 0; x < QR_SIZE; x++) {
            uint16_t color = qr->modules[y * QR_SIZE + x] ? 0x0000 : 0xFFFF;  // black or white
            lcd_fillRect(QR_OFFSET_X + x * QR_SCALE,
                         QR_OFFSET_Y + y * QR_SCALE,
                         QR_SCALE, QR_SCALE,
                         color);
        }
    }
}
