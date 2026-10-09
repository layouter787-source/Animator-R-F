#include "mp4_export.h"

#include <algorithm>
#include <cstdint>
#include <vector>

#ifdef ARF_WITH_MP4
extern "C" {
#include "arf_mp4.h"
}
#endif

#ifdef ARF_WITH_MP4

namespace {

int writeCallback(int64_t offset, const void* data, size_t size, void* token) {
  auto* dev = static_cast<QIODevice*>(token);
  if (!dev->seek(offset)) return 1;
  return dev->write(static_cast<const char*>(data), qint64(size)) == qint64(size) ? 0 : 1;
}

inline uint8_t clamp8(int v) { return uint8_t(std::clamp(v, 0, 255)); }

// RGB -> YUV 4:2:0 (BT.601, faixa limitada), cortando no centro para W x H.
void toI420(const QImage& img, int ox, int oy, int W, int H, uint8_t* Y, uint8_t* U, uint8_t* V) {
  for (int y = 0; y < H; ++y) {
    const auto* row = reinterpret_cast<const quint32*>(img.constScanLine(oy + y)) + ox;
    for (int x = 0; x < W; ++x) {
      const quint32 c = row[x];
      const int r = int((c >> 16) & 255), g = int((c >> 8) & 255), b = int(c & 255);
      Y[y * W + x] = clamp8(((66 * r + 129 * g + 25 * b + 128) >> 8) + 16);
    }
  }
  for (int y = 0; y < H / 2; ++y) {
    const auto* r0 = reinterpret_cast<const quint32*>(img.constScanLine(oy + 2 * y)) + ox;
    const auto* r1 = reinterpret_cast<const quint32*>(img.constScanLine(oy + 2 * y + 1)) + ox;
    for (int x = 0; x < W / 2; ++x) {
      const quint32 p[4] = {r0[2 * x], r0[2 * x + 1], r1[2 * x], r1[2 * x + 1]};
      int r = 0, g = 0, b = 0;
      for (quint32 c : p) {
        r += int((c >> 16) & 255);
        g += int((c >> 8) & 255);
        b += int(c & 255);
      }
      r /= 4; g /= 4; b /= 4;
      U[y * (W / 2) + x] = clamp8(((-38 * r - 74 * g + 112 * b + 128) >> 8) + 128);
      V[y * (W / 2) + x] = clamp8(((112 * r - 94 * g - 18 * b + 128) >> 8) + 128);
    }
  }
}

}  // namespace

bool mp4Available() { return true; }

bool writeMp4(QIODevice* out, int width, int height, int fps, int frameCount,
              const std::function<QImage(int)>& frameAt, QString* error) {
  const int W = width & ~15, H = height & ~15;
  if (W < 16 || H < 16) {
    *error = QStringLiteral("Projeto pequeno demais para exportar vídeo.");
    return false;
  }
  const int ox = (width - W) / 2, oy = (height - H) / 2;

  ArfMp4* enc = arf_mp4_open(W, H, fps, 26, writeCallback, out);
  if (!enc) {
    *error = QStringLiteral("Não foi possível iniciar o codificador de vídeo.");
    return false;
  }

  std::vector<uint8_t> y(size_t(W) * size_t(H)), u(size_t(W / 2) * size_t(H / 2)), v(u.size());
  bool ok = true;
  for (int i = 0; i < frameCount && ok; ++i) {
    const QImage img = frameAt(i).convertToFormat(QImage::Format_RGB32);
    toI420(img, ox, oy, W, H, y.data(), u.data(), v.data());
    ok = arf_mp4_add_frame(enc, y.data(), u.data(), v.data()) == 0;
  }
  const bool closed = arf_mp4_close(enc) == 0;
  if (!ok || !closed) *error = QStringLiteral("Falha ao codificar o vídeo.");
  return ok && closed;
}

#else  // sem suporte a MP4 neste build

bool mp4Available() { return false; }

bool writeMp4(QIODevice*, int, int, int, int, const std::function<QImage(int)>&, QString* error) {
  *error = QStringLiteral("Exportação de vídeo indisponível nesta versão.");
  return false;
}

#endif
