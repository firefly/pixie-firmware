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

To get this project and it's submodules:
```sh
git clone --recurse-submodules https://github.com/firefly/pixie-firmware.git
```

If you already cloned the project and forgot `--recurse-submodules`
```sh
git submodule update --init --recursive
```

To get upstream changes from the remote submodules
```sh
git pull --recurse-submodules
```

Use [docker](https://docs.docker.com/engine/install) to build the project:
```sh
docker run --rm -v $PWD:/project -w /project -e HOME=/tmp espressif/idf idf.py build
```

Troubleshooting
---------------

1. If you get `error: implicit declaration of function x; did you mean function y? [-Wimplicit-function-declaration]`, check and update the `firefly-scene` and `firefly-display` submodules in the components folder:

```sh
# check the submodules are from the correct branch
git submodule status

# update the submodules
git submodule update --init --recursive

# pull submodules changes from the remote repositories
git pull --recurse-submodules
```


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
