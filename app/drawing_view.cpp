#include "drawing_view.h"

#include <QInputDevice>
#include <QLineF>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QTouchEvent>
#include <QWheelEvent>
#include <algorithm>
#include <cmath>

namespace {

constexpr int kMaxCached = 16;
constexpr qreal kMinZoom = 0.2;
constexpr qreal kMaxZoom = 16.0;

// Largura do traço conforme a pressão (1.0 = largura cheia).
float widthFor(const arf::Stroke& s, float pressure) {
  return s.size * (0.2f + 0.8f * std::clamp(pressure, 0.0f, 1.0f));
}

void paintStroke(QPainter& p, const arf::Stroke& s, bool preview) {
  if (s.points.empty()) return;
  QColor col = QColor::fromRgba(s.color);
  p.save();
  if (s.eraser) {
    if (preview) col = Qt::white;
    else p.setCompositionMode(QPainter::CompositionMode_Clear);
  }
  const auto pt = [&](size_t i) { return QPointF(s.points[i].x, s.points[i].y); };
  const float p0 = s.points[0].pressure;

  if (s.points.size() == 1) {
    p.setPen(Qt::NoPen);
    p.setBrush(col);
    const qreal r = widthFor(s, p0) / 2;
    p.drawEllipse(pt(0), r, r);
  } else {
    bool uniform = true;
    for (const auto& q : s.points)
      if (std::abs(q.pressure - p0) > 0.02f) { uniform = false; break; }

    if (uniform) {
      QPainterPath path;
      path.moveTo(pt(0));
      for (size_t i = 1; i + 1 < s.points.size(); ++i) path.quadTo(pt(i), (pt(i) + pt(i + 1)) / 2);
      path.lineTo(pt(s.points.size() - 1));
      p.setPen(QPen(col, widthFor(s, p0), Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
      p.setBrush(Qt::NoBrush);
      p.drawPath(path);
    } else {
      // Pressão variável: um segmento por par de pontos, com largura própria.
      for (size_t i = 1; i < s.points.size(); ++i) {
        const float pr = (s.points[i - 1].pressure + s.points[i].pressure) / 2;
        p.setPen(QPen(col, widthFor(s, pr), Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        p.drawLine(pt(i - 1), pt(i));
      }
    }
  }
  p.restore();
}

// Caneta usa a pressão real; dedo e mouse usam pressão cheia.
float pressureOf(const QMouseEvent* e) {
  const auto* dev = e->pointingDevice();
  if (dev && dev->type() == QInputDevice::DeviceType::Stylus && !e->points().isEmpty())
    return std::clamp(float(e->points().first().pressure()), 0.05f, 1.0f);
  return 1.0f;
}

}  // namespace

DrawingView::DrawingView(QQuickItem* parent) : QQuickPaintedItem(parent) {
  setAcceptedMouseButtons(Qt::LeftButton | Qt::RightButton | Qt::MiddleButton);
  setAcceptTouchEvents(true);
  setAntialiasing(true);
  setOpaquePainting(true);
  anim_.addLayer("Camada 1");
  playTimer_.setTimerType(Qt::PreciseTimer);
  connect(&playTimer_, &QTimer::timeout, this,
          [this] { setFrame(frame_ % anim_.frameCount + 1); });
}

QStringList DrawingView::layerNames() const {
  QStringList names;
  for (const auto& l : anim_.layers()) names << QString::fromStdString(l.name);
  return names;
}

void DrawingView::setFrame(int f) {
  f = std::clamp(f, 1, anim_.frameCount);
  if (f == frame_) return;
  cancelStroke();
  frame_ = f;
  emit frameChanged();
  update();
}

void DrawingView::setActiveLayer(int i) {
  i = std::clamp(i, 0, int(anim_.layers().size()) - 1);
  if (i == activeLayer_) return;
  cancelStroke();
  activeLayer_ = i;
  ++revision_;
  emit layersChanged();
  emit revisionChanged();
}

void DrawingView::setTool(const QString& t) {
  if (t == tool_) return;
  tool_ = t;
  emit toolChanged();
}

void DrawingView::setColor(const QColor& c) {
  if (c == color_) return;
  color_ = c;
  emit colorChanged();
}

void DrawingView::setBrushSize(qreal s) {
  s = std::clamp<qreal>(s, 1, 80);
  if (qFuzzyCompare(s, brushSize_)) return;
  brushSize_ = s;
  emit brushSizeChanged();
}

void DrawingView::setOnionSkin(bool on) {
  if (on == onion_) return;
  onion_ = on;
  emit onionSkinChanged();
  update();
}

int DrawingView::addLayer() {
  const int i = anim_.addLayer(QString("Camada %1").arg(anim_.layers().size() + 1).toStdString());
  activeLayer_ = i;
  ++revision_;
  emit layersChanged();
  emit revisionChanged();
  return i;
}

void DrawingView::toggleLayerVisible(int index) {
  if (arf::Layer* l = anim_.layer(index)) {
    l->visible = !l->visible;
    invalidate();
    emit layersChanged();
  }
}

bool DrawingView::layerVisible(int index) const {
  return index >= 0 && index < int(anim_.layers().size()) && anim_.layers()[index].visible;
}

bool DrawingView::hasKey(int frame) const { return anim_.hasKey(activeLayer_, frame); }

void DrawingView::insertBlankKey() {
  anim_.insertBlankKey(activeLayer_, frame_);
  invalidate();
}

void DrawingView::undo() {
  if (anim_.undo()) invalidate();
}

void DrawingView::redo() {
  if (anim_.redo()) invalidate();
}

void DrawingView::togglePlay() {
  cancelStroke();
  playing_ = !playing_;
  if (playing_) playTimer_.start(1000 / anim_.fps);
  else playTimer_.stop();
  emit playingChanged();
  update();
}

void DrawingView::resetView() {
  zoom_ = 1.0;
  rotation_ = 0.0;
  pan_ = QPointF();
  emit viewChanged();
  update();
}

void DrawingView::invalidate() {
  cache_.clear();
  ++revision_;
  emit revisionChanged();
  update();
}

// ---- vista ----------------------------------------------------------------

qreal DrawingView::baseScale() const {
  return std::min(width() / anim_.width, height() / anim_.height);
}

QTransform DrawingView::viewTransform() const {
  const qreal s = baseScale() * zoom_;
  QTransform t;
  t.translate(width() / 2 + pan_.x(), height() / 2 + pan_.y());
  t.rotate(rotation_);
  t.scale(s, s);
  t.translate(-anim_.width / 2.0, -anim_.height / 2.0);
  return t;
}

QPointF DrawingView::toDoc(const QPointF& p) const {
  bool ok = false;
  const QTransform inv = viewTransform().inverted(&ok);
  return ok ? inv.map(p) : p;
}

void DrawingView::zoomAbout(const QPointF& center, qreal factor) {
  const QPointF docPt = toDoc(center);
  zoom_ = std::clamp(zoom_ * factor, kMinZoom, kMaxZoom);
  pan_ += center - viewTransform().map(docPt);  // mantém o ponto sob o dedo/cursor
  emit viewChanged();
  update();
}

// ---- composição -----------------------------------------------------------

QImage DrawingView::compose(int frame) {
  if (auto it = cache_.constFind(frame); it != cache_.constEnd()) return *it;
  const QSize sz(anim_.width, anim_.height);
  QImage out(sz, QImage::Format_ARGB32_Premultiplied);
  out.fill(Qt::transparent);
  QPainter op(&out);
  for (int i = 0; i < int(anim_.layers().size()); ++i) {
    const auto& layer = anim_.layers()[i];
    if (!layer.visible) continue;
    const arf::Drawing* d = anim_.drawingAt(i, frame);
    if (!d || d->strokes.empty()) continue;
    QImage li(sz, QImage::Format_ARGB32_Premultiplied);
    li.fill(Qt::transparent);
    QPainter lp(&li);
    lp.setRenderHint(QPainter::Antialiasing);
    for (const auto& s : d->strokes) paintStroke(lp, s, false);
    lp.end();
    op.setOpacity(layer.opacity);
    op.drawImage(0, 0, li);
  }
  op.end();
  if (cache_.size() >= kMaxCached) cache_.clear();
  cache_.insert(frame, out);
  return out;
}

QImage DrawingView::onionImage(int frame, const QColor& tint) {
  const int key = (tint.red() > tint.green() ? 100000 : 200000) + frame;
  if (auto it = cache_.constFind(key); it != cache_.constEnd()) return *it;
  QImage img = compose(frame);
  QPainter tp(&img);
  tp.setCompositionMode(QPainter::CompositionMode_SourceIn);
  tp.fillRect(img.rect(), tint);
  tp.end();
  if (cache_.size() >= kMaxCached) cache_.clear();
  cache_.insert(key, img);
  return img;
}

void DrawingView::paint(QPainter* p) {
  p->fillRect(boundingRect(), QColor("#2b2d31"));
  p->setRenderHint(QPainter::SmoothPixmapTransform);
  p->setRenderHint(QPainter::Antialiasing);
  p->setTransform(viewTransform());
  p->fillRect(QRectF(0, 0, anim_.width, anim_.height), Qt::white);

  if (onion_ && !playing_) {
    p->setOpacity(0.35);
    if (frame_ > 1) p->drawImage(0, 0, onionImage(frame_ - 1, QColor(200, 70, 60)));
    if (frame_ < anim_.frameCount) p->drawImage(0, 0, onionImage(frame_ + 1, QColor(60, 150, 90)));
    p->setOpacity(1.0);
  }

  p->drawImage(0, 0, compose(frame_));
  if (drawing_) paintStroke(*p, current_, true);
}

// ---- traço ----------------------------------------------------------------

bool DrawingView::beginStroke(const QPointF& pos, float pressure) {
  const arf::Layer* l =
      (activeLayer_ < int(anim_.layers().size())) ? &anim_.layers()[activeLayer_] : nullptr;
  if (!l || !l->visible || l->locked || playing_) return false;
  const float mul = tool_ == "Brush" ? 1.8f : (tool_ == "Eraser" ? 2.5f : 1.0f);
  current_ = arf::Stroke{};
  current_.size = float(brushSize_) * mul;
  current_.eraser = tool_ == "Eraser";
  current_.color = color_.rgba();
  const QPointF d = toDoc(pos);
  current_.points.push_back({float(d.x()), float(d.y()), pressure});
  drawing_ = true;
  update();
  return true;
}

void DrawingView::extendStroke(const QPointF& pos, float pressure) {
  if (!drawing_) return;
  const QPointF d = toDoc(pos);
  const auto& last = current_.points.back();
  if (std::abs(d.x() - last.x) + std::abs(d.y() - last.y) < 0.5) return;
  current_.points.push_back({float(d.x()), float(d.y()), pressure});
  update();
}

void DrawingView::endStroke() {
  if (!drawing_) return;
  drawing_ = false;
  anim_.addStroke(activeLayer_, frame_, std::move(current_));
  current_ = arf::Stroke{};
  invalidate();
}

void DrawingView::cancelStroke() {
  if (!drawing_) return;
  drawing_ = false;
  current_ = arf::Stroke{};
  update();
}

// ---- entrada --------------------------------------------------------------

void DrawingView::mousePressEvent(QMouseEvent* e) {
  if (e->button() != Qt::LeftButton) {  // botão direito/meio arrasta a vista (desktop)
    panning_ = true;
    lastMouse_ = e->position();
    e->accept();
    return;
  }
  if (beginStroke(e->position(), pressureOf(e))) e->accept();
  else e->ignore();
}

void DrawingView::mouseMoveEvent(QMouseEvent* e) {
  if (panning_) {
    pan_ += e->position() - lastMouse_;
    lastMouse_ = e->position();
    emit viewChanged();
    update();
    return;
  }
  extendStroke(e->position(), pressureOf(e));
}

void DrawingView::mouseReleaseEvent(QMouseEvent* e) {
  if (panning_) {
    panning_ = false;
    e->accept();
    return;
  }
  endStroke();
  e->accept();
}

void DrawingView::wheelEvent(QWheelEvent* e) {
  zoomAbout(e->position(), std::pow(1.0015, e->angleDelta().y()));
  e->accept();
}

void DrawingView::touchEvent(QTouchEvent* e) {
  if (e->type() == QEvent::TouchCancel) {
    cancelStroke();
    gesture_ = false;
    e->accept();
    return;
  }

  const auto& pts = e->points();
  QList<const QEventPoint*> active;
  for (const auto& p : pts)
    if (p.state() != QEventPoint::Released) active.append(&p);

  if (active.size() >= 2) {
    // Dois dedos: zoom + pan + rotação em torno do ponto entre os dedos.
    cancelStroke();
    gesture_ = true;
    const QPointF a = active[0]->position(), b = active[1]->position();
    const QPointF pa = active[0]->lastPosition(), pb = active[1]->lastPosition();
    const qreal d = QLineF(a, b).length(), pd = QLineF(pa, pb).length();
    if (d > 1 && pd > 1) {
      const QPointF c = (a + b) / 2, pc = (pa + pb) / 2;
      qreal dAng = std::atan2(b.y() - a.y(), b.x() - a.x()) - std::atan2(pb.y() - pa.y(), pb.x() - pa.x());
      while (dAng > M_PI) dAng -= 2 * M_PI;
      while (dAng < -M_PI) dAng += 2 * M_PI;

      const QPointF docPt = toDoc(pc);
      zoom_ = std::clamp(zoom_ * d / pd, kMinZoom, kMaxZoom);
      rotation_ += dAng * 180.0 / M_PI;
      pan_ += c - viewTransform().map(docPt);
      emit viewChanged();
      update();
    }
    e->accept();
    return;
  }

  if (active.isEmpty()) {  // todos os dedos saíram
    if (gesture_) gesture_ = false;
    else endStroke();
    e->accept();
    return;
  }

  if (!gesture_) {  // um dedo: desenha
    const QEventPoint& p = *active[0];
    if (p.state() == QEventPoint::Pressed) beginStroke(p.position(), 1.0f);
    else if (p.state() == QEventPoint::Updated) extendStroke(p.position(), 1.0f);
  }
  e->accept();
}
