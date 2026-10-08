#pragma once
#include <QByteArray>
#include <QIODevice>
#include <QString>
#include <vector>

// Gravador de ZIP simples (sem compressão): ideal para sequências de PNG, que já são comprimidos.
class ZipWriter {
public:
  explicit ZipWriter(QIODevice* device) : dev_(device) {}
  bool addFile(const QString& name, const QByteArray& data);
  bool finish();

private:
  struct Entry {
    QByteArray name;
    quint32 crc;
    quint32 size;
    quint32 offset;
  };
  bool write(const QByteArray& bytes);

  QIODevice* dev_;
  std::vector<Entry> entries_;
  quint32 offset_ = 0;
};
