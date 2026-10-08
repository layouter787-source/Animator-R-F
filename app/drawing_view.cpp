#include "drawing_view.h"

#include <QBuffer>
#include <QFile>
#include <QInputDevice>
#include <QLineF>
#include <QMouseEvent>
#include <QPainter>
#include <QRegularExpression>
#include <QTouchEvent>
#include <QWheelEvent>
#include <algorithm>
#include <cmath>

#include "brush_engine.h"
#include "gif_writer.h"
#include "mp4_export.h"
#include "project_io.h"
#include "project_store.h"
#include "zip_writer.h"

namespace {

constexpr qreal kMinZoom = 0.2;
constexpr qreal kMaxZoom = 16.0;

quint64 layerKey(int layer, int keyFrame) {
  return (quint64(quint32(layer)) << 32) | quint64(quint32(keyFrame));
}

// Pincel MyPaint ou pincel interno, conforme o traço.
void applyStroke(QImage& layer, const arf::Stroke& s) {
  if (!s.preset.empty()) mp::paintStroke(layer, s);
  else brush::compositeStroke(layer, s);
}

// Caneta usa a pressão real; dedo e mouse usam pressão cheia.
float pressureOf(const QMouseEvent* e) {
  const auto* dev = e->pointingDevice();
  if (dev && dev->type() == QInputDevice::DeviceType::Stylus && !e->points().isEmpty())
    return std::clamp(float(e->points().first().pressure()), 0.05f, 1.0f);
  return 1.0f;
}

QByteArray pngBytes(const QImage& img) {
  QByteArray data;
  QBuffer buf(&data);
  buf.open(QIODevice::WriteOnly);
  img.save(&buf, "PNG");
  return data;
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
  saveTimer_.setSingleShot(true);
  saveTimer_.setInterval(1500);
  connect(&saveTimer_, &QTimer::timeout, this, &DrawingView::saveNow);
}

DrawingView::~DrawingView() { saveNow(); }

// Limites de cache proporcionais ao tamanho do projeto (4K ocupa muito mais memória que 720p).
int DrawingView::maxCachedFrames() const {
  const qint64 bytes = qint64(anim_.width) * anim_.height * 4;
  return int(std::clamp<qint64>(160'000'000 / std::max<qint64>(bytes, 1), 2, 16));
}

int DrawingView::maxLayerImages() const {
  const qint64 bytes = qint64(anim_.width) * anim_.height * 4;
  return int(std::clamp<qint64>(200'000'000 / std::max<qint64>(bytes, 1), 4, 24));
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

void DrawingView::setPreset(const QString& id) {
  if (id == preset_) return;
  preset_ = id;
  emit presetChanged();
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
  scheduleSave();
  return i;
}

void DrawingView::toggleLayerVisible(int index) {
  if (arf::Layer* l = anim_.layer(index)) {
    l->visible = !l->visible;
    invalidate();
    emit layersChanged();
    scheduleSave();
  }
}

bool DrawingView::layerVisible(int index) const {
  return index >= 0 && index < int(anim_.layers().size()) && anim_.layers()[index].visible;
}

bool DrawingView::hasKey(int frame) const { return anim_.hasKey(activeLayer_, frame); }

void DrawingView::insertBlankKey() {
  anim_.insertBlankKey(activeLayer_, frame_);
  invalidate();
  scheduleSave();
}

void DrawingView::undo() {
  if (anim_.undo()) {
    invalidate(true);
    scheduleSave();
  }
}

void DrawingView::redo() {
  if (anim_.redo()) {
    invalidate(true);
    scheduleSave();
  }
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

// ---- projeto --------------------------------------------------------------

bool DrawingView::openProject(const QString& id) {
  arf::Animation loaded;
  QString name;
  if (id.isEmpty() || !projectio::load(ProjectStore::projectFile(id), loaded, &name)) return false;

  cancelStroke();
  playTimer_.stop();
  if (playing_) {
    playing_ = false;
    emit playingChanged();
  }
  saveTimer_.stop();

  anim_ = std::move(loaded);
  projectId_ = id;
  projectName_ = name;
  frame_ = 1;
  activeLayer_ = 0;
  cache_.clear();
  layerImgs_.clear();
  liveImg_ = QImage();
  zoom_ = 1.0;
  rotation_ = 0.0;
  pan_ = QPointF();
  ++revision_;
  emit projectChanged();
  emit frameChanged();
  emit layersChanged();
  emit revisionChanged();
  emit viewChanged();
  update();
  return true;
}

void DrawingView::scheduleSave() {
  if (!projectId_.isEmpty()) saveTimer_.start();
}

void DrawingView::saveNow() {
  saveTimer_.stop();
  if (projectId_.isEmpty()) return;
  if (!projectio::save(ProjectStore::projectFile(projectId_), anim_, projectName_)) return;
  renderFrame(1)
      .scaled(360, 360, Qt::KeepAspectRatio, Qt::SmoothTransformation)
      .save(ProjectStore::thumbFile(projectId_), "PNG");
  ProjectStore::writeMeta(projectId_, projectName_, anim_.width, anim_.height, anim_.fps,
                          anim_.frameCount);
}

QImage DrawingView::renderFrame(int frame) {
  QImage img(anim_.width, anim_.height, QImage::Format_RGB32);
  img.fill(Qt::white);
  QPainter p(&img);
  p.drawImage(0, 0, compose(frame));
  return img;
}

void DrawingView::exportAs(const QString& kind, const QUrl& url) {
  if (busy_) return;
  cancelStroke();
  busy_ = true;
  emit busyChanged();
  // Deixa a tela mostrar "exportando" antes de começar o trabalho pesado.
  QTimer::singleShot(50, this, [this, kind, url] {
    QString message;
    const bool ok = doExport(kind, url, &message);
    busy_ = false;
    emit busyChanged();
    emit exportFinished(ok, message);
  });
}

bool DrawingView::doExport(const QString& kind, const QUrl& url, QString* message) {
  // No Android o destino é um endereço content:// escolhido pelo usuário.
  const QString path = url.isLocalFile() ? url.toLocalFile() : url.toString();
  QFile f(path);
  if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
    *message = QStringLiteral("Não foi possível criar o arquivo.");
    return false;
  }

  QString base = projectName_;
  base.replace(QRegularExpression("[^\\w\\-]+"), "_");
  if (base.isEmpty()) base = "animacao";

  bool ok = true;
  if (kind == "png") {
    ok = f.write(pngBytes(renderFrame(frame_))) > 0;
  } else if (kind == "gif") {
    const int delay = std::max(2, int(std::lround(100.0 / anim_.fps)));
    const QByteArray gif = encodeGif(anim_.width, anim_.height, anim_.frameCount, delay,
                                     [this](int i) { return renderFrame(i + 1); });
    ok = !gif.isEmpty() && f.write(gif) == gif.size();
  } else if (kind == "zip") {
    ZipWriter zip(&f);
    for (int i = 1; i <= anim_.frameCount && ok; ++i)
      ok = zip.addFile(QString("%1_%2.png").arg(base).arg(i, 4, 10, QLatin1Char('0')),
                       pngBytes(renderFrame(i)));
    ok = ok && zip.finish();
  } else if (kind == "mp4") {
    QString error;
    ok = writeMp4(&f, anim_.width, anim_.height, anim_.fps, anim_.frameCount,
                  [this](int i) { return renderFrame(i + 1); }, &error);
    if (!ok) {
      f.close();
      *message = error;
      return false;
    }
  } else {
    *message = QStringLiteral("Formato de exportação desconhecido.");
    return false;
  }
  f.close();
  *message = ok ? QStringLiteral("Exportado com sucesso.") : QStringLiteral("Falha ao gravar o arquivo.");
  return ok;
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
    for (const auto& s : d->strokes) applyStroke(img, s);
  if (layerImgs_.size() >= maxLayerImages()) layerImgs_.clear();
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
  if (cache_.size() >= maxCachedFrames()) cache_.clear();
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
  if (cache_.size() >= maxCachedFrames()) cache_.clear();
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
  if (drawing_) {
    if (live_) p->drawImage(0, 0, liveImg_);  // pincel MyPaint: pintado ponto a ponto
    else brush::paintLive(*p, current_, previewColor(), QRect(0, 0, anim_.width, anim_.height));
  }
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
  const bool usePreset = tool_ == "Preset" && mp::hasPreset(preset_);
  if (usePreset) {
    current_.preset = preset_.toStdString();
    current_.size = float(brushSize_);
  } else if (tool_ == "Ink") {
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
  current_.points.push_back({float(d.x()), float(d.y()), pressure, 0.0f});
  rawLast_ = d;
  rawPressure_ = pressure;
  strokeClock_.start();

  live_.reset();
  if (usePreset) {
    const QSize sz(anim_.width, anim_.height);
    if (liveImg_.size() != sz) liveImg_ = QImage(sz, QImage::Format_ARGB32_Premultiplied);
    liveImg_.fill(Qt::transparent);
    live_ = mp::beginLive(current_, &liveImg_);
    if (!live_) current_.preset.clear();  // sem o motor: cai no pincel interno
  }
  drawing_ = true;
  update();
  return true;
}

void DrawingView::addPoint(const QPointF& docPt, float pressure) {
  const auto& last = current_.points.back();
  if (QLineF(QPointF(last.x, last.y), docPt).length() < 0.6) return;
  const float t = float(strokeClock_.nsecsElapsed() / 1e9);
  current_.points.push_back({float(docPt.x()), float(docPt.y()), pressure, t});
  if (live_) live_->add(current_.points.back());
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
  live_.reset();
  const int layer = activeLayer_, frame = frame_;
  if (anim_.addStroke(layer, frame, current_)) {
    // Pinta só o traço novo na imagem da camada, sem refazer o desenho todo.
    auto it = layerImgs_.find(layerKey(layer, frame));
    if (it != layerImgs_.end()) applyStroke(*it, current_);
    cache_.clear();
    ++revision_;
    emit revisionChanged();
    scheduleSave();
  }
  current_ = arf::Stroke{};
  update();
}

void DrawingView::cancelStroke() {
  if (!drawing_) return;
  drawing_ = false;
  live_.reset();
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
