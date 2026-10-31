#!/bin/sh
# 校验 C++ 源码符合仓库 .clang-format；CI 与本地共用。
# 用法: tools/check-format.sh            （校验 src tests bench fuzz tools）
#       tools/check-format.sh -i        （就地格式化后再校验）
# clang-format 版本敏感，建议与 CI 一致: pip install "clang-format==20.1.0"
set -e

cd "$(dirname "$0")/.."

clang_format=clang-format
if ! command -v "$clang_format" >/dev/null 2>&1; then
    echo "error: clang-format not found, install via: pip install \"clang-format==20.1.0\"" >&2
    exit 2
fi

if [ "${1:-}" = "-i" ]; then
    find src tests bench fuzz tools \( -name '*.cpp' -o -name '*.hpp' \) -exec "$clang_format" -i {} +
fi

status=0
for file in $(find src tests bench fuzz tools -name '*.cpp' -o -name '*.hpp'); do
    if ! "$clang_format" --dry-run -Werror "$file" >/dev/null 2>&1; then
        echo "not formatted: $file (run: tools/check-format.sh -i)"
        status=1
    fi
done

if [ "$status" -eq 0 ]; then
    echo "clang-format check passed"
fi
exit "$status"
