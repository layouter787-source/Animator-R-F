#include "brush_engine.h"

#include <QLineF>
#include <QPainterPath>
#include <QPolygonF>
#include <algorithm>
#include <cmath>
#include <vector>

namespace brush {
namespace {

using arf::BrushType;

struct Sample {
  QPointF pos;
  double radius;
};

// Raio conforme pressão, tipo de pincel e posição no traço (afinamento da tinta nas pontas).
double radiusFor(const arf::Stroke& s, float pressure, double arc, double total) {
  const double pr = std::clamp<double>(pressure, 0.0, 1.0);
  double w;
  switch (s.brush) {
    case BrushType::Ink: w = 0.12 + 0.88 * pr; break;
    case BrushType::Soft: w = 0.35 + 0.65 * pr; break;
    default: w = 0.55 + 0.45 * pr; break;
  }
  if (s.brush == BrushType::Ink) {
    const double taper = std::max<double>(s.size * 3.0, 6.0);
    double m = std::min(1.0, 0.25 + 0.75 * arc / taper);
    if (total >= 0) m = std::min(m, std::min(1.0, 0.1 + 0.9 * (total - arc) / taper));
    w *= std::clamp(m, 0.05, 1.0);
  }
  return std::max(0.3, 0.5 * s.size * w);
}

QPointF catmullRom(const QPointF& p0, const QPointF& p1, const QPointF& p2, const QPointF& p3,
                   double t) {
  const double t2 = t * t, t3 = t2 * t;
  return 0.5 * ((2.0 * p1) + (p2 - p0) * t + (2.0 * p0 - 5.0 * p1 + 4.0 * p2 - p3) * t2 +
                (3.0 * p1 - p0 - 3.0 * p2 + p3) * t3);
}

// Linha central do traço: curva Catmull-Rom reamostrada com passo fino, com o raio em cada ponto.
std::vector<Sample> sampleStroke(const arf::Stroke& s, bool finalEnd) {
  const int n = int(s.points.size());
  std::vector<Sample> out;
  if (n == 0) return out;

  std::vector<double> cum(n, 0.0);
  for (int i = 1; i < n; ++i) {
    const double dx = s.points[i].x - s.points[i - 1].x;
    const double dy = s.points[i].y - s.points[i - 1].y;
    cum[i] = cum[i - 1] + std::sqrt(dx * dx + dy * dy);
  }
  const double total = finalEnd ? cum.back() : -1.0;
  const auto P = [&](int k) {
    k = std::clamp(k, 0, n - 1);
    return QPointF(s.points[k].x, s.points[k].y);
  };

  if (n == 1) {  // toque simples: um ponto cheio
    out.push_back({P(0), radiusFor(s, s.points[0].pressure, 1e9, -1)});
    return out;
  }

  for (int i = 0; i + 1 < n; ++i) {
    const QPointF p0 = P(i - 1), p1 = P(i), p2 = P(i + 1), p3 = P(i + 2);
    const double len = QLineF(p1, p2).length();
    const double r1 = radiusFor(s, s.points[i].pressure, cum[i], total);
    const double r2 = radiusFor(s, s.points[i + 1].pressure, cum[i + 1], total);
    const double spacing = std::clamp((r1 + r2) * 0.3, 0.5, 2.0);
    const int steps = std::max(1, int(std::ceil(len / spacing)));
    for (int k = 0; k < steps; ++k) {
      const double t = double(k) / steps;
      out.push_back({catmullRom(p0, p1, p2, p3, t), r1 + (r2 - r1) * t});
    }
  }
  out.push_back({P(n - 1), radiusFor(s, s.points[n - 1].pressure, cum[n - 1], total)});

  // Suaviza o raio (tira o ruído da pressão), mantendo as pontas.
  for (int pass = 0; pass < 2; ++pass) {
    std::vector<double> r(out.size());
    for (size_t k = 0; k < out.size(); ++k) r[k] = out[k].radius;
    for (size_t k = 1; k + 1 < out.size(); ++k)
      out[k].radius = 0.25 * r[k - 1] + 0.5 * r[k] + 0.25 * r[k + 1];
  }
  return out;
}

void addCircle(QPainterPath& path, const QPointF& c, double r) {
  const int seg = std::clamp(int(8 + r * 2.5), 12, 48);
  QPolygonF poly;
  poly.reserve(seg);
  for (int i = 0; i < seg; ++i) {
    const double a = 2.0 * M_PI * i / seg;
    poly << QPointF(c.x() + r * std::cos(a), c.y() + r * std::sin(a));
  }
  path.addPolygon(poly);
  path.closeSubpath();
}

// Uma única forma: borda esquerda, borda direita e pontas redondas, preenchidas juntas.
QPainterPath ribbon(const std::vector<Sample>& smp, double scale) {
  QPainterPath path;
  path.setFillRule(Qt::WindingFill);
  const int n = int(smp.size());
  if (n == 0) return path;
  if (n == 1) {
    addCircle(path, smp[0].pos, smp[0].radius * scale);
    return path;
  }

  std::vector<QPointF> dir(n);
  for (int k = 0; k < n; ++k) {
    const QPointF d = smp[std::min(n - 1, k + 1)].pos - smp[std::max(0, k - 1)].pos;
    const double l = std::hypot(d.x(), d.y());
    dir[k] = l > 1e-9 ? d / l : (k > 0 ? dir[k - 1] : QPointF(1, 0));
  }

  QPolygonF poly;
  poly.reserve(2 * n);
  std::vector<QPointF> right(n);
  for (int k = 0; k < n; ++k) {
    const double r = smp[k].radius * scale;
    const QPointF nrm(dir[k].y() * r, -dir[k].x() * r);
    poly << smp[k].pos + nrm;
    right[k] = smp[k].pos - nrm;
  }
  for (int k = n - 1; k >= 0; --k) poly << right[k];
  path.addPolygon(poly);
  path.closeSubpath();

  addCircle(path, smp.front().pos, smp.front().radius * scale);
  addCircle(path, smp.back().pos, smp.back().radius * scale);
  // Curvas fechadas: um círculo no vértice evita falhas na borda interna.
  for (int k = 1; k + 1 < n; ++k)
    if (QPointF::dotProduct(dir[k - 1], dir[k + 1]) < 0.9)
      addCircle(path, smp[k].pos, smp[k].radius * scale);
  return path;
}

void fillStroke(QPainter& p, const arf::Stroke& s, const std::vector<Sample>& smp,
                const QColor& color) {
  p.setPen(Qt::NoPen);
  if (s.brush == BrushType::Soft && s.antialias && s.hardness < 0.99f) {
    // Pincel macio: camadas da mesma forma, cada vez menores, formam a borda esfumada.
    constexpr int kPasses = 6;
    const double feather = (1.0 - s.hardness) * 0.8;
    QColor c = color;
    c.setAlphaF(color.alphaF() * 0.4);
    p.setBrush(c);
    for (int j = 0; j < kPasses; ++j) {
      const double sc = (1.0 + feather * (1.0 - double(j) / (kPasses - 1))) / (1.0 + feather * 0.5);
      p.drawPath(ribbon(smp, sc));
    }
    return;
  }
  p.setBrush(color);
  p.drawPath(ribbon(smp, 1.0));
}

}  // namespace

void configure(QPainter& p, const arf::Stroke& s) {
  p.setRenderHint(QPainter::Antialiasing, s.antialias);
}

QRectF strokeBounds(const arf::Stroke& s) {
  if (s.points.empty()) return {};
  float x0 = s.points[0].x, x1 = x0, y0 = s.points[0].y, y1 = y0;
  for (const auto& q : s.points) {
    x0 = std::min(x0, q.x); x1 = std::max(x1, q.x);
    y0 = std::min(y0, q.y); y1 = std::max(y1, q.y);
  }
  const float m = s.size * 0.8f + 4.0f;
  return QRectF(QPointF(x0 - m, y0 - m), QPointF(x1 + m, y1 + m));
}

void paintStroke(QPainter& p, const arf::Stroke& s, const QColor& color, bool finalEnd) {
  if (s.points.empty()) return;
  configure(p, s);
  std::vector<Sample> smp = sampleStroke(s, finalEnd);
  if (!s.antialias) {  // traço pixelado: pontos presos à grade de pixels
    for (auto& q : smp) {
      q.pos = QPointF(std::round(q.pos.x()), std::round(q.pos.y()));
      q.radius = std::max(0.5, std::floor(q.radius * 2 + 0.5) / 2);
    }
  }
  fillStroke(p, s, smp, color);
}

void paintLive(QPainter& p, const arf::Stroke& s, const QColor& color, const QRect& docRect) {
  if (s.points.empty()) return;
  if (s.opacity >= 0.999f && s.brush != BrushType::Soft) {
    p.save();
    paintStroke(p, s, color, false);
    p.restore();
    return;
  }
  const QRect r = strokeBounds(s).toAlignedRect().intersected(docRect);
  if (r.isEmpty()) return;
  QImage tmp(r.size(), QImage::Format_ARGB32_Premultiplied);
  tmp.fill(Qt::transparent);
  {
    QPainter tp(&tmp);
    tp.translate(-r.topLeft());
    paintStroke(tp, s, color, false);
  }
  p.save();
  p.setOpacity(s.opacity);
  p.drawImage(r.topLeft(), tmp);
  p.restore();
}

void compositeStroke(QImage& layer, const arf::Stroke& s) {
  const QRect r = strokeBounds(s).toAlignedRect().intersected(layer.rect());
  if (r.isEmpty()) return;
  QImage tmp(r.size(), QImage::Format_ARGB32_Premultiplied);
  tmp.fill(Qt::transparent);
  {
    QPainter tp(&tmp);
    tp.translate(-r.topLeft());
    paintStroke(tp, s, s.eraser ? QColor(Qt::black) : QColor::fromRgba(s.color), true);
  }
  QPainter lp(&layer);
  lp.setCompositionMode(s.eraser ? QPainter::CompositionMode_DestinationOut
                                 : QPainter::CompositionMode_SourceOver);
  lp.setOpacity(s.opacity);
  lp.drawImage(r.topLeft(), tmp);
}

}  // namespace brush
