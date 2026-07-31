#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."

IMAGE="${NEZHA_IMAGE:-nezhaguard:latest}"
echo "=== 构建 NezhaGuard Docker 镜像: ${IMAGE} ==="
docker build -t "${IMAGE}" .
echo "=== 构建完成: ${IMAGE} ==="
