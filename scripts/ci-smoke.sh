#!/usr/bin/env bash
# Compila o app no desktop e roda um teste de fumaça sem tela:
# cria um projeto de exemplo (com traços), abre o editor, salva e exporta PNG, GIF, ZIP e MP4;
# depois confere os arquivos.
set -euo pipefail
exec > >(tee build.log) 2>&1

cmake -S . -B build-smoke -G Ninja -DCMAKE_BUILD_TYPE=Release \
  -DARF_BUILD_TESTS=OFF -DCMAKE_PREFIX_PATH="${QT_ROOT_DIR:?}"
cmake --build build-smoke

rm -f /tmp/arf_smoke.png /tmp/arf_smoke.gif /tmp/arf_smoke.zip /tmp/arf_smoke.mp4
ARF_SMOKE_TEST=1 QT_QPA_PLATFORM=offscreen QT_QUICK_BACKEND=software \
  ./build-smoke/app/AnimatorRF --smoke 2>&1 | tee smoke-run.log

if grep -Eiq "TypeError|ReferenceError|is not a type|Cannot assign|Unable to assign|is not installed|Cannot read property|ERRO smoke" smoke-run.log; then
  echo "ERRO: o QML emitiu erros de execução (veja acima)"
  exit 1
fi

python3 - <<'PY'
import zipfile, sys, zlib, struct

def need(cond, msg):
    if not cond:
        print("ERRO:", msg)
        sys.exit(1)

png = open("/tmp/arf_smoke.png", "rb").read()
need(png[:8] == b"\x89PNG\r\n\x1a\n", "PNG inválido")
need(len(png) > 20000, "PNG parece em branco (%d bytes): os traços não foram desenhados" % len(png))

gif = open("/tmp/arf_smoke.gif", "rb").read()
need(gif[:6] == b"GIF89a" and gif[-1:] == b"\x3b", "GIF inválido")
need(gif.count(b"\x21\xf9\x04") == 48, "GIF deveria ter 48 quadros, tem %d" % gif.count(b"\x21\xf9\x04"))

z = zipfile.ZipFile("/tmp/arf_smoke.zip")
need(z.testzip() is None, "ZIP corrompido")
need(len(z.namelist()) == 48, "ZIP deveria ter 48 PNGs, tem %d" % len(z.namelist()))

mp4 = open("/tmp/arf_smoke.mp4", "rb").read()
need(mp4[4:8] == b"ftyp", "MP4 sem cabeçalho ftyp")
need(b"moov" in mp4 and b"mdat" in mp4, "MP4 sem moov/mdat")
need(b"avcC" in mp4, "MP4 sem configuração H.264 (avcC)")
need(len(mp4) > 20000, "MP4 pequeno demais (%d bytes)" % len(mp4))

print("Exportações OK:", len(png), "bytes (PNG),", len(gif), "bytes (GIF),",
      len(z.namelist()), "arquivos (ZIP),", len(mp4), "bytes (MP4)")
PY

# Se o ffprobe existir no ambiente, confere que o vídeo decodifica e tem 48 quadros.
if command -v ffprobe >/dev/null 2>&1; then
  frames="$(ffprobe -v error -count_frames -select_streams v:0 \
            -show_entries stream=nb_read_frames -of csv=p=0 /tmp/arf_smoke.mp4)"
  echo "ffprobe: $frames quadros"
  [ "$frames" = "48" ] || { echo "ERRO: o MP4 deveria ter 48 quadros"; exit 1; }
fi
echo "Teste de fumaça OK"
