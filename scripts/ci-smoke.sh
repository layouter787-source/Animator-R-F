#!/usr/bin/env bash
# Compila o app no desktop e abre o editor QML sem tela (teste de fumaça).
# Falha se o app não abrir ou se o QML emitir erros de execução.
set -euo pipefail
exec > >(tee build.log) 2>&1

cmake -S . -B build-smoke -G Ninja -DCMAKE_BUILD_TYPE=Release \
  -DARF_BUILD_TESTS=OFF -DCMAKE_PREFIX_PATH="${QT_ROOT_DIR:?}"
cmake --build build-smoke

ARF_SMOKE_TEST=1 QT_QPA_PLATFORM=offscreen QT_QUICK_BACKEND=software \
  ./build-smoke/app/AnimatorRF --smoke 2>&1 | tee smoke-run.log

if grep -Eiq "TypeError|ReferenceError|is not a type|Cannot assign|Unable to assign|is not installed|Cannot read property" smoke-run.log; then
  echo "ERRO: o QML emitiu erros de execução (veja acima)"
  exit 1
fi
echo "Teste de fumaça OK"
