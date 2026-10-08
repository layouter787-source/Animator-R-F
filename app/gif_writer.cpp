#include "gif_writer.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <unordered_map>
#include <vector>

namespace {

struct Rgb {
  quint8 r = 0, g = 0, b = 0;
};

inline int channel(quint32 c, int ch) { return ch == 0 ? int((c >> 16) & 255) : ch == 1 ? int((c >> 8) & 255) : int(c & 255); }

// ---- paleta: median cut ----------------------------------------------------

std::vector<Rgb> buildPalette(std::vector<quint32>& px, int maxColors) {
  struct Box {
    size_t b, e;
    int ch;
    int range;
  };
  const auto measure = [&](size_t b, size_t e) {
    int mn[3] = {255, 255, 255}, mx[3] = {0, 0, 0};
    for (size_t i = b; i < e; ++i)
      for (int c = 0; c < 3; ++c) {
        const int v = channel(px[i], c);
        mn[c] = std::min(mn[c], v);
        mx[c] = std::max(mx[c], v);
      }
    int best = 0;
    for (int c = 1; c < 3; ++c)
      if (mx[c] - mn[c] > mx[best] - mn[best]) best = c;
    return Box{b, e, best, mx[best] - mn[best]};
  };

  std::vector<Box> boxes;
  if (!px.empty()) boxes.push_back(measure(0, px.size()));
  while (int(boxes.size()) < maxColors) {
    int pick = -1;
    double bestScore = 0;
    for (int i = 0; i < int(boxes.size()); ++i) {
      const double score = double(boxes[i].range) * double(boxes[i].e - boxes[i].b);
      if (boxes[i].e - boxes[i].b > 1 && boxes[i].range > 0 && score > bestScore) {
        bestScore = score;
        pick = i;
      }
    }
    if (pick < 0) break;
    const Box bx = boxes[pick];
    const int ch = bx.ch;
    std::sort(px.begin() + long(bx.b), px.begin() + long(bx.e),
              [ch](quint32 a, quint32 b) { return channel(a, ch) < channel(b, ch); });
    const size_t mid = bx.b + (bx.e - bx.b) / 2;
    boxes[pick] = measure(bx.b, mid);
    boxes.push_back(measure(mid, bx.e));
  }

  std::vector<Rgb> pal;
  for (const Box& bx : boxes) {
    quint64 sr = 0, sg = 0, sb = 0;
    const size_t n = bx.e - bx.b;
    for (size_t i = bx.b; i < bx.e; ++i) {
      sr += quint64(channel(px[i], 0));
      sg += quint64(channel(px[i], 1));
      sb += quint64(channel(px[i], 2));
    }
    pal.push_back({quint8(sr / n), quint8(sg / n), quint8(sb / n)});
  }
  pal.resize(size_t(maxColors));  // completa com preto, se faltar
  return pal;
}

// ---- LZW -------------------------------------------------------------------

class BitWriter {
public:
  explicit BitWriter(QByteArray& out) : out_(out) {}
  void put(quint32 code, int size) {
    cur_ |= code << nbits_;
    nbits_ += size;
    while (nbits_ >= 8) {
      block_.append(char(cur_ & 255));
      cur_ >>= 8;
      nbits_ -= 8;
      if (block_.size() == 255) flushBlock();
    }
  }
  void finish() {
    if (nbits_ > 0) {
      block_.append(char(cur_ & 255));
      cur_ = 0;
      nbits_ = 0;
    }
    flushBlock();
    out_.append(char(0));  // fim dos sub-blocos
  }

private:
  void flushBlock() {
    if (block_.isEmpty()) return;
    out_.append(char(block_.size()));
    out_.append(block_);
    block_.clear();
  }
  QByteArray& out_;
  QByteArray block_;
  quint32 cur_ = 0;
  int nbits_ = 0;
};

void lzwEncode(const std::vector<quint8>& idx, int minCode, QByteArray& out) {
  const int clear = 1 << minCode, eoi = clear + 1;
  int next = eoi + 1, codeSize = minCode + 1;
  BitWriter bw(out);
  std::unordered_map<quint32, quint16> dict;
  dict.reserve(8192);

  bw.put(quint32(clear), codeSize);
  quint32 prefix = idx[0];
  for (size_t i = 1; i < idx.size(); ++i) {
    const quint32 c = idx[i];
    const quint32 key = (prefix << 8) | c;
    const auto it = dict.find(key);
    if (it != dict.end()) {
      prefix = it->second;
      continue;
    }
    bw.put(prefix, codeSize);
    if (next < 4096) {
      dict[key] = quint16(next++);
      if (next > (1 << codeSize) && codeSize < 12) ++codeSize;
    } else {  // tabela cheia: recomeça
      bw.put(quint32(clear), codeSize);
      dict.clear();
      next = eoi + 1;
      codeSize = minCode + 1;
    }
    prefix = c;
  }
  bw.put(prefix, codeSize);
  if (next < 4096) {
    ++next;
    if (next > (1 << codeSize) && codeSize < 12) ++codeSize;
  }
  bw.put(quint32(eoi), codeSize);
  bw.finish();
}

void put16(QByteArray& out, int v) {
  out.append(char(v & 255));
  out.append(char((v >> 8) & 255));
}

}  // namespace

QByteArray encodeGif(int width, int height, int frameCount, int delayCs,
                     const std::function<QImage(int)>& frameAt) {
  if (width < 1 || height < 1 || frameCount < 1) return {};

  // Passo 1: amostra pixels de até 12 quadros espalhados para montar a paleta.
  std::vector<quint32> samples;
  const int sampled = std::min(frameCount, 12);
  const size_t perFrame = 160000 / size_t(sampled);
  for (int s = 0; s < sampled; ++s) {
    const int f = sampled == 1 ? 0 : s * (frameCount - 1) / (sampled - 1);
    const QImage img = frameAt(f).convertToFormat(QImage::Format_RGB32);
    const size_t total = size_t(img.width()) * size_t(img.height());
    const size_t stride = std::max<size_t>(1, total / perFrame);
    for (size_t i = 0; i < total; i += stride) {
      const quint32 px = reinterpret_cast<const quint32*>(img.constBits())[i];
      samples.push_back(px & 0x00FFFFFF);
    }
  }
  std::vector<Rgb> palette = buildPalette(samples, 256);

  // Cor mais próxima (com cache, pois as animações repetem muitas cores).
  std::unordered_map<quint32, quint8> cache;
  cache.reserve(1 << 16);
  const auto nearest = [&](quint32 rgb) -> quint8 {
    const auto it = cache.find(rgb);
    if (it != cache.end()) return it->second;
    const int r = int((rgb >> 16) & 255), g = int((rgb >> 8) & 255), b = int(rgb & 255);
    int best = 0, bestD = 1 << 30;
    for (int i = 0; i < 256; ++i) {
      const int dr = r - palette[size_t(i)].r, dg = g - palette[size_t(i)].g, db = b - palette[size_t(i)].b;
      const int d = dr * dr * 3 + dg * dg * 4 + db * db * 2;  // peso perceptual aproximado
      if (d < bestD) {
        bestD = d;
        best = i;
        if (d == 0) break;
      }
    }
    cache.emplace(rgb, quint8(best));
    return quint8(best);
  };

  QByteArray out;
  out.append("GIF89a");
  put16(out, width);
  put16(out, height);
  out.append(char(0xF7));  // paleta global de 256 cores
  out.append(char(0));
  out.append(char(0));
  for (const Rgb& c : palette) {
    out.append(char(c.r));
    out.append(char(c.g));
    out.append(char(c.b));
  }
  // Repetir para sempre (extensão NETSCAPE).
  out.append("\x21\xFF\x0B" "NETSCAPE2.0" "\x03\x01\x00\x00\x00", 19);

  // Passo 2: codifica cada quadro.
  std::vector<quint8> idx(size_t(width) * size_t(height));
  for (int f = 0; f < frameCount; ++f) {
    const QImage img = frameAt(f).convertToFormat(QImage::Format_RGB32);
    for (int y = 0; y < height; ++y) {
      const auto* row = reinterpret_cast<const quint32*>(img.constScanLine(y));
      for (int x = 0; x < width; ++x) idx[size_t(y) * size_t(width) + size_t(x)] = nearest(row[x] & 0x00FFFFFF);
    }
    out.append("\x21\xF9\x04\x04", 4);  // controle gráfico: sem transparência
    put16(out, std::max(2, delayCs));
    out.append(char(0));
    out.append(char(0));
    out.append(char(0x2C));  // descritor da imagem
    put16(out, 0);
    put16(out, 0);
    put16(out, width);
    put16(out, height);
    out.append(char(0));
    out.append(char(8));  // tamanho mínimo do código LZW
    lzwEncode(idx, 8, out);
  }
  out.append(char(0x3B));
  return out;
}
