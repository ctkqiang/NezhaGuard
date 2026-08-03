#!/bin/bash
set -euo pipefail
# ============================================================
# NezhaGuard Linux 打包脚本
# 用 Docker Ubuntu 24.04 编译，生成 .deb
# ============================================================

PROJ="$(cd "$(dirname "$0")/.." && pwd)"
ARTIFACTS="$PROJ/artifacts"
mkdir -p "$ARTIFACTS"

IMAGE="nezhaguard-builder:24.04"

echo "=== 1/4 构建 Docker 镜像 ==="
docker build -t "$IMAGE" -f "$PROJ/Dockerfile" "$PROJ"

echo "=== 2/4 容器内编译 ==="
CONTAINER=$(docker run -d --rm "$IMAGE" sleep infinity)
docker exec "$CONTAINER" bash -c "
    cd /build && mkdir -p build && cd build &&
    cmake .. -DCMAKE_BUILD_TYPE=Release &&
    make -j\$(nproc) NezhaGuard
"

echo "=== 3/4 生成 .deb ==="
docker exec "$CONTAINER" bash -c "
    cd /build/build && cpack -G DEB
"
docker cp "$CONTAINER:/build/build/nezhaguard-0.0.1-Linux.deb" "$ARTIFACTS/nezhaguard-0.0.1-amd64.deb" 2>/dev/null || \
docker cp "$CONTAINER:/build/build/nezhaguard_0.0.1_amd64.deb" "$ARTIFACTS/" 2>/dev/null || true

echo "=== 4/4 清理容器 ==="
docker stop "$CONTAINER" >/dev/null 2>&1 || true

echo ""
echo "  Linux 产物:"
ls -la "$ARTIFACTS/"*.deb 2>/dev/null || echo "  (未找到 .deb — 检查容器输出)"
