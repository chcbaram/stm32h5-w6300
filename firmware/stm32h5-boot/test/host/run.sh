#!/bin/sh
# 호스트 유닛 테스트. 하드웨어가 필요 없다.
set -e
cd "$(dirname "$0")"
cmake -S . -B build >/dev/null
cmake --build build -j8 >/dev/null
exec ./build/boot_test "$@"
