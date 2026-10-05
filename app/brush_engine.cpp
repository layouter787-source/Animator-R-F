#include "brush_engine.h"

#include <QLineF>
#include <QRadialGradient>
#include <algorithm>
#include <cmath>

namespace brush {
namespace {

using arf::BrushType;

// Raio da gota conforme pressão, tipo de pincel e posição no traço (afinamento da tinta).
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

void stamp(QPainter& p, const arf::Stroke& s, QPointF c, double r, const QColor& col) {
  if (!s.antialias) {  // traço pixelado: centro e raio presos à grade de pixels
    c = QPointF(std::round(c.x()), std::round(c.y()));
    r = std::max(0.5, std::floor(r * 2 + 0.5) / 2);
  }
  p.setPen(Qt::NoPen);
  if (s.brush == BrushType::Soft && s.antialias && s.hardness < 0.99f) {
    QRadialGradient g(c, r);
    QColor edge = col;
    edge.setAlpha(0);
    g.setColorAt(0.0, col);
    g.setColorAt(std::clamp<double>(s.hardness, 0.0, 0.99), col);
    g.setColorAt(1.0, edge);
    p.setBrush(g);
  } else {
    p.setBrush(col);
  }
  p.drawEllipse(c, r, r);
}

QPointF catmullRom(const QPointF& p0, const QPointF& p1, const QPointF& p2, const QPointF& p3,
                   double t) {
  const double t2 = t * t, t3 = t2 * t;
  return 0.5 * ((2.0 * p1) + (p2 - p0) * t + (2.0 * p0 - 5.0 * p1 + 4.0 * p2 - p3) * t2 +
                (3.0 * p1 - p0 - 3.0 * p2 + p3) * t3);
}

}  // namespace

void configure(QPainter& p, const arf::Stroke& s) {
  p.setRenderHint(QPainter::Antialiasing, s.antialias);
}

std::vector<double> cumulativeLength(const arf::Stroke& s) {
  std::vector<double> cum(s.points.size(), 0.0);
  for (size_t i = 1; i < s.points.size(); ++i) {
    const double dx = s.points[i].x - s.points[i - 1].x;
    const double dy = s.points[i].y - s.points[i - 1].y;
    cum[i] = cum[i - 1] + std::sqrt(dx * dx + dy * dy);
  }
  return cum;
}

QRectF strokeBounds(const arf::Stroke& s) {
  if (s.points.empty()) return {};
  float x0 = s.points[0].x, x1 = x0, y0 = s.points[0].y, y1 = y0;
  for (const auto& q : s.points) {
    x0 = std::min(x0, q.x); x1 = std::max(x1, q.x);
    y0 = std::min(y0, q.y); y1 = std::max(y1, q.y);
  }
  const float m = s.size * 0.6f + 3.0f;
  return QRectF(QPointF(x0 - m, y0 - m), QPointF(x1 + m, y1 + m));
}

void paintSegment(QPainter& p, const arf::Stroke& s, int i, const std::vector<double>& cum,
                  double total, const QColor& color) {
  const int n = int(s.points.size());
  if (i < 0 || i + 1 >= n || int(cum.size()) < i + 2) return;
  const auto P = [&](int k) {
    k = std::clamp(k, 0, n - 1);
    return QPointF(s.points[k].x, s.points[k].y);
  };
  const QPointF p0 = P(i - 1), p1 = P(i), p2 = P(i + 1), p3 = P(i + 2);
  const double len = QLineF(p1, p2).length();
  const float pr1 = s.points[i].pressure, pr2 = s.points[i + 1].pressure;
  const double a1 = cum[i], a2 = cum[i + 1];

  // Espaço entre gotas: 20% do raio, para o traço sair contínuo e liso.
  const double rm = (radiusFor(s, pr1, a1, total) + radiusFor(s, pr2, a2, total)) / 2;
  const double spacing = std::max(0.4, rm * 0.2);
  const int steps = std::max(1, int(std::ceil(len / spacing)));
  for (int k = 0; k < steps; ++k) {
    const double t = double(k) / steps;
    stamp(p, s, catmullRom(p0, p1, p2, p3, t),
          radiusFor(s, pr1 + (pr2 - pr1) * float(t), a1 + (a2 - a1) * t, total), color);
  }
}

void paintDot(QPainter& p, const arf::Stroke& s, const QColor& color) {
  if (s.points.empty()) return;
  const auto& q = s.points[0];
  stamp(p, s, QPointF(q.x, q.y), radiusFor(s, q.pressure, 1e9, -1), color);
}

void paintStroke(QPainter& p, const arf::Stroke& s, const QColor& color) {
  if (s.points.empty()) return;
  configure(p, s);
  if (s.points.size() == 1) {
    paintDot(p, s, color);
    return;
  }
  const std::vector<double> cum = cumulativeLength(s);
  const double total = cum.back();
  for (int i = 0; i + 1 < int(s.points.size()); ++i) paintSegment(p, s, i, cum, total, color);
  const auto& last = s.points.back();
  stamp(p, s, QPointF(last.x, last.y), radiusFor(s, last.pressure, total, total), color);
}

void compositeStroke(QImage& layer, const arf::Stroke& s) {
  const QRect r = strokeBounds(s).toAlignedRect().intersected(layer.rect());
  if (r.isEmpty()) return;
  QImage tmp(r.size(), QImage::Format_ARGB32_Premultiplied);
  tmp.fill(Qt::transparent);
  {
    QPainter tp(&tmp);
    tp.translate(-r.topLeft());
    paintStroke(tp, s, s.eraser ? QColor(Qt::black) : QColor::fromRgba(s.color));
  }
  QPainter lp(&layer);
  lp.setCompositionMode(s.eraser ? QPainter::CompositionMode_DestinationOut
                                 : QPainter::CompositionMode_SourceOver);
  lp.setOpacity(s.opacity);
  lp.drawImage(r.topLeft(), tmp);
}

}  // namespace brush
