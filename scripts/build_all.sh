#!/usr/bin/env sh
# Builds the Android packages for both supported GD versions.
#
#   GEODE_SDK_2081  Geode 5.11.2 checkout (GD 2.2081)
#   GEODE_SDK_2091  Geode 6.0.0 checkout (GD 2.2091)
#   ANDROID_NDK     NDK r29 or newer
set -eu

ROOT=$(cd "$(dirname "$0")/.." && pwd)
NDK=${ANDROID_NDK:?set ANDROID_NDK}
SDK_2081=${GEODE_SDK_2081:?set GEODE_SDK_2081}
SDK_2091=${GEODE_SDK_2091:?set GEODE_SDK_2091}
TOOLCHAIN=$NDK/build/cmake/android.toolchain.cmake

configure() {
    cmake -B "$ROOT/$1" -G Ninja -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_TOOLCHAIN_FILE="$TOOLCHAIN" \
        -DANDROID_ABI="$2" -DANDROID_PLATFORM=android-23
}

build_variant() {
    version=$1
    sdk=$2
    gd=$3
    geode=$4

    cp "$ROOT/mod.json" "$ROOT/mod.json.bak"
    sed -i.bak "s/\"geode\": \"[0-9.]*\"/\"geode\": \"$geode\"/; s/\"2\.20[0-9]*\"/\"$gd\"/g" "$ROOT/mod.json"

    export GEODE_SDK=$sdk
    for abi in arm64-v8a armeabi-v7a; do
        if [ "$abi" = armeabi-v7a ]; then
            suffix=android32
        else
            suffix=android64
        fi
        dir="build-$version-$suffix"
        configure "$dir" "$abi"
        cmake --build "$ROOT/$dir" --parallel
        mkdir -p "$ROOT/dist/$version"
        cp "$ROOT/$dir/openhands.friendship-streaks.geode" \
           "$ROOT/dist/$version/openhands.friendship-streaks-$suffix.geode"
    done

    mv "$ROOT/mod.json.bak" "$ROOT/mod.json"
    rm -f "$ROOT/mod.json.bak.bak"
}

build_variant 2.2081 "$SDK_2081" 2.2081 5.11.2
build_variant 2.2091 "$SDK_2091" 2.2091 6.0.0

echo "Packages written to $ROOT/dist"
