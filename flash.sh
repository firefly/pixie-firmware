#!/usr/bin/env bash
set -euo pipefail

cd "$(dirname "$0")"

usage() {
  cat <<EOF
Usage: $0 [-p PORT] [-b BAUD] [--app-only] [--monitor]

Flashes build/pixie.bin to a connected Firefly Pixie. Auto-detects the
serial port if -p is omitted; first \$ESPPORT then \$1 then a /dev/tty
scan are consulted.

Options:
  -p PORT       serial port (e.g. /dev/tty.usbmodem1101)
  -b BAUD       baud rate (default: 921600 - the ESP32-C3's built-in
                USB-Serial/JTAG handles this fine and is ~2x faster
                than the more conservative 460800)
  --app-only    only write the application image; skip the bootloader
                and partition table. Use this when you have rebuilt
                the app but the bootloader and partition layout
                haven't changed - cuts about a third off the total.
  --monitor     open serial monitor after flashing
  -h, --help    show this help
EOF
}

PORT="${ESPPORT:-}"
BAUD="${ESPBAUD:-921600}"
MONITOR=0
APP_ONLY=0

while [ $# -gt 0 ]; do
  case "$1" in
    -p) PORT="$2"; shift 2 ;;
    -b) BAUD="$2"; shift 2 ;;
    --app-only) APP_ONLY=1; shift ;;
    --monitor) MONITOR=1; shift ;;
    -h|--help) usage; exit 0 ;;
    *) echo "error: unknown argument: $1" >&2; usage >&2; exit 2 ;;
  esac
done

if [ -z "$PORT" ]; then
  for p in /dev/tty.usbmodem* /dev/cu.usbmodem* /dev/ttyACM* /dev/ttyUSB*; do
    if [ -e "$p" ]; then PORT="$p"; break; fi
  done
fi

if [ -z "$PORT" ]; then
  echo "error: no serial port detected" >&2
  echo "  pass -p /dev/your-port or export ESPPORT=/dev/your-port" >&2
  echo "  on macOS try: ls /dev/tty.usbmodem*" >&2
  echo "  on linux try: ls /dev/ttyACM* /dev/ttyUSB*" >&2
  exit 1
fi

if [ ! -f build/pixie.bin ]; then
  echo "error: build/pixie.bin not found - run ./build.sh first" >&2
  exit 1
fi

echo "==> Flashing $PORT @ ${BAUD}"

if [ "$APP_ONLY" = "1" ]; then
  echo "==> Flashing app only (skipping bootloader + partition table)"
  WRITE_ARGS="0x10000 build/pixie.bin"
else
  WRITE_ARGS="0x0     build/bootloader/bootloader.bin \
              0x8000  build/partition_table/partition-table.bin \
              0x10000 build/pixie.bin"
fi

if command -v esptool.py >/dev/null 2>&1; then
  # shellcheck disable=SC2086 # WRITE_ARGS is intentionally word-split
  esptool.py --chip esp32c3 -p "$PORT" -b "$BAUD" \
    --before default_reset --after hard_reset \
    write_flash --flash_mode dio --flash_size 16MB --flash_freq 80m \
    $WRITE_ARGS
elif command -v idf.py >/dev/null 2>&1; then
  if [ "$APP_ONLY" = "1" ]; then
    idf.py -p "$PORT" -b "$BAUD" app-flash
  else
    idf.py -p "$PORT" -b "$BAUD" flash
  fi
else
  echo "error: neither esptool.py nor idf.py found in PATH" >&2
  echo "  install one of:" >&2
  echo "    pip install esptool" >&2
  echo "    or set up ESP-IDF: https://docs.espressif.com/projects/esp-idf/" >&2
  exit 1
fi

echo
echo "==> Flash complete"

if [ "$MONITOR" = "1" ]; then
  echo
  echo "  +------------------------------------------------+"
  echo "  |  Press Ctrl+]  to exit the monitor             |"
  echo "  |  Ctrl+C is forwarded to the device, not caught |"
  echo "  +------------------------------------------------+"
  echo
  if command -v idf.py >/dev/null 2>&1; then
    exec idf.py -p "$PORT" monitor
  elif python3 -c 'import esp_idf_monitor' >/dev/null 2>&1; then
    # Standalone monitor (pip install esp-idf-monitor). Reads pixie.elf
    # and resolves panic PCs to source locations - same UX as idf.py
    # monitor, no ESP-IDF or Docker needed.
    exec python3 -m esp_idf_monitor -p "$PORT" build/pixie.elf
  elif [ "$(uname -s)" = "Linux" ] && command -v docker >/dev/null 2>&1 \
       && docker info >/dev/null 2>&1; then
    # Docker --device passthrough only works on Linux; on macOS Docker
    # Desktop runs in a VM and host /dev/tty paths aren't visible.
    IDF_IMAGE="${IDF_IMAGE:-espressif/idf:v5.5.4}"
    exec docker run --rm -it \
      -v "$PWD":/project -w /project -e HOME=/tmp \
      --device "$PORT" \
      "$IDF_IMAGE" idf.py -p "$PORT" monitor
  elif command -v screen >/dev/null 2>&1; then
    echo "note: 'screen' won't symbolicate panic addresses; install" >&2
    echo "      esp-idf-monitor for that:  pip install esp-idf-monitor" >&2
    echo "note: in screen, exit with Ctrl+A then K (then y)" >&2
    exec screen "$PORT" 115200
  else
    echo "warning: no monitor tool available" >&2
    echo "  install one:  pip install esp-idf-monitor" >&2
  fi
else
  echo "Monitor with: ./flash.sh --monitor   (or)  screen $PORT 115200"
fi
