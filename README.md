Firefly Pixie: Firmware
=======================

This is early prototype firmware to help design and refine the
Firefly SDK.

It currently amounts to little more than a clone of Space Invaders,
but has many of the necessary API to implement a hardware wallet.

- [Device](https://github.com/firefly/pixie-device) Design, Schematics and PCB
- [Case](https://github.com/firefly/pixie-case)

Development
-----------

1. Install the Docker engine
   See the [Docker Installation Guide](https://docs.docker.com/engine/install) to download and install the docker engine

2. Build the firmware
   ```sh
     git clone https://github.com/firefly/pixie-firmware.git
     git checkout v0.0.1
     cd pixie-firmware
     docker run --rm -v $PWD:/project -w /project -u $UID -e HOME=/tmp espressif/idf idf.py build
   ```
   * At the end of the build, there will be a new "build" folder created

3. Flash the firmware to the device
    - Connect the device to your computer
    - Goto this site: https://espressif.github.io/esptool-js/
    - Click the "Connect" button under the "Program" section (Baudrate = 921600)
    - Select the serial port for your device on the pop up dialog
    - Add the following Flash Address/File:
        ```sh
           0x0:     build/bootloader/bootloader.bin
           0x8000:  build/partition_table/partition-table.bin
           0x10000: build/pixie.bin
        ```
    - Click the "Program" button to flash the firmware
    - When the log show "Leaving...", it's done flashing

4. Reset the device
    - On https://espressif.github.io/esptool-js/, disconnect from the Program section
    - Click the "Start" button under the "Console" section (Baudrate = 115200)
    - Select the serial port for your device
    - Click the "Reset" button if a blank screen is displayed
    - Should see the "Hello World" printed on the log

Hardware Specifications
-----------------------

- **Processor:** ESP32-C3 (32-bit RISC-V)
- **Speed:** 160Mhz
- **Memory:** 400kb RAM, 16Mb Flash, 4kb eFuse
- **Inputs:** 4x tactile buttons
- **Outputs:**
  - 240x240px IPS 1.3" display (16-bit color)
  - 4x RGB LED (WS2812B)
- **Conectivity:**
  - USB-C
  - BLE


License
-------

BSD License.
