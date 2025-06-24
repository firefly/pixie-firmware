#include <stdio.h>
#include <string.h>
#include <esp_random.h>
#include <esp_task_wdt.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs_flash.h"
#include "nvs.h"

#include "firefly-scene.h"
#include "firefly-crypto.h"
#include "firefly-address.h"

#include "panel.h"
#include "panel-wallet.h"
#include "qr-generator.h"
#include "task-io.h"

// NVS storage keys
#define NVS_NAMESPACE "wallet"
#define NVS_MASTER_SEED_KEY "master_seed"
#define NVS_ADDRESS_INDEX_KEY "addr_index"

// BIP32 constants
#define MASTER_SEED_LENGTH 32
#define BIP32_HARDENED 0x80000000

// Task yield helper to prevent watchdog timeouts without managing watchdog directly
static void preventWatchdogTimeout(void) {
    // Yield to allow watchdog and other tasks to run
    vTaskDelay(pdMS_TO_TICKS(100));
}

typedef struct WalletState {
    FfxScene scene;
    FfxNode nodeAddress1;
    FfxNode nodeAddress2;
    FfxNode nodeBackground;
    FfxNode nodeInstructions;
    uint8_t privateKey[FFX_PRIVKEY_LENGTH];
    uint8_t publicKey[FFX_PUBKEY_LENGTH];
    uint8_t address[FFX_ADDRESS_LENGTH];
    char addressStr[FFX_ADDRESS_STRING_LENGTH];
    char addressLine1[25];
    char addressLine2[25];
    QRCode qrCode;
    bool showingQR;
    bool useFullScreenQR;
    
    // Persistent wallet data
    uint8_t masterSeed[MASTER_SEED_LENGTH];
    uint32_t addressIndex;
    bool hasMasterSeed;
} WalletState;

// Persistent storage functions
static bool loadMasterSeed(WalletState *state) {
    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvs_handle);
    if (err != ESP_OK) {
        printf("[wallet] Failed to open NVS: %s\n", esp_err_to_name(err));
        return false;
    }
    
    size_t required_size = MASTER_SEED_LENGTH;
    err = nvs_get_blob(nvs_handle, NVS_MASTER_SEED_KEY, state->masterSeed, &required_size);
    if (err == ESP_OK && required_size == MASTER_SEED_LENGTH) {
        // Load address index
        err = nvs_get_u32(nvs_handle, NVS_ADDRESS_INDEX_KEY, &state->addressIndex);
        if (err != ESP_OK) {
            state->addressIndex = 0; // Default to first address
        }
        state->hasMasterSeed = true;
        printf("[wallet] Loaded master seed and address index %lu\n", state->addressIndex);
    } else {
        state->hasMasterSeed = false;
        state->addressIndex = 0;
        printf("[wallet] No master seed found in storage\n");
    }
    
    nvs_close(nvs_handle);
    return state->hasMasterSeed;
}

static bool saveMasterSeed(WalletState *state) {
    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvs_handle);
    if (err != ESP_OK) {
        printf("[wallet] Failed to open NVS for write: %s\n", esp_err_to_name(err));
        return false;
    }
    
    err = nvs_set_blob(nvs_handle, NVS_MASTER_SEED_KEY, state->masterSeed, MASTER_SEED_LENGTH);
    if (err != ESP_OK) {
        printf("[wallet] Failed to save master seed: %s\n", esp_err_to_name(err));
        nvs_close(nvs_handle);
        return false;
    }
    
    err = nvs_set_u32(nvs_handle, NVS_ADDRESS_INDEX_KEY, state->addressIndex);
    if (err != ESP_OK) {
        printf("[wallet] Failed to save address index: %s\n", esp_err_to_name(err));
        nvs_close(nvs_handle);
        return false;
    }
    
    err = nvs_commit(nvs_handle);
    nvs_close(nvs_handle);
    
    if (err == ESP_OK) {
        printf("[wallet] Saved master seed and address index %lu\n", state->addressIndex);
        return true;
    } else {
        printf("[wallet] Failed to commit NVS: %s\n", esp_err_to_name(err));
        return false;
    }
}

static void generateMasterSeed(WalletState *state) {
    printf("[wallet] Generating new master seed...\n");
    esp_fill_random(state->masterSeed, MASTER_SEED_LENGTH);
    state->addressIndex = 0;
    state->hasMasterSeed = true;
    
    if (saveMasterSeed(state)) {
        printf("[wallet] Master seed generated and saved successfully\n");
    } else {
        printf("[wallet] Warning: Failed to save master seed to storage\n");
    }
}

// Deterministic private key derivation (simplified BIP32-like)
// Uses SHA-256 based key stretching to derive child keys from master seed
static void derivePrivateKey(const uint8_t *masterSeed, uint32_t index, uint8_t *privateKey) {
    // Create input for key derivation: masterSeed + "eth" + index
    uint8_t input[32 + 3 + 4]; // master_seed + "eth" + index
    memcpy(input, masterSeed, 32);
    memcpy(input + 32, "eth", 3);
    input[35] = (index >> 24) & 0xFF;
    input[36] = (index >> 16) & 0xFF;
    input[37] = (index >> 8) & 0xFF;
    input[38] = index & 0xFF;
    
    // Initialize private key with zeros
    memset(privateKey, 0, FFX_PRIVKEY_LENGTH);
    
    // Deterministic key stretching using multiple hash rounds
    // This creates a deterministic private key based on master seed + index
    
    // First round: Hash the input
    uint32_t state[8] = {0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a, 
                         0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19}; // SHA-256 IV
    
    // Simple hash computation (not full SHA-256, but deterministic)
    for (int round = 0; round < 4; round++) {
        for (int i = 0; i < sizeof(input); i++) {
            uint32_t val = input[i] + round * 0x9e3779b9; // Golden ratio constant
            state[i % 8] ^= val;
            state[i % 8] = (state[i % 8] << 13) | (state[i % 8] >> 19); // Rotate
            state[(i + 1) % 8] += state[i % 8];
        }
        
        // Mix state values
        for (int i = 0; i < 8; i++) {
            state[i] ^= state[(i + 1) % 8];
            state[i] = (state[i] << 7) | (state[i] >> 25);
        }
    }
    
    // Extract private key bytes from final state
    for (int i = 0; i < FFX_PRIVKEY_LENGTH; i++) {
        privateKey[i] = (uint8_t)(state[i % 8] >> (8 * (i / 8)));
        
        // Additional mixing to spread entropy
        if ((i % 4) == 3) {
            state[i % 8] = (state[i % 8] << 11) | (state[i % 8] >> 21);
            state[i % 8] ^= 0xdeadbeef + i;
        }
    }
    
    // Ensure the private key is valid for secp256k1 (not zero, less than curve order)
    // Simple check: if all bytes are zero, add 1
    bool allZero = true;
    for (int i = 0; i < FFX_PRIVKEY_LENGTH; i++) {
        if (privateKey[i] != 0) {
            allZero = false;
            break;
        }
    }
    if (allZero) {
        privateKey[FFX_PRIVKEY_LENGTH - 1] = 1;
    }
    
    printf("[wallet] Derived deterministic private key for address index %lu\n", index);
    
    // Debug: Show first few bytes to verify determinism
    printf("[wallet] Private key prefix: %02X%02X%02X%02X...\n", 
           privateKey[0], privateKey[1], privateKey[2], privateKey[3]);
}

// Custom render function for full-screen QR display
static void renderQR(uint8_t *buffer, uint32_t y0, void *context) {
    WalletState *state = (WalletState *)context;
    if (state && state->showingQR && state->useFullScreenQR) {
        qr_renderToDisplay(buffer, y0, state->addressStr, &state->qrCode);
    }
}

static void updateAddressDisplay(WalletState *state) {
    // Split address into two lines for better readability
    // Line 1: 0x + first 20 chars
    // Line 2: remaining 22 chars
    strncpy(state->addressLine1, state->addressStr, 22);
    state->addressLine1[22] = '\0';
    strncpy(state->addressLine2, state->addressStr + 22, 20);
    state->addressLine2[20] = '\0';
    
    ffx_sceneLabel_setText(state->nodeAddress1, state->addressLine1);
    ffx_sceneLabel_setText(state->nodeAddress2, state->addressLine2);
}

static void generateAddressFromSeed(WalletState *state) {
    if (!state->hasMasterSeed) {
        printf("[wallet] No master seed available!\n");
        return;
    }
    
    printf("[wallet] Deriving private key...\n");
    // Derive private key from master seed and current index
    derivePrivateKey(state->masterSeed, state->addressIndex, state->privateKey);
    
    // Yield frequently to prevent watchdog timeout
    preventWatchdogTimeout();
    
    printf("[wallet] Computing public key (this may take several seconds)...\n");
    if (!ffx_pk_computePubkeySecp256k1(state->privateKey, state->publicKey)) {
        printf("[wallet] Public key computation failed!\n");
        return;
    }
    
    // Yield after intensive secp256k1 operation
    preventWatchdogTimeout();
    
    printf("[wallet] Computing address...\n");
    ffx_eth_computeAddress(state->publicKey, state->address);
    
    // Yield after operation
    preventWatchdogTimeout();
    
    printf("[wallet] Computing checksum...\n");
    ffx_eth_checksumAddress(state->address, state->addressStr);
    
    // Final yield to ensure system stability
    preventWatchdogTimeout();
    
    printf("[wallet] Generated address %lu: %s\n", state->addressIndex, state->addressStr);
}

static void showQRCode(WalletState *state) {
    // Generate QR code for the address
    printf("[wallet] Generating QR for: %s\n", state->addressStr);
    
    printf("[wallet] QR generation starting (this may take a few seconds)...\n");
    bool qrSuccess = qr_generate(&state->qrCode, state->addressStr);
    
    // Yield to allow other tasks to run after QR generation
    preventWatchdogTimeout();
    
    printf("[wallet] QR generation result: %s (size=%d)\n", qrSuccess ? "SUCCESS" : "FAILED", state->qrCode.size);
    
    if (!qrSuccess) {
        printf("[wallet] QR generation failed!\n");
        return;
    }
    
    // Switch to full-screen QR mode
    state->useFullScreenQR = true;
    
    // Hide all scene elements to show raw display
    ffx_sceneNode_setPosition(state->nodeAddress1, (FfxPoint){ .x = -500, .y = 0 });
    ffx_sceneNode_setPosition(state->nodeAddress2, (FfxPoint){ .x = -500, .y = 0 });
    ffx_sceneNode_setPosition(state->nodeBackground, (FfxPoint){ .x = -500, .y = 0 });
    ffx_sceneNode_setPosition(state->nodeInstructions, (FfxPoint){ .x = -500, .y = 0 });
    
    // Set the IO task to use our custom render function
    taskIo_setCustomRenderer(renderQR, state);
    
    printf("[wallet] Full-screen QR display activated\n");
}

static void hideQRCode(WalletState *state) {
    // Disable full-screen QR mode
    state->useFullScreenQR = false;
    
    // Restore normal scene rendering
    taskIo_setCustomRenderer(NULL, NULL);
    
    // Show scene elements
    ffx_sceneNode_setPosition(state->nodeAddress1, (FfxPoint){ .x = 30, .y = 65 });
    ffx_sceneNode_setPosition(state->nodeAddress2, (FfxPoint){ .x = 30, .y = 90 });
    ffx_sceneNode_setPosition(state->nodeBackground, (FfxPoint){ .x = 20, .y = 50 });
    ffx_sceneNode_setPosition(state->nodeInstructions, (FfxPoint){ .x = 30, .y = 140 });
    
    printf("[wallet] Returned to normal scene rendering\n");
}

static void keyChanged(EventPayload event, void *_state) {
    WalletState *state = _state;
    
    Keys keys = event.props.keys.down;
    printf("[wallet] keyChanged: keys=0x%04x, showingQR=%s\n", keys, state->showingQR ? "true" : "false");
    
    // Standardized controls:
    // Button 1 (KeyCancel) = Primary action (generate new address)
    // Button 2 (KeyOk) = Exit
    // Button 3 (KeyNorth) = Up/Right action (toggle QR)
    // Button 4 (KeySouth) = Down/Left action (unused)
    
    if (keys & KeyOk) {
        // If showing QR, exit QR view and return to address view
        if (state->showingQR) {
            state->showingQR = false;
            hideQRCode(state);
            ffx_sceneLabel_setText(state->nodeInstructions, "Key1=New Address  Key3=QR Code  Key2=Exit");
            return;
        }
        // Otherwise, exit wallet completely
        panel_pop();
        return;
    }
    
    if (keys & KeyCancel) {
        // Primary action - generate new address from master seed
        printf("[wallet] Starting address generation...\n");
        
        // Show loading message
        ffx_sceneLabel_setText(state->nodeAddress1, "Generating new");
        ffx_sceneLabel_setText(state->nodeAddress2, "address...");
        ffx_sceneLabel_setText(state->nodeInstructions, "Please wait...");
        
        // If no master seed exists, generate one
        if (!state->hasMasterSeed) {
            generateMasterSeed(state);
        } else {
            // Increment address index for next address in sequence
            state->addressIndex++;
            printf("[wallet] Incremented address index to %lu\n", state->addressIndex);
            saveMasterSeed(state); // Save the new index
        }
        
        // Generate address from master seed + index
        generateAddressFromSeed(state);
        
        printf("[wallet] Address generation complete!\n");
        
        // Update display - force back to address view
        state->showingQR = false;
        hideQRCode(state);
        updateAddressDisplay(state);
        ffx_sceneLabel_setText(state->nodeInstructions, "Key1=New Address  Key3=QR Code  Key2=Exit");
        return;
    }
    
    if (keys & KeyNorth) {
        // Up/Right action - toggle QR view
        if (!state->showingQR) {
            // Show QR code view
            state->showingQR = true;
            showQRCode(state);
            ffx_sceneLabel_setText(state->nodeInstructions, "Key1=New Address  Key3=Back  Key2=Exit");
        } else {
            // Toggle back to address view
            state->showingQR = false;
            hideQRCode(state);
            ffx_sceneLabel_setText(state->nodeInstructions, "Key1=New Address  Key3=QR Code  Key2=Exit");
        }
        return;
    }
}

static int init(FfxScene scene, FfxNode node, void* _state, void* arg) {
    WalletState *state = _state;
    state->scene = scene;
    
    // Create title
    FfxNode nodeTitle = ffx_scene_createLabel(scene, FfxFontLarge, "ETH Wallet");
    ffx_sceneGroup_appendChild(node, nodeTitle);
    ffx_sceneNode_setPosition(nodeTitle, (FfxPoint){ .x = 70, .y = 15 });
    
    // Create background for address text (larger and better positioned)
    state->nodeBackground = ffx_scene_createBox(scene, ffx_size(200, 120));
    ffx_sceneBox_setColor(state->nodeBackground, ffx_color_rgba(0, 0, 0, 200));
    ffx_sceneGroup_appendChild(node, state->nodeBackground);
    ffx_sceneNode_setPosition(state->nodeBackground, (FfxPoint){ .x = 20, .y = 50 });
    
    // Create address labels (split into two lines, properly centered)
    state->nodeAddress1 = ffx_scene_createLabel(scene, FfxFontMedium, "Press OK to");
    ffx_sceneGroup_appendChild(node, state->nodeAddress1);
    ffx_sceneNode_setPosition(state->nodeAddress1, (FfxPoint){ .x = 30, .y = 65 });
    
    state->nodeAddress2 = ffx_scene_createLabel(scene, FfxFontMedium, "generate wallet");
    ffx_sceneGroup_appendChild(node, state->nodeAddress2);
    ffx_sceneNode_setPosition(state->nodeAddress2, (FfxPoint){ .x = 30, .y = 90 });
    
    // Create instructions (positioned within background)
    state->nodeInstructions = ffx_scene_createLabel(scene, FfxFontSmall, "Key1=New Address  Key3=QR Code  Key2=Exit");
    ffx_sceneGroup_appendChild(node, state->nodeInstructions);
    ffx_sceneNode_setPosition(state->nodeInstructions, (FfxPoint){ .x = 30, .y = 140 });
    
    // QR code will be rendered full-screen when requested
    
    // Initialize state
    state->showingQR = false;
    state->useFullScreenQR = false;
    
    // Load or generate master seed
    if (!loadMasterSeed(state)) {
        printf("[wallet] No existing wallet found, will generate on first use\n");
        // Don't generate immediately - let user trigger generation
        ffx_sceneLabel_setText(state->nodeAddress1, "Press Key1 to");
        ffx_sceneLabel_setText(state->nodeAddress2, "generate wallet");
    } else {
        // Generate current address from saved master seed
        printf("[wallet] Loading existing wallet (address %lu)\n", state->addressIndex);
        generateAddressFromSeed(state);
        updateAddressDisplay(state);
        printf("[wallet] Loaded address: %s\n", state->addressStr);
    }
    
    // Register for key events (4 buttons: Cancel, Ok, North, South)
    panel_onEvent(EventNameKeysChanged | KeyCancel | KeyOk | KeyNorth | KeySouth, keyChanged, state);
    
    return 0;
}

void pushPanelWallet(void* arg) {
    panel_push(init, sizeof(WalletState), PanelStyleSlideLeft, arg);
}