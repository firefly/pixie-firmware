#!/bin/bash

# Firefly Pixie Build Script

set -e

echo "🔨 Building Firefly Pixie Firmware..."
echo "====================================="

docker run --rm -v $PWD:/project -w /project -e HOME=/tmp espressif/idf idf.py build

if [ $? -eq 0 ]; then
    echo ""
    echo "✅ Build successful!"
    echo ""
    echo "📦 Firmware size:"
    ls -lh build/pixie.bin
    echo ""
    echo "⚡ To flash: ./flash.sh [device_path]"
    echo "🔍 To find device: ./find-device.sh"
else
    echo "❌ Build failed"
    exit 1
fi