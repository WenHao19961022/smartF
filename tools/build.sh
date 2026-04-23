#!/bin/bash
set -e

# 项目根目录
PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

# 构建输出目录
BUILD_DIR="$PROJECT_ROOT/build"
mkdir -p "$BUILD_DIR"

# 清理旧的构建产物
echo "Cleaning old build files..."
rm -rf "$BUILD_DIR"/*

# 检查依赖
echo "Checking dependencies..."
command -v gcc >/dev/null 2>&1 || { echo "gcc not found, please install gcc."; exit 1; }
command -v make >/dev/null 2>&1 || { echo "make not found, please install make."; exit 1; }

# 编译源代码
echo "Building project..."
if [ -f "$PROJECT_ROOT/Makefile" ]; then
    make -C "$PROJECT_ROOT"
elif [ -d "$PROJECT_ROOT/src" ]; then
    SRC_FILES=$(find "$PROJECT_ROOT/src" -name '*.c')
    for src in $SRC_FILES; do
        obj="$BUILD_DIR/$(basename "${src%.c}.o")"
        gcc -c "$src" -o "$obj"
    done
    gcc "$BUILD_DIR"/*.o -o "$BUILD_DIR/smart_fridge"
else
    echo "No Makefile or src directory found. Please add source files."
    exit 1
fi

echo "Build finished. Output in $BUILD_DIR"