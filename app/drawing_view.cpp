#include "drawing_view.h"

#include <QInputDevice>
#include <QLineF>
#include <QMouseEvent>
#include <QPainter>
#include <QTouchEvent>
#include <QWheelEvent>
#include <algorithm>
#include <cmath>

#include "brush_engine.h"

namespace {

constexpr int kMaxCached = 16;
constexpr int kMaxLayerImages = 24;
constexpr qreal kMinZoom = 0.2;
constexpr qreal kMaxZoom = 16.0;

quint64 layerKey(int layer, int keyFrame) {
  return (quint64(quint32(layer)) << 32) | quint64(quint32(keyFrame));
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

void DrawingView::setBrushOpacity(qreal o) {
  o = std::clamp<qreal>(o, 0.05, 1.0);
  if (qFuzzyCompare(o, brushOpacity_)) return;
  brushOpacity_ = o;
  emit brushOpacityChanged();
}

void DrawingView::setStabilizer(qreal s) {
  s = std::clamp<qreal>(s, 0.0, 1.0);
  if (qFuzzyCompare(s + 1.0, stabilizer_ + 1.0)) return;
  stabilizer_ = s;
  emit stabilizerChanged();
}

void DrawingView::setSmoothStrokes(bool on) {
  if (on == smoothStrokes_) return;
  smoothStrokes_ = on;
  emit smoothStrokesChanged();
  update();
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
  if (anim_.undo()) invalidate(true);
}

void DrawingView::redo() {
  if (anim_.redo()) invalidate(true);
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

void DrawingView::invalidate(bool layersToo) {
  cache_.clear();
  if (layersToo) layerImgs_.clear();
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

// Imagem do desenho (quadro-chave) de uma camada, com todos os traços já pintados.
QImage DrawingView::layerImage(int layer, int frame) {
  const int key = anim_.keyFrameAt(layer, frame);
  if (key == 0) return QImage();
  const quint64 id = layerKey(layer, key);
  if (auto it = layerImgs_.constFind(id); it != layerImgs_.constEnd()) return *it;

  QImage img(QSize(anim_.width, anim_.height), QImage::Format_ARGB32_Premultiplied);
  img.fill(Qt::transparent);
  if (const arf::Drawing* d = anim_.drawingAt(layer, frame))
    for (const auto& s : d->strokes) brush::compositeStroke(img, s);
  if (layerImgs_.size() >= kMaxLayerImages) layerImgs_.clear();
  layerImgs_.insert(id, img);
  return img;
}

QImage DrawingView::compose(int frame) {
  if (auto it = cache_.constFind(frame); it != cache_.constEnd()) return *it;
  QImage out(QSize(anim_.width, anim_.height), QImage::Format_ARGB32_Premultiplied);
  out.fill(Qt::transparent);
  QPainter op(&out);
  for (int i = 0; i < int(anim_.layers().size()); ++i) {
    const auto& layer = anim_.layers()[i];
    if (!layer.visible) continue;
    const QImage li = layerImage(i, frame);
    if (li.isNull()) continue;
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
  // Suavização ligada: amostragem bilinear ao dar zoom. Desligada: pixels nítidos.
  p->setRenderHint(QPainter::SmoothPixmapTransform, smoothStrokes_);
  p->setRenderHint(QPainter::Antialiasing, true);
  p->setTransform(viewTransform());
  p->fillRect(QRectF(0, 0, anim_.width, anim_.height), Qt::white);

  if (onion_ && !playing_) {
    p->setOpacity(0.35);
    if (frame_ > 1) p->drawImage(0, 0, onionImage(frame_ - 1, QColor(200, 70, 60)));
    if (frame_ < anim_.frameCount) p->drawImage(0, 0, onionImage(frame_ + 1, QColor(60, 150, 90)));
    p->setOpacity(1.0);
  }

  p->drawImage(0, 0, compose(frame_));
  // Traço em andamento: desenhado como forma única e contínua (vetorial, na resolução da tela).
  if (drawing_) brush::paintLive(*p, current_, previewColor(), QRect(0, 0, anim_.width, anim_.height));
}

// ---- traço ----------------------------------------------------------------

QColor DrawingView::previewColor() const {
  return current_.eraser ? QColor(Qt::white) : QColor::fromRgba(current_.color);
}

bool DrawingView::beginStroke(const QPointF& pos, float pressure) {
  const arf::Layer* l =
      (activeLayer_ < int(anim_.layers().size())) ? &anim_.layers()[activeLayer_] : nullptr;
  if (!l || !l->visible || l->locked || playing_) return false;

  current_ = arf::Stroke{};
  if (tool_ == "Ink") {
    current_.brush = arf::BrushType::Ink;
    current_.size = float(brushSize_);
  } else if (tool_ == "Brush") {
    current_.brush = arf::BrushType::Soft;
    current_.size = float(brushSize_) * 2.2f;
    current_.hardness = 0.3f;
  } else if (tool_ == "Eraser") {
    current_.size = float(brushSize_) * 2.5f;
    current_.eraser = true;
  } else {
    current_.size = float(brushSize_) * 0.7f;
  }
  current_.opacity = current_.eraser ? 1.0f : float(brushOpacity_);
  current_.antialias = smoothStrokes_;
  current_.color = color_.rgba();

  const QPointF d = toDoc(pos);
  current_.points.push_back({float(d.x()), float(d.y()), pressure});
  rawLast_ = d;
  rawPressure_ = pressure;
  drawing_ = true;
  update();
  return true;
}

void DrawingView::addPoint(const QPointF& docPt, float pressure) {
  const auto& last = current_.points.back();
  if (QLineF(QPointF(last.x, last.y), docPt).length() < 0.6) return;
  current_.points.push_back({float(docPt.x()), float(docPt.y()), pressure});
  update();
}

void DrawingView::extendStroke(const QPointF& pos, float pressure) {
  if (!drawing_) return;
  const QPointF raw = toDoc(pos);
  rawLast_ = raw;
  rawPressure_ = pressure;
  // Estabilizador: a ponta do traço persegue o dedo/caneta com atraso (média exponencial).
  const double k = 1.0 - 0.92 * stabilizer_;
  const auto last = current_.points.back();
  const QPointF np(last.x + (raw.x() - last.x) * k, last.y + (raw.y() - last.y) * k);
  const float npr = last.pressure + (pressure - last.pressure) * 0.5f;
  addPoint(np, npr);
}

void DrawingView::finishStabilizer() {
  const double k = std::max(0.35, 1.0 - 0.92 * stabilizer_);
  for (int i = 0; i < 40; ++i) {  // alcança o ponto final real
    const auto last = current_.points.back();
    const QPointF cur(last.x, last.y);
    if (QLineF(cur, rawLast_).length() <= 0.6) break;
    addPoint(cur + (rawLast_ - cur) * k, rawPressure_);
  }
}

void DrawingView::endStroke() {
  if (!drawing_) return;
  finishStabilizer();
  drawing_ = false;
  const int layer = activeLayer_, frame = frame_;
  if (anim_.addStroke(layer, frame, current_)) {
    // Pinta só o traço novo na imagem da camada, sem refazer o desenho todo.
    auto it = layerImgs_.find(layerKey(layer, frame));
    if (it != layerImgs_.end()) brush::compositeStroke(*it, current_);
    cache_.clear();
    ++revision_;
    emit revisionChanged();
  }
  current_ = arf::Stroke{};
  update();
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
