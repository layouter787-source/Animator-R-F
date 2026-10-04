#!/usr/bin/env bash
# Compila o APK (debug, assinado automaticamente) com Qt for Android.
set -euo pipefail
exec > >(tee build.log) 2>&1

: "${QT_ROOT_DIR:?}" "${ANDROID_HOME:?}" "${NDK_VERSION:?}"
HOST="$(cd "$QT_ROOT_DIR/../gcc_64" && pwd)"

yes | "$ANDROID_HOME/cmdline-tools/latest/bin/sdkmanager" \
  "ndk;${NDK_VERSION}" "platforms;android-34" "build-tools;34.0.0" >/dev/null || true

"$QT_ROOT_DIR/bin/qt-cmake" -S . -B build-android -G Ninja \
  -DQT_HOST_PATH="$HOST" \
  -DANDROID_SDK_ROOT="$ANDROID_HOME" \
  -DANDROID_NDK_ROOT="$ANDROID_HOME/ndk/${NDK_VERSION}" \
  -DCMAKE_BUILD_TYPE=Debug -DARF_BUILD_TESTS=OFF
cmake --build build-android --target apk
echo "APK gerado"
