#!/usr/bin/env bash
# 在项目目录用 ESP-IDF venv Python 直接调用 idf.py（绕过 MinGW 检测）

IDF_VENV="/c/Espressif/python_env/idf5.2_py3.11_env/Scripts/python.exe"
IDF_TOOL="/c/Espressif/frameworks/esp-idf-v5.2.1/tools/idf.py"

export PATH="/c/Espressif/tools/cmake/3.24.0/bin:/c/Espressif/tools/ninja/1.11.1:/c/Espressif/tools/xtensa-esp-elf/esp-13.2.0_20230928/xtensa-esp-elf/bin:$PATH"

# Windows 风格 IDF_PATH（idf.py 内部需要）
export IDF_PATH='C:\Espressif\frameworks\esp-idf-v5.2.1'

echo "=== idf.py location: $IDF_TOOL"
echo "=== Python: $IDF_VENV"
echo "=== IDF_PATH: $IDF_PATH"
echo ""

exec "$IDF_VENV" "$IDF_TOOL" "$@"
