#include "zip_writer.h"

#include <array>

namespace {

quint32 crc32(const QByteArray& data) {
  static const std::array<quint32, 256> table = [] {
    std::array<quint32, 256> t{};
    for (quint32 i = 0; i < 256; ++i) {
      quint32 c = i;
      for (int k = 0; k < 8; ++k) c = (c & 1) ? 0xEDB88320u ^ (c >> 1) : c >> 1;
      t[i] = c;
    }
    return t;
  }();
  quint32 c = 0xFFFFFFFFu;
  for (char byte : data) c = table[(c ^ quint8(byte)) & 0xFF] ^ (c >> 8);
  return c ^ 0xFFFFFFFFu;
}

void u16(QByteArray& b, quint16 v) {
  b.append(char(v & 255));
  b.append(char(v >> 8));
}

void u32(QByteArray& b, quint32 v) {
  u16(b, quint16(v & 0xFFFF));
  u16(b, quint16(v >> 16));
}

constexpr quint16 kUtf8Flag = 0x0800;
constexpr quint16 kDosDate = 0x2821;  // 01/01/2000

}  // namespace

bool ZipWriter::write(const QByteArray& bytes) {
  if (dev_->write(bytes) != bytes.size()) return false;
  offset_ += quint32(bytes.size());
  return true;
}

bool ZipWriter::addFile(const QString& name, const QByteArray& data) {
  Entry e;
  e.name = name.toUtf8();
  e.crc = crc32(data);
  e.size = quint32(data.size());
  e.offset = offset_;

  QByteArray h;
  u32(h, 0x04034b50);
  u16(h, 20);
  u16(h, kUtf8Flag);
  u16(h, 0);  // sem compressão
  u16(h, 0);  // hora
  u16(h, kDosDate);
  u32(h, e.crc);
  u32(h, e.size);
  u32(h, e.size);
  u16(h, quint16(e.name.size()));
  u16(h, 0);
  h.append(e.name);
  if (!write(h) || !write(data)) return false;
  entries_.push_back(e);
  return true;
}

bool ZipWriter::finish() {
  const quint32 cdStart = offset_;
  for (const Entry& e : entries_) {
    QByteArray h;
    u32(h, 0x02014b50);
    u16(h, 20);
    u16(h, 20);
    u16(h, kUtf8Flag);
    u16(h, 0);
    u16(h, 0);
    u16(h, kDosDate);
    u32(h, e.crc);
    u32(h, e.size);
    u32(h, e.size);
    u16(h, quint16(e.name.size()));
    u16(h, 0);  // extra
    u16(h, 0);  // comentário
    u16(h, 0);  // disco
    u16(h, 0);  // atributos internos
    u32(h, 0);  // atributos externos
    u32(h, e.offset);
    h.append(e.name);
    if (!write(h)) return false;
  }
  const quint32 cdSize = offset_ - cdStart;

  QByteArray end;
  u32(end, 0x06054b50);
  u16(end, 0);
  u16(end, 0);
  u16(end, quint16(entries_.size()));
  u16(end, quint16(entries_.size()));
  u32(end, cdSize);
  u32(end, cdStart);
  u16(end, 0);
  return write(end);
}
