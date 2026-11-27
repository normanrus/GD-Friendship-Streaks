#!/usr/bin/env sh
# Builds the Android packages for GD 2.2091.
#
#   GEODE_SDK   Geode 6.0.0 checkout (GD 2.2091)
#   ANDROID_NDK NDK r29 or newer
set -eu

ROOT=$(cd "$(dirname "$0")/.." && pwd)
NDK=${ANDROID_NDK:?set ANDROID_NDK}
SDK=${GEODE_SDK:?set GEODE_SDK}
TOOLCHAIN=$NDK/build/cmake/android.toolchain.cmake

export GEODE_SDK=$SDK
for abi in arm64-v8a armeabi-v7a; do
    if [ "$abi" = armeabi-v7a ]; then
        suffix=android32
    else
        suffix=android64
    fi
    dir="build-$suffix"
    cmake -B "$ROOT/$dir" -G Ninja -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_TOOLCHAIN_FILE="$TOOLCHAIN" \
        -DANDROID_ABI="$abi" -DANDROID_PLATFORM=android-23
    cmake --build "$ROOT/$dir" --parallel
    mkdir -p "$ROOT/dist"
    cp "$ROOT/$dir/openhands.friendship-streaks.geode" \
       "$ROOT/dist/openhands.friendship-streaks-$suffix.geode"
done

echo "Packages written to $ROOT/dist"
