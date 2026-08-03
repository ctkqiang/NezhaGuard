#!/bin/bash
set -euo pipefail
# ============================================================
# NezhaGuard macOS 打包脚本
# 生成 .dmg 和可选 .pkg
# ============================================================

PROJ="$(cd "$(dirname "$0")/.." && pwd)"
BUILD="$PROJ/build-release"
APP="$BUILD/NezhaGuard.app"
DMG="$PROJ/artifacts/NezhaGuard-0.0.1-arm64.dmg"
PKG="$PROJ/artifacts/NezhaGuard-0.0.1.pkg"

mkdir -p "$PROJ/artifacts"

echo "=== 1/4 编译 Release ==="
cmake -S "$PROJ" -B "$BUILD" \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_OSX_ARCHITECTURES=arm64 \
    -DCMAKE_PREFIX_PATH=/opt/homebrew/opt/qt
cmake --build "$BUILD" --target NezhaGuard -j"$(sysctl -n hw.ncpu)"

echo "=== 2/4 macdeployqt ==="
macdeployqt "$APP" -verbose=1

echo "=== 3/4 复制配置文件 ==="
mkdir -p "$APP/Contents/Resources/rules"
mkdir -p "$APP/Contents/Resources/config/notifier"
cp "$PROJ/rules/default.yaml" "$APP/Contents/Resources/rules/"
cp "$PROJ/config/monitor_apps.conf" "$APP/Contents/Resources/config/"
cp "$PROJ/config/database.conf" "$APP/Contents/Resources/config/"
cp "$PROJ/config/notifier/"*.conf "$APP/Contents/Resources/config/notifier/"

echo "=== 4/4 生成 .dmg ==="
hdiutil create -volname "NezhaGuard" -srcfolder "$APP" -ov -format UDZO "$DMG"

echo ""
echo "  macOS 产物:"
echo "    dmg: $DMG"
echo ""
echo "  可选 .pkg: pkgbuild --root $APP --identifier com.nezhaguard.siem --version 0.0.1 $PKG"
