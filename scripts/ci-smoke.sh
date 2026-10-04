#!/usr/bin/env bash
# Compila o app no desktop e carrega a interface QML sem tela (teste de fumaça).
set -euo pipefail
exec > >(tee build.log) 2>&1

cmake -S . -B build-smoke -G Ninja -DCMAKE_BUILD_TYPE=Release \
  -DARF_BUILD_TESTS=OFF -DCMAKE_PREFIX_PATH="${QT_ROOT_DIR:?}"
cmake --build build-smoke
ARF_SMOKE_TEST=1 QT_QPA_PLATFORM=offscreen QT_QUICK_BACKEND=software \
  ./build-smoke/app/AnimatorRF
echo "Teste de fumaça OK"
