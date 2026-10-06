#include "mypaint_engine.h"

#include <QDirIterator>
#include <QFile>
#include <algorithm>
#include <cmath>

#ifdef ARF_WITH_MYPAINT
extern "C" {
#include <mypaint-brush.h>
#include <mypaint-surface.h>
}
#endif

namespace mp {

#ifdef ARF_WITH_MYPAINT

namespace {

QByteArray readPreset(const QString& id) {
  QFile f(":/brushes/" + id + ".myb");
  if (!f.open(QIODevice::ReadOnly)) return {};
  return f.readAll();
}

// ---- superfície: o libmypaint pede "gotas" (dabs) e nós pintamos no QImage ----

struct QtSurface {
  MyPaintSurface base;  // precisa ser o primeiro membro
  QImage* img = nullptr;
};

QtSurface* asQt(MyPaintSurface* s) { return reinterpret_cast<QtSurface*>(s); }

// Opacidade da gota em função da distância (rr = distância² normalizada), como no MyPaint.
inline float dabOpacity(float rr, float hardness) {
  if (rr > 1.0f) return 0.0f;
  const float h = std::clamp(hardness, 0.0001f, 0.9999f);
  return rr <= h ? 1.0f - rr * (1.0f / h - 1.0f) : h / (1.0f - h) * (1.0f - rr);
}

int drawDab(MyPaintSurface* surface, float x, float y, float radius, float cr, float cg, float cb,
            float opaque, float hardness, float alphaEraser, float aspect, float angle,
            float lockAlpha, float /*colorize*/) {
  QImage& img = *asQt(surface)->img;
  if (radius < 0.1f) return 0;
  const float fringe = radius + 1.0f;
  const int x0 = std::max(0, int(std::floor(x - fringe)));
  const int x1 = std::min(img.width() - 1, int(std::ceil(x + fringe)));
  const int y0 = std::max(0, int(std::floor(y - fringe)));
  const int y1 = std::min(img.height() - 1, int(std::ceil(y + fringe)));
  if (x0 > x1 || y0 > y1) return 0;

  const float ang = angle / 360.0f * 2.0f * float(M_PI);
  const float sn = std::sin(ang), cs = std::cos(ang);
  const float invR2 = 1.0f / (radius * radius);
  const float asp = std::max(aspect, 1.0f);

  for (int py = y0; py <= y1; ++py) {
    auto* row = reinterpret_cast<quint32*>(img.scanLine(py));
    for (int px = x0; px <= x1; ++px) {
      const float yy = py + 0.5f - y, xx = px + 0.5f - x;
      const float yyr = (yy * cs - xx * sn) * asp;
      const float xxr = yy * sn + xx * cs;
      const float rr = (yyr * yyr + xxr * xxr) * invR2;
      const float opa = dabOpacity(rr, hardness);
      if (opa <= 0.0f) continue;
      const float a = std::clamp(opa * opaque, 0.0f, 1.0f);

      const quint32 d = row[px];  // premultiplicado
      const float dA = qAlpha(d) / 255.0f, dR = qRed(d) / 255.0f, dG = qGreen(d) / 255.0f,
                  dB = qBlue(d) / 255.0f;
      float nA, nR, nG, nB;
      if (lockAlpha > 0.5f) {  // só muda a cor, mantém a transparência
        if (dA <= 0.0f) continue;
        nA = dA;
        nR = (a * cr + (1 - a) * (dR / dA)) * dA;
        nG = (a * cg + (1 - a) * (dG / dA)) * dA;
        nB = (a * cb + (1 - a) * (dB / dA)) * dA;
      } else {  // alphaEraser: 1 = pinta, 0 = apaga
        nA = a * alphaEraser + (1 - a) * dA;
        nR = a * cr * alphaEraser + (1 - a) * dR;
        nG = a * cg * alphaEraser + (1 - a) * dG;
        nB = a * cb * alphaEraser + (1 - a) * dB;
      }
      const int iA = std::clamp(int(nA * 255.0f + 0.5f), 0, 255);
      row[px] = qRgba(std::min(iA, std::clamp(int(nR * 255.0f + 0.5f), 0, 255)),
                      std::min(iA, std::clamp(int(nG * 255.0f + 0.5f), 0, 255)),
                      std::min(iA, std::clamp(int(nB * 255.0f + 0.5f), 0, 255)), iA);
    }
  }
  return 1;
}

// Cor média sob a gota (para borrar/misturar).
void getColor(MyPaintSurface* surface, float x, float y, float radius, float* r, float* g,
              float* b, float* a) {
  const QImage& img = *asQt(surface)->img;
  const float fringe = std::max(radius, 1.0f) + 1.0f;
  const int x0 = std::max(0, int(std::floor(x - fringe)));
  const int x1 = std::min(img.width() - 1, int(std::ceil(x + fringe)));
  const int y0 = std::max(0, int(std::floor(y - fringe)));
  const int y1 = std::min(img.height() - 1, int(std::ceil(y + fringe)));
  const float invR2 = 1.0f / (std::max(radius, 1.0f) * std::max(radius, 1.0f));

  float sumW = 0, sumA = 0, sumR = 0, sumG = 0, sumB = 0;
  for (int py = y0; py <= y1; ++py) {
    const auto* row = reinterpret_cast<const quint32*>(img.constScanLine(py));
    for (int px = x0; px <= x1; ++px) {
      const float yy = py + 0.5f - y, xx = px + 0.5f - x;
      const float w = dabOpacity((yy * yy + xx * xx) * invR2, 0.5f);
      if (w <= 0.0f) continue;
      const quint32 d = row[px];
      sumW += w;
      sumA += w * qAlpha(d) / 255.0f;
      sumR += w * qRed(d) / 255.0f;
      sumG += w * qGreen(d) / 255.0f;
      sumB += w * qBlue(d) / 255.0f;
    }
  }
  if (sumW <= 1e-6f || sumA <= 1e-6f) {
    *r = *g = *b = *a = 0.0f;
    return;
  }
  *a = std::clamp(sumA / sumW, 0.0f, 1.0f);
  *r = std::clamp(sumR / sumA, 0.0f, 1.0f);
  *g = std::clamp(sumG / sumA, 0.0f, 1.0f);
  *b = std::clamp(sumB / sumA, 0.0f, 1.0f);
}

void beginAtomic(MyPaintSurface*) {}
void endAtomic(MyPaintSurface*, MyPaintRectangle*) {}
void destroySurface(MyPaintSurface*) {}

void initSurface(QtSurface& s, QImage* img) {
  mypaint_surface_init(&s.base);
  s.base.draw_dab = drawDab;
  s.base.get_color = getColor;
  s.base.begin_atomic = beginAtomic;
  s.base.end_atomic = endAtomic;
  s.base.destroy = destroySurface;
  s.base.save_png = nullptr;
  s.img = img;
}

// ---- pincel ----

MyPaintBrush* makeBrush(const arf::Stroke& s) {
  const QByteArray json = readPreset(QString::fromStdString(s.preset));
  if (json.isEmpty()) return nullptr;
  MyPaintBrush* b = mypaint_brush_new();
  if (!mypaint_brush_from_string(b, json.constData())) {
    mypaint_brush_unref(b);
    return nullptr;
  }
  // Cor, tamanho e opacidade vêm dos controles do app; o resto é do pincel.
  const QColor c = QColor::fromRgba(s.color);
  float h = 0, sat = 0, v = 0;
  c.getHsvF(&h, &sat, &v);
  if (h < 0) h = 0;
  mypaint_brush_set_base_value(b, MYPAINT_BRUSH_SETTING_COLOR_H, h);
  mypaint_brush_set_base_value(b, MYPAINT_BRUSH_SETTING_COLOR_S, sat);
  mypaint_brush_set_base_value(b, MYPAINT_BRUSH_SETTING_COLOR_V, v);
  mypaint_brush_set_base_value(b, MYPAINT_BRUSH_SETTING_RADIUS_LOGARITHMIC,
                               std::log(std::max(0.5f, s.size * 0.5f)));
  mypaint_brush_set_base_value(
      b, MYPAINT_BRUSH_SETTING_OPAQUE,
      mypaint_brush_get_base_value(b, MYPAINT_BRUSH_SETTING_OPAQUE) * s.opacity);
  mypaint_brush_reset(b);
  mypaint_brush_new_stroke(b);
  return b;
}

// Dedo e mouse não têm pressão: usa um valor médio, como o MyPaint faz.
inline float effectivePressure(float p) { return p >= 0.999f ? 0.6f : p; }

class LiveImpl : public Live {
public:
  LiveImpl(MyPaintBrush* brush, QImage* target, const arf::Point& first)
      : brush_(brush), prevT_(first.t) {
    initSurface(surface_, target);
    // Move a ponta para o ponto inicial sem pintar.
    mypaint_brush_stroke_to(brush_, &surface_.base, first.x, first.y, 0.0f, 0.0f, 0.0f, 1.0);
  }
  ~LiveImpl() override { mypaint_brush_unref(brush_); }

  void add(const arf::Point& p) override {
    const double dt = std::max(0.002, double(p.t) - prevT_);
    prevT_ = p.t;
    mypaint_brush_stroke_to(brush_, &surface_.base, p.x, p.y, effectivePressure(p.pressure), 0.0f,
                            0.0f, dt);
  }

private:
  MyPaintBrush* brush_;
  QtSurface surface_;
  double prevT_;
};

}  // namespace

bool available() { return true; }

QVariantList presets() {
  static QVariantList cache;
  static bool built = false;
  if (built) return cache;
  built = true;

  const QString root = ":/brushes/";
  QStringList ids;
  QDirIterator it(":/brushes", QStringList{"*.myb"}, QDir::Files, QDirIterator::Subdirectories);
  while (it.hasNext()) {
    const QString path = it.next();
    ids << path.mid(root.size(), path.size() - root.size() - 4);
  }
  std::sort(ids.begin(), ids.end(), [](const QString& a, const QString& b) {
    return a.compare(b, Qt::CaseInsensitive) < 0;
  });
  for (const QString& id : ids) {
    const int slash = id.lastIndexOf('/');
    QVariantMap m;
    m["id"] = id;
    m["group"] = slash > 0 ? id.left(slash) : QString();
    m["name"] = id.mid(slash + 1);
    m["preview"] = "qrc:/brushes/" + id + "_prev.png";
    cache << m;
  }
  return cache;
}

bool hasPreset(const QString& id) { return QFile::exists(":/brushes/" + id + ".myb"); }

void paintStroke(QImage& layer, const arf::Stroke& s) {
  if (s.points.empty() || layer.format() != QImage::Format_ARGB32_Premultiplied) return;
  MyPaintBrush* b = makeBrush(s);
  if (!b) return;
  QtSurface surf;
  initSurface(surf, &layer);

  const arf::Point& p0 = s.points.front();
  mypaint_brush_stroke_to(b, &surf.base, p0.x, p0.y, 0.0f, 0.0f, 0.0f, 1.0);
  double prevT = p0.t;
  for (const arf::Point& p : s.points) {
    const double dt = std::max(0.002, double(p.t) - prevT);
    prevT = p.t;
    mypaint_brush_stroke_to(b, &surf.base, p.x, p.y, effectivePressure(p.pressure), 0.0f, 0.0f, dt);
  }
  const arf::Point& last = s.points.back();
  mypaint_brush_stroke_to(b, &surf.base, last.x, last.y, 0.0f, 0.0f, 0.0f, 0.01);  // levanta a caneta
  mypaint_brush_unref(b);
}

std::unique_ptr<Live> beginLive(const arf::Stroke& s, QImage* target) {
  if (s.points.empty() || !target) return nullptr;
  MyPaintBrush* b = makeBrush(s);
  if (!b) return nullptr;
  return std::make_unique<LiveImpl>(b, target, s.points.front());
}

#else  // sem MyPaint neste build

bool available() { return false; }
QVariantList presets() { return {}; }
bool hasPreset(const QString&) { return false; }
void paintStroke(QImage&, const arf::Stroke&) {}
std::unique_ptr<Live> beginLive(const arf::Stroke&, QImage*) { return nullptr; }

#endif

}  // namespace mp
