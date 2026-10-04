#!/usr/bin/env bash
# Instala o Qt (desktop + Android) com aqtinstall e compila o APK debug.
# Tudo fica neste script (e em ci/config.env), para o workflow nunca precisar mudar.
set -euo pipefail
exec > >(tee build.log) 2>&1

: "${ANDROID_HOME:?}" "${NDK_VERSION:?}" "${QT_VERSION:?}"

python3 -m venv .venv
set +u; . .venv/bin/activate; set -u
pip install -q aqtinstall

QTV="$(python3 -m aqt list-qt linux desktop --spec "$QT_VERSION" --latest-version)"
echo "Qt $QTV"
QT_DIR="$HOME/Qt"

python3 -m aqt install-qt linux desktop "$QTV" linux_gcc_64 -O "$QT_DIR"
# Qt 6.7+ publica o Android no host "all_os"; tenta "linux" como alternativa.
python3 -m aqt install-qt all_os android "$QTV" android_arm64_v8a -O "$QT_DIR" \
  || python3 -m aqt install-qt linux android "$QTV" android_arm64_v8a -O "$QT_DIR"

HOST="$QT_DIR/$QTV/gcc_64"
QT_ANDROID="$QT_DIR/$QTV/android_arm64_v8a"

yes | "$ANDROID_HOME/cmdline-tools/latest/bin/sdkmanager" \
  "ndk;${NDK_VERSION}" "platforms;android-34" "build-tools;34.0.0" >/dev/null || true

"$QT_ANDROID/bin/qt-cmake" -S . -B build-android -G Ninja \
  -DQT_HOST_PATH="$HOST" \
  -DANDROID_SDK_ROOT="$ANDROID_HOME" \
  -DANDROID_NDK_ROOT="$ANDROID_HOME/ndk/${NDK_VERSION}" \
  -DCMAKE_BUILD_TYPE=Debug -DARF_BUILD_TESTS=OFF
cmake --build build-android --target apk
echo "APK gerado"
