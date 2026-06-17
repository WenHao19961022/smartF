#!/bin/bash
set -o pipefail

# 1. 定义颜色常数
GREEN='\033[0;32m'
RED='\033[0;31m'
YELLOW='\033[1;33m'
NC='\033[0m'

PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$PROJECT_ROOT" || exit 1

# 2. 日志设置
LOG_DIR="$PROJECT_ROOT/logs"
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

# 3. 检查 build 目录权限
BUILD_DIR="$PROJECT_ROOT/build"
if [ -e "$BUILD_DIR" ] && [ ! -w "$BUILD_DIR" ]; then
    echo -e "${RED}build 目录不可写，无法清理或重新配置 CMake。${NC}"
    echo -e "${YELLOW}这通常是因为之前用 sudo 执行过构建脚本，导致 build 目录归 root 所有。${NC}"
    echo "请在 Jetson 上执行下面其中一个命令后重试："
    echo "  sudo chown -R \$(id -u):\$(id -g) \"$BUILD_DIR\""
    echo "或："
    echo "  sudo rm -rf \"$BUILD_DIR\""
    exit 1
fi

if [ ! -d "$BUILD_DIR" ]; then
    echo "创建 build 目录..."
    mkdir -p "$BUILD_DIR" || {
        echo -e "${RED}创建 build 目录失败：$BUILD_DIR${NC}"
        exit 1
    }
fi

cd "$BUILD_DIR" || exit 1

# 4. 清理旧的构建文件
echo "正在清理旧的构建文件..."
if ! rm -rf -- ./* ./.??* 2>/dev/null; then
    echo -e "${RED}清理 build 目录失败，目录内可能存在当前用户无权限删除的文件。${NC}"
    echo "请在 Jetson 上执行下面其中一个命令后重试："
    echo "  sudo chown -R \$(id -u):\$(id -g) \"$BUILD_DIR\""
    echo "或："
    echo "  sudo rm -rf \"$BUILD_DIR\""
    exit 1
fi

# 5. 运行 CMake
echo "运行 CMake 配置..."
cmake "$PROJECT_ROOT"
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
