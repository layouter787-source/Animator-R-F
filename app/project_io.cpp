#include "project_io.h"

#include <QDataStream>
#include <QFile>
#include <QSaveFile>

namespace projectio {
namespace {

constexpr quint32 kMagic = 0x41524631;  // "ARF1"
constexpr quint32 kVersion = 1;

// Limites para não alocar memória absurda se o arquivo estiver corrompido.
constexpr qint32 kMaxLayers = 128;
constexpr qint32 kMaxKeys = 100000;
constexpr qint32 kMaxStrokes = 1000000;
constexpr qint32 kMaxPoints = 5000000;

}  // namespace

bool save(const QString& path, const arf::Animation& anim, const QString& name) {
  QSaveFile f(path);
  if (!f.open(QIODevice::WriteOnly)) return false;
  QDataStream out(&f);
  out.setVersion(QDataStream::Qt_6_0);
  out.setFloatingPointPrecision(QDataStream::SinglePrecision);

  out << kMagic << kVersion << name;
  out << qint32(anim.width) << qint32(anim.height) << qint32(anim.fps) << qint32(anim.frameCount);
  out << qint32(anim.layers().size());
  for (const arf::Layer& l : anim.layers()) {
    out << QString::fromStdString(l.name) << l.visible << l.locked << l.opacity;
    out << qint32(l.keys.size());
    for (const auto& [frame, drawing] : l.keys) {
      out << qint32(frame) << qint32(drawing.strokes.size());
      for (const arf::Stroke& s : drawing.strokes) {
        out << quint32(s.color) << s.size << quint8(s.brush) << s.hardness << s.opacity
            << s.antialias << s.eraser << QString::fromStdString(s.preset)
            << qint32(s.points.size());
        for (const arf::Point& p : s.points) out << p.x << p.y << p.pressure << p.t;
      }
    }
  }
  if (out.status() != QDataStream::Ok) return false;
  return f.commit();
}

bool load(const QString& path, arf::Animation& anim, QString* name) {
  QFile f(path);
  if (!f.open(QIODevice::ReadOnly)) return false;
  QDataStream in(&f);
  in.setVersion(QDataStream::Qt_6_0);
  in.setFloatingPointPrecision(QDataStream::SinglePrecision);

  quint32 magic = 0, version = 0;
  QString projectName;
  in >> magic >> version >> projectName;
  if (magic != kMagic || version != kVersion) return false;

  qint32 w = 0, h = 0, fps = 0, frames = 0, layerCount = 0;
  in >> w >> h >> fps >> frames >> layerCount;
  if (in.status() != QDataStream::Ok || w < 16 || h < 16 || w > 8192 || h > 8192 || fps < 1 ||
      fps > 240 || frames < 1 || frames > 100000 || layerCount < 1 || layerCount > kMaxLayers)
    return false;

  arf::Animation result;
  result.width = w;
  result.height = h;
  result.fps = fps;
  result.frameCount = frames;

  for (qint32 li = 0; li < layerCount; ++li) {
    QString layerName;
    bool visible = true, locked = false;
    float opacity = 1.0f;
    qint32 keyCount = 0;
    in >> layerName >> visible >> locked >> opacity >> keyCount;
    if (in.status() != QDataStream::Ok || keyCount < 0 || keyCount > kMaxKeys) return false;

    const int idx = result.addLayer(layerName.toStdString());
    arf::Layer* layer = result.layer(idx);
    layer->visible = visible;
    layer->locked = locked;
    layer->opacity = opacity;

    for (qint32 ki = 0; ki < keyCount; ++ki) {
      qint32 frame = 0, strokeCount = 0;
      in >> frame >> strokeCount;
      if (in.status() != QDataStream::Ok || strokeCount < 0 || strokeCount > kMaxStrokes) return false;
      arf::Drawing& drawing = layer->keys[frame];
      drawing.strokes.reserve(size_t(std::min<qint32>(strokeCount, 4096)));
      for (qint32 si = 0; si < strokeCount; ++si) {
        arf::Stroke s;
        quint32 color = 0;
        quint8 brush = 0;
        QString preset;
        qint32 pointCount = 0;
        in >> color >> s.size >> brush >> s.hardness >> s.opacity >> s.antialias >> s.eraser >>
            preset >> pointCount;
        if (in.status() != QDataStream::Ok || pointCount < 0 || pointCount > kMaxPoints) return false;
        s.color = color;
        s.brush = brush <= 2 ? arf::BrushType(brush) : arf::BrushType::Pencil;
        s.preset = preset.toStdString();
        s.points.resize(size_t(pointCount));
        for (arf::Point& p : s.points) in >> p.x >> p.y >> p.pressure >> p.t;
        if (in.status() != QDataStream::Ok) return false;
        drawing.strokes.push_back(std::move(s));
      }
    }
  }

  anim = std::move(result);
  if (name) *name = projectName;
  return true;
}

}  // namespace projectio
