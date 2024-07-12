Booting
=======

**NOTE**: This is a planning document on ideas for the secure bootloader.
It is not currently implemented.

----

The first-stage bootloader is located in the ROM and boots the
second-stage bootloader.

The second-stage bootloader:
  1. Initializes: @TODO list things
  2. Initialize flash encryption and secure booting
  3. If the DFU key sequence is held down for 5s; boot into factory
  4. If no OTA apps installed; boot into factory
  5. Select the boot partition based on `ota_data`
  6. Load the image into RAM (IRAM + DRAM) and transfer managment to that image


Factory App
-----------

1. If no existing configuration, perform provisioning over BLE
2. Enter DFU mode; searching for a BLE source for Firmware


Provisioning Manager
--------------------

Each provisioning pod has a private key, whose public key is embedded
in the DFU firmware.
The provisioning manager communicates over BLE.

- queryEntropy(sessionPubkey)
  - generate ephemeral key, compute shared secret K
  - return encrpyt(randomBytes(16), K)

- register(clientMac, clientPubkey)
  - assign a serial number S
  - sign(config, 


- device:
  - creates random attestation private key:
    - using local and manager entropy
    - discard if compressed key would not begin with a 0x02
    - write it to flash (encrypted)
    - flush and verify
  - sends:
    - MAC address
    - attestion public key
- provisioning manager:
   - replies:
    - device config (serial number, model number, zeros)
    - sign(`v=1&config=${ hex(config, 8) }&pubkey=${ hex(pubKey, 32) }`)
- device:
  - writes attestation to flash
  - flush and verify
  - write config to block 3 of eFuses

Notes
 - This process (from the devices point of view) is idempotent and
   can be restarted at any time and continued from any step
 - The Provisioner must store the serial number and MAC to ensire
   the same device always receives the same serial number

