#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."

ACTION="${1:-apply}"
echo "=== NezhaGuard K8s ${ACTION} ==="

kubectl "${ACTION}" -k k8s/

if [ "${ACTION}" = "apply" ]; then
    echo "=== 等待 DaemonSet 就绪 ==="
    kubectl rollout status daemonset/nezhaguard -n nezhaguard --timeout=120s
    echo "=== Pods 状态 ==="
    kubectl get pods -n nezhaguard -o wide
fi
