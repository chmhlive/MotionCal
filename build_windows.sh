#!/usr/bin/env bash
set -euo pipefail

SRC_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# 允许外部环境配置 wx-config 路径，默认向上级目录查找
WXCONFIG="${WXCONFIG:-$SRC_DIR/../wxWidgets/build-mingw64/install/bin/wx-config}"
BUILD_LOG="/tmp/motion_cal-build.log"

if [[ "${1:-}" == "clean" ]]; then
    DO_CLEAN=1
else
    DO_CLEAN=0
fi

# 如果用户的特定环境存在该路径则加入
if [[ -d "/vhdx/.bin/mingw-w64/usr/bin" ]]; then
    export PATH="/vhdx/.bin/mingw-w64/usr/bin:$PATH"
fi

for tool in x86_64-w64-mingw32-gcc x86_64-w64-mingw32-g++ x86_64-w64-mingw32-windres; do
    if ! command -v "$tool" >/dev/null 2>&1; then
        echo "missing tool: $tool" >&2
        exit 1
    fi
done

if [[ ! -x "$WXCONFIG" ]]; then
    echo "missing wx-config: $WXCONFIG" >&2
    echo "提示: 请通过环境变量 WXCONFIG 指定 wx-config 的准确路径！" >&2
    exit 1
fi

cd "$SRC_DIR"

MAKE_ARGS=(
    OS=WINDOWS
    MINGW_TOOLCHAIN=x86_64-w64-mingw32
    WXCONFIG="$WXCONFIG"
)

if [[ "$DO_CLEAN" == "1" ]]; then
    make "${MAKE_ARGS[@]}" clean
fi

echo "building Windows exe; log: $BUILD_LOG"
if make "${MAKE_ARGS[@]}" >"$BUILD_LOG" 2>&1; then
    file MotionCal.exe
else
    echo "build failed; tail of $BUILD_LOG:" >&2
    tail -n 80 "$BUILD_LOG" >&2 || true
    exit 1
fi
