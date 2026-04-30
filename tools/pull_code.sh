#!/bin/bash

# 颜色常数
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

echo -e "${YELLOW}⚠️  警告：这将强制重置本地修改并同步远程仓库...${NC}"

# 1. 将所有文件加入暂存区（确保新文件也能被 reset 掉）
git add .

# 2. 强制重置到当前分支状态，丢弃所有本地改动
echo -e "${BLUE}>>> 正在重置本地修改...${NC}"
git reset --hard

# 3. 拉取远程 main 分支
echo -e "${BLUE}>>> 正在从 origin main 拉取更新...${NC}"
git pull origin main

if [ $? -eq 0 ]; then
    echo -e "\033[0;32m✅ 更新成功！\033[0m"
else
    echo -e "\033[0;31m❌ 更新失败，请检查网络或仓库权限。\033[0m"
    exit 1
fi