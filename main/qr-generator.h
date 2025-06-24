#ifndef __QR_GENERATOR_H__
#define __QR_GENERATOR_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>

#define QR_VERSION 3
#define QR_SIZE 29                              // Version 3 = 29x29
#define QR_MODULES (QR_SIZE * QR_SIZE)
#define QR_DATA_CODEWORDS 53
#define QR_ECC_CODEWORDS 15
#define QR_TOTAL_CODEWORDS (QR_DATA_CODEWORDS + QR_ECC_CODEWORDS)

#define QR_SCALE 6                              // 6px per module
#define QR_OFFSET_X ((240 - QR_SIZE * QR_SCALE) / 2)
#define QR_OFFSET_Y ((240 - QR_SIZE * QR_SCALE) / 2)

// QR Code structure
typedef struct {
    uint8_t modules[QR_MODULES];                // 1 = black, 0 = white
    int size;
} QRCode;

// API
bool qr_generate(QRCode *qr, const char *text);
bool qr_getModule(const QRCode *qr, int x, int y);
void qr_renderToDisplay(uint8_t *buffer, uint32_t y0, const char *ethAddress, const QRCode *qr);
void render_qr(const QRCode *qr);               // Alternative render API (for testing)

#ifdef __cplusplus
}
#endif

#endif  // __QR_GENERATOR_H__
