#pragma once
#include <QColor>
#include <QElapsedTimer>
#include <QHash>
#include <QImage>
#include <QPointF>
#include <QQuickPaintedItem>
#include <QString>
#include <QStringList>
#include <QTimer>
#include <QTransform>
#include <QUrl>
#include <QVariantList>
#include <QtQml/qqmlregistration.h>

#include <memory>

#include "arf/drawing.h"
#include "mypaint_engine.h"

// Canvas de animação: desenha traços nas camadas, mostra onion skin e reproduz.
// Gestos: 1 dedo/caneta desenha; 2 dedos fazem zoom, pan e rotação.
class DrawingView : public QQuickPaintedItem {
  Q_OBJECT
  QML_ELEMENT
  Q_PROPERTY(int frame READ frame WRITE setFrame NOTIFY frameChanged)
  Q_PROPERTY(int frameCount READ frameCount NOTIFY projectChanged)
  Q_PROPERTY(QString projectName READ projectName NOTIFY projectChanged)
  Q_PROPERTY(bool busy READ busy NOTIFY busyChanged)
  Q_PROPERTY(int activeLayer READ activeLayer WRITE setActiveLayer NOTIFY layersChanged)
  Q_PROPERTY(QStringList layerNames READ layerNames NOTIFY layersChanged)
  Q_PROPERTY(QString tool READ tool WRITE setTool NOTIFY toolChanged)
  Q_PROPERTY(QString preset READ preset WRITE setPreset NOTIFY presetChanged)
  Q_PROPERTY(QVariantList presets READ presets CONSTANT)
  Q_PROPERTY(QColor color READ color WRITE setColor NOTIFY colorChanged)
  Q_PROPERTY(qreal brushSize READ brushSize WRITE setBrushSize NOTIFY brushSizeChanged)
  Q_PROPERTY(qreal brushOpacity READ brushOpacity WRITE setBrushOpacity NOTIFY brushOpacityChanged)
  Q_PROPERTY(qreal stabilizer READ stabilizer WRITE setStabilizer NOTIFY stabilizerChanged)
  Q_PROPERTY(bool smoothStrokes READ smoothStrokes WRITE setSmoothStrokes NOTIFY smoothStrokesChanged)
  Q_PROPERTY(bool onionSkin READ onionSkin WRITE setOnionSkin NOTIFY onionSkinChanged)
  Q_PROPERTY(bool playing READ playing NOTIFY playingChanged)
  Q_PROPERTY(bool canUndo READ canUndo NOTIFY revisionChanged)
  Q_PROPERTY(bool canRedo READ canRedo NOTIFY revisionChanged)
  Q_PROPERTY(int revision READ revision NOTIFY revisionChanged)
  Q_PROPERTY(qreal zoom READ zoom NOTIFY viewChanged)

public:
  explicit DrawingView(QQuickItem* parent = nullptr);
  ~DrawingView() override;

  int frame() const { return frame_; }
  int frameCount() const { return anim_.frameCount; }
  QString projectName() const { return projectName_; }
  bool busy() const { return busy_; }
  int activeLayer() const { return activeLayer_; }
  QStringList layerNames() const;
  QString tool() const { return tool_; }
  QString preset() const { return preset_; }
  QVariantList presets() const { return mp::presets(); }
  QColor color() const { return color_; }
  qreal brushSize() const { return brushSize_; }
  qreal brushOpacity() const { return brushOpacity_; }
  qreal stabilizer() const { return stabilizer_; }
  bool smoothStrokes() const { return smoothStrokes_; }
  bool onionSkin() const { return onion_; }
  bool playing() const { return playing_; }
  bool canUndo() const { return anim_.canUndo(); }
  bool canRedo() const { return anim_.canRedo(); }
  int revision() const { return revision_; }
  qreal zoom() const { return zoom_; }

  void setFrame(int f);
  void setActiveLayer(int i);
  void setTool(const QString& t);
  void setPreset(const QString& id);
  void setColor(const QColor& c);
  void setBrushSize(qreal s);
  void setBrushOpacity(qreal o);
  void setStabilizer(qreal s);
  void setSmoothStrokes(bool on);
  void setOnionSkin(bool on);

  Q_INVOKABLE int addLayer();
  Q_INVOKABLE void toggleLayerVisible(int index);
  Q_INVOKABLE bool layerVisible(int index) const;
  Q_INVOKABLE bool hasKey(int frame) const;
  Q_INVOKABLE void insertBlankKey();
  Q_INVOKABLE void undo();
  Q_INVOKABLE void redo();
  Q_INVOKABLE void togglePlay();
  Q_INVOKABLE void resetView();

  // Projeto: abrir, salvar (também é chamado sozinho depois de cada alteração) e exportar.
  Q_INVOKABLE bool openProject(const QString& id);
  Q_INVOKABLE void saveNow();
  Q_INVOKABLE void exportAs(const QString& kind, const QUrl& url);  // "png", "gif" ou "zip"

  void paint(QPainter* painter) override;

signals:
  void frameChanged();
  void projectChanged();
  void busyChanged();
  void exportFinished(bool ok, const QString& message);
  void layersChanged();
  void toolChanged();
  void presetChanged();
  void colorChanged();
  void brushSizeChanged();
  void brushOpacityChanged();
  void stabilizerChanged();
  void smoothStrokesChanged();
  void onionSkinChanged();
  void playingChanged();
  void revisionChanged();
  void viewChanged();

protected:
  void mousePressEvent(QMouseEvent* e) override;
  void mouseMoveEvent(QMouseEvent* e) override;
  void mouseReleaseEvent(QMouseEvent* e) override;
  void touchEvent(QTouchEvent* e) override;
  void wheelEvent(QWheelEvent* e) override;

private:
  QTransform viewTransform() const;
  qreal baseScale() const;
  QPointF toDoc(const QPointF& p) const;
  void zoomAbout(const QPointF& center, qreal factor);

  bool beginStroke(const QPointF& pos, float pressure);
  void extendStroke(const QPointF& pos, float pressure);
  void addPoint(const QPointF& docPt, float pressure);
  void finishStabilizer();
  void endStroke();
  void cancelStroke();
  QColor previewColor() const;

  QImage layerImage(int layer, int frame);
  QImage compose(int frame);
  QImage onionImage(int frame, const QColor& tint);
  QImage renderFrame(int frame);  // quadro final sobre fundo branco
  void invalidate(bool layersToo = false);
  void scheduleSave();
  bool doExport(const QString& kind, const QUrl& url, QString* message);
  int maxCachedFrames() const;
  int maxLayerImages() const;

  arf::Animation anim_;
  arf::Stroke current_;
  std::unique_ptr<mp::Live> live_;           // pincel MyPaint em andamento
  QImage liveImg_;
  QElapsedTimer strokeClock_;
  QHash<int, QImage> cache_;                 // quadros já compostos (e onion skin)
  QHash<quint64, QImage> layerImgs_;         // imagem de cada (camada, quadro-chave)
  QTimer playTimer_;
  QTimer saveTimer_;
  QString projectId_;
  QString projectName_;
  bool busy_ = false;
  int frame_ = 1;
  int activeLayer_ = 0;
  int revision_ = 0;
  QString tool_ = "Pencil";
  QString preset_;
  QColor color_ = QColor("#111111");
  qreal brushSize_ = 6;
  qreal brushOpacity_ = 1.0;
  qreal stabilizer_ = 0.35;
  bool smoothStrokes_ = true;  // sempre ligado por padrão; desligar deixa o traço pixelado
  bool onion_ = true;
  bool playing_ = false;
  bool drawing_ = false;
  QPointF rawLast_;
  float rawPressure_ = 1.0f;

  // Vista (zoom/pan/rotação), relativa ao ajuste automático da página na tela.
  qreal zoom_ = 1.0;
  qreal rotation_ = 0.0;  // graus
  QPointF pan_;
  bool gesture_ = false;
  bool panning_ = false;
  QPointF lastMouse_;
};
