#!/usr/bin/env bash
set -euo pipefail

cd "$(dirname "$0")"

if ! command -v docker >/dev/null 2>&1; then
  echo "error: docker not found in PATH" >&2
  echo "  install Docker Desktop: https://docs.docker.com/get-docker" >&2
  exit 1
fi

if ! docker info >/dev/null 2>&1; then
  echo "error: docker daemon is not running" >&2
  echo "  start Docker Desktop and try again" >&2
  exit 1
fi

if [ ! -f components/firefly-hollows/include/firefly-hollows.h ]; then
  echo "==> Initializing submodules"
  git submodule update --init --recursive
fi

# Workaround for a bug in the pinned firefly-hollows commit (2fbda9d):
# src/task-ble.c references TaskBleInit (defined in src/hollows.h) without
# including it, so the build fails. Upstream fixed this in commit 1f55b89,
# but that commit also reshuffles the FfxKey bit assignments, which is too
# risky to inherit without also updating downstream code. Patch in place;
# idempotent so re-runs are safe. Real fix: open PR against
# firefly/component-hollows to land just the include change.
hollows_ble="components/firefly-hollows/src/task-ble.c"
if [ -f "$hollows_ble" ] && ! grep -q '^#include "hollows.h"' "$hollows_ble"; then
  echo "==> patching $hollows_ble (add missing #include \"hollows.h\")"
  awk '
    /^#include "firefly-tx.h"/ && !patched {
      print
      print ""
      print "// Patched by build.sh: pinned commit is missing this include"
      print "#include \"hollows.h\""
      patched = 1
      next
    }
    { print }
  ' "$hollows_ble" > "$hollows_ble.tmp" && mv "$hollows_ble.tmp" "$hollows_ble"
fi

# Same root cause - hollows.c:132 has a malformed initializer; the
# `.version = version` line is missing a trailing comma, so the next
# `.ready` field is parsed as a member access on `version`.
hollows_c="components/firefly-hollows/src/hollows.c"
if [ -f "$hollows_c" ] && grep -q '^[[:space:]]*\.version = version$' "$hollows_c"; then
  echo "==> patching $hollows_c (add missing comma in TaskBleInit init)"
  sed -i.bak 's/^\([[:space:]]*\)\.version = version$/\1.version = version,/' \
    "$hollows_c" && rm -f "$hollows_c.bak"
fi

# IDF v5.5's NimBLE has an internally-inconsistent BLE_HS_DEBUG config:
# the BLE_HS_DBG_ASSERT macro is active and asserts ble_hs_locked_by_cur_task,
# but the lock-bookkeeping in ble_hs_lock_nested that would set the owning
# task handle never runs in this code path, so the assert always fails
# right after the BLE host task starts. The cyberdeck demos don't need
# BLE; bypass the host init by exiting taskBleFunc right after it signals
# ready. Wallet panel won't work, but it's broken on this commit anyway.
if [ -f "$hollows_ble" ] && \
   ! grep -q 'PIXIE-PATCH: skip-ble-init' "$hollows_ble" && \
   grep -q '^    xSemaphoreGive(init->ready);$' "$hollows_ble"; then
  echo "==> patching $hollows_ble (skip BLE host init for demo build)"
  awk '
    /^    xSemaphoreGive\(init->ready\);$/ && !patched {
      print $0
      print ""
      print "    // PIXIE-PATCH: skip-ble-init - bypass NimBLE host init,"
      print "    // which trips an internally-inconsistent BLE_HS_DEBUG"
      print "    // assert in ble_hs_id_addr on IDF v5.5."
      print "    vTaskDelete(NULL);"
      patched = 1
      next
    }
    { print }
  ' "$hollows_ble" > "$hollows_ble.tmp" && mv "$hollows_ble.tmp" "$hollows_ble"
fi

# Pin the IDF image - `espressif/idf:latest` (6.x) fails to bootstrap
# on this project. v5.5.x is the most recent line known to build cleanly.
# Override with IDF_IMAGE if you know better.
IDF_IMAGE="${IDF_IMAGE:-espressif/idf:v5.5.4}"

# Switching IDF major.minor versions reuses incompatible cmake cache
# (different python_env path). Wipe build/ when the image changes.
if [ -d build ] && [ -f build/.idf-image ]; then
  cached=$(cat build/.idf-image)
  if [ "$cached" != "$IDF_IMAGE" ]; then
    echo "==> IDF image changed ($cached -> $IDF_IMAGE); cleaning build/"
    rm -rf build
  fi
fi

echo "==> Building Pixie firmware ($IDF_IMAGE)"
docker run --rm \
  -v "$PWD":/project \
  -w /project \
  -e HOME=/tmp \
  "$IDF_IMAGE" idf.py build

mkdir -p build && echo "$IDF_IMAGE" > build/.idf-image

echo
echo "==> Build complete"
if [ -f build/pixie.bin ]; then
  ls -lh build/pixie.bin
fi
echo
echo "Flash with:  ./flash.sh"
