#include "drawing_view.h"

#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <algorithm>

namespace {

void paintStroke(QPainter& p, const arf::Stroke& s, bool preview) {
  if (s.points.empty()) return;
  QColor col = QColor::fromRgba(s.color);
  p.save();
  if (s.eraser) {
    if (preview) col = Qt::white;
    else p.setCompositionMode(QPainter::CompositionMode_Clear);
  }
  if (s.points.size() == 1) {
    p.setPen(Qt::NoPen);
    p.setBrush(col);
    p.drawEllipse(QPointF(s.points[0].x, s.points[0].y), s.size / 2, s.size / 2);
  } else {
    const auto pt = [&](size_t i) { return QPointF(s.points[i].x, s.points[i].y); };
    QPainterPath path;
    path.moveTo(pt(0));
    for (size_t i = 1; i + 1 < s.points.size(); ++i) path.quadTo(pt(i), (pt(i) + pt(i + 1)) / 2);
    path.lineTo(pt(s.points.size() - 1));
    p.setPen(QPen(col, s.size, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    p.setBrush(Qt::NoBrush);
    p.drawPath(path);
  }
  p.restore();
}

constexpr int kMaxCached = 16;

}  // namespace

DrawingView::DrawingView(QQuickItem* parent) : QQuickPaintedItem(parent) {
  setAcceptedMouseButtons(Qt::LeftButton);
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
  frame_ = f;
  emit frameChanged();
  update();
}

void DrawingView::setActiveLayer(int i) {
  i = std::clamp(i, 0, int(anim_.layers().size()) - 1);
  if (i == activeLayer_) return;
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
  playing_ = !playing_;
  if (playing_) playTimer_.start(1000 / anim_.fps);
  else playTimer_.stop();
  emit playingChanged();
  update();
}

void DrawingView::invalidate() {
  cache_.clear();
  ++revision_;
  emit revisionChanged();
  update();
}

qreal DrawingView::pageScale() const {
  return std::min(width() / anim_.width, height() / anim_.height);
}

QPointF DrawingView::toDoc(const QPointF& p) const {
  const qreal s = pageScale();
  const qreal ox = (width() - anim_.width * s) / 2;
  const qreal oy = (height() - anim_.height * s) / 2;
  return QPointF((p.x() - ox) / s, (p.y() - oy) / s);
}

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
  const qreal s = pageScale();
  p->translate((width() - anim_.width * s) / 2, (height() - anim_.height * s) / 2);
  p->scale(s, s);
  p->fillRect(QRectF(0, 0, anim_.width, anim_.height), Qt::white);

  if (onion_ && !playing_) {
    p->setOpacity(0.35);
    if (frame_ > 1) p->drawImage(0, 0, onionImage(frame_ - 1, QColor(200, 70, 60)));
    if (frame_ < anim_.frameCount) p->drawImage(0, 0, onionImage(frame_ + 1, QColor(60, 150, 90)));
    p->setOpacity(1.0);
  }

  p->drawImage(0, 0, compose(frame_));
  if (drawing_) {
    p->setRenderHint(QPainter::Antialiasing);
    paintStroke(*p, current_, true);
  }
}

void DrawingView::mousePressEvent(QMouseEvent* e) {
  const arf::Layer* l = (activeLayer_ < int(anim_.layers().size())) ? &anim_.layers()[activeLayer_] : nullptr;
  if (!l || !l->visible || l->locked || playing_) {
    e->ignore();
    return;
  }
  const float mul = tool_ == "Brush" ? 1.8f : (tool_ == "Eraser" ? 2.5f : 1.0f);
  current_ = arf::Stroke{};
  current_.size = float(brushSize_) * mul;
  current_.eraser = tool_ == "Eraser";
  current_.color = color_.rgba();
  const QPointF d = toDoc(e->position());
  current_.points.push_back({float(d.x()), float(d.y()), 1.0f});
  drawing_ = true;
  update();
  e->accept();
}

void DrawingView::mouseMoveEvent(QMouseEvent* e) {
  if (!drawing_) return;
  const QPointF d = toDoc(e->position());
  const auto& last = current_.points.back();
  if (std::abs(d.x() - last.x) + std::abs(d.y() - last.y) < 0.5) return;
  current_.points.push_back({float(d.x()), float(d.y()), 1.0f});
  update();
}

void DrawingView::mouseReleaseEvent(QMouseEvent* e) {
  if (!drawing_) return;
  drawing_ = false;
  anim_.addStroke(activeLayer_, frame_, std::move(current_));
  current_ = arf::Stroke{};
  invalidate();
  e->accept();
}
