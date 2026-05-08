

#ifndef __PANELS_H__
#define __PANELS_H__

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */


#include "firefly-cbor.h"
#include "firefly-tx.h"


///////////////////////////////
// Info Panel Extenstions


// See: panel-tx.c

bool appendAddress(void *info, const char* title, FfxDataResult *address,
  FfxDataResult *chainId);
bool appendData(void *info, const char* title, FfxDataResult *data);
bool appendDecimal(void *info, const char *title, uint8_t decimals,
  FfxDataResult *value);
//void appendDevice(void *info, const char* title);
bool appendNetwork(void *info, FfxDataResult *chainId);


///////////////////////////////
// Menu

int pushPanelMenu();


///////////////////////////////
// Simple Game and Toy Panels

typedef enum GameResult {
    GameResultLose       = 0,
    GameResultWin        = 1,
    GameResultQuit       = 0xff
} GameResult;

// See: panel-gifs.c
int pushPanelGifs();

// See: panel-space.c
GameResult pushPanelSpace();


///////////////////////////////
// Cyberdeck Demo Panels

// See: panel-cyber.c
int pushPanelCyber();

// See: panel-life.c
int pushPanelLife();

// See: panel-bytes.c
int pushPanelBytes();

// See: panel-stats.c
int pushPanelStats();


///////////////////////////////
// Wallet Panels

// See: panel-connect.c
int pushPanelConnect();

// See: panel-attest.c
bool pushPanelAttest(FfxCborCursor *attest);

// See: panel-tx.c
bool pushPanelTx(FfxDataResult *tx);



#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __PANELS_H__ */
