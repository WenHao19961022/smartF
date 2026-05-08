#!/bin/bash

# 1. 定义颜色常数
GREEN='\033[0;32m'
RED='\033[0;31m'
NC='\033[0m'

# 2. 日志设置
LOG_DIR="./logs"
if [ ! -d "$LOG_DIR" ]; then
    mkdir -p "$LOG_DIR"
fi
# 生成以时间命名的日志文件，例如 build_20240520_1530.log
LOG_FILE="$LOG_DIR/build_$(date +%Y%m%d_%H%M%S).log"

# 定义一个函数，将所有输出重定向到 tee
# 这会让后续所有的 echo 和程序输出同时显示在屏幕并写入日志
exec > >(tee -a "$LOG_FILE") 2>&1

echo -e "${GREEN}>>> 开始构建 Smart Fridge 项目...${NC}"
echo "日志记录于: $LOG_FILE"
echo "----------------------------------------"

# 3. 检查并进入 build 目录
if [ ! -d "./build" ]; then
    echo "创建 build 目录..."
    mkdir build
fi

cd build || exit

# 4. 清理旧的构建文件
echo "正在清理旧的构建文件..."
rm -rf *

# 5. 运行 CMake
echo "运行 CMake 配置..."
cmake ..
if [ $? -ne 0 ]; then
    echo -e "${RED}CMake 配置失败！详情请查看 $LOG_FILE${NC}"
    exit 1
fi

# 6. 编译项目
echo "正在编译..."
make -j$(nproc)
if [ $? -ne 0 ]; then
    echo -e "${RED}编译失败！详情请查看 $LOG_FILE${NC}"
    exit 1
fi

# 7. 运行程序
echo -e "${GREEN}>>> 编译成功，启动程序...${NC}"
echo "----------------------------------------"

# 确定可执行文件路径
TARGET="./bin/smart_fridge_app"
if [ ! -f "$TARGET" ]; then
    if [ -f "../bin/smart_fridge_app" ]; then
        TARGET="../bin/smart_fridge_app"
    else
        TARGET="./smart_fridge_app"
    fi
fi

# 提示用户即将进入 sudo 模式，防止 sudo 提示符被日志缓冲淹没
echo "正在以 root 权限启动程序..."
sudo "$TARGET"

echo -e "${GREEN}>>> 程序已退出。完整日志已保存至: $LOG_FILE${NC}"