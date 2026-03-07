#!/bin/bash

# WindNerd Core - Compile and Flash Script
# Uses STM32CubeProgrammer (more reliable than OpenOCD for STM32G0)
# Usage: ./compile_flash.sh <path_to_ino_file>

echo "=== WindNerd Core Compile & Flash ==="
echo ""

# Check if .ino file path is provided
if [ $# -eq 0 ]; then
    echo "Usage: $0 <path_to_ino_file>"
    echo "Example: $0 examples/4g-air780e/4g-air780e.ino"
    echo "Example: $0 examples/4g-air780e/4g-test-simple.ino"
    exit 1
fi

INO_FILE="$1"

# Check if file exists
if [ ! -f "$INO_FILE" ]; then
    echo "❌ Error: File '$INO_FILE' not found!"
    exit 1
fi

# Get directory and filename
INO_DIR=$(dirname "$INO_FILE")
INO_NAME=$(basename "$INO_FILE")
BUILD_DIR="$INO_DIR/build"

echo "📁 Source file: $INO_FILE"
echo "📁 Build directory: $BUILD_DIR"
echo ""

# Navigate to the .ino file directory
cd "$INO_DIR" || { echo "❌ Cannot access directory: $INO_DIR"; exit 1; }

echo "1. Compiling firmware for STM32G031F8Px..."
arduino-cli compile --fqbn STMicroelectronics:stm32:GenG0:pnum=GENERIC_G031F8PX --output-dir ./build "$INO_NAME"

if [ $? -eq 0 ]; then
    echo "✅ Compilation successful!"
    echo ""
    
    # Generate binary filename from .ino filename
    BIN_FILE="./build/${INO_NAME}.bin"
    
    echo "2. Flashing firmware using STM32CubeProgrammer..."
    echo "📎 Binary file: $BIN_FILE"
    STM32_Programmer_CLI -c port=SWD -w "$BIN_FILE" 0x08000000 -v -rst
    
    if [ $? -eq 0 ]; then
        echo ""
        echo "✅ Flash and verify successful!"
        echo "✅ Device reset completed!"
        echo ""
        echo "📡 Serial output available on TX2 (USART2) at 115200 baud"
        echo "🔌 Connect TTL adapter: RX->TX2 (Yellow wire), GND->GND"
    else
        echo "❌ Flash failed!"
        exit 1
    fi
else
    echo "❌ Compilation failed!"
    exit 1
fi