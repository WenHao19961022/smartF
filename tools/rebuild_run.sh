#!/bin/bash

# 1. 定义颜色常数（可选，让输出更易读）
GREEN='\033[0;32m'
RED='\033[0;31m'
NC='\033[0m' # 无颜色

echo -e "${GREEN}>>> 开始构建 Smart Fridge 项目...${NC}"

# 2. 检查并进入 build 目录
if [ ! -d "./build" ]; then
    echo "创建 build 目录..."
    mkdir build
fi

cd build || exit

# 3. 清理旧的构建文件
echo "正在清理旧的构建文件..."
rm -rf *

# 4. 运行 CMake
echo "运行 CMake 配置..."
cmake ..
if [ $? -ne 0 ]; then
    echo -e "${RED}CMake 配置失败！${NC}"
    exit 1
fi

# 5. 编译项目
# 使用 -j$(nproc) 可以调用所有 CPU 核心加速编译
echo "正在编译..."
make -j$(nproc)
if [ $? -ne 0 ]; then
    echo -e "${RED}编译失败！${NC}"
    exit 1
fi

# 6. 运行程序
echo -e "${GREEN}>>> 编译成功，启动程序...${NC}"
echo "----------------------------------------"
if [ -f "./bin/smart_fridge_app" ]; then
    ./bin/smart_fridge_app
else
    # 兼容性处理：如果你的可执行文件不在 bin 目录下，而是在 build 根目录
    ../bin/smart_fridge_app || ./smart_fridge_app
fi