#pragma once
#include <QColor>
#include <QHash>
#include <QImage>
#include <QPointF>
#include <QQuickPaintedItem>
#include <QString>
#include <QStringList>
#include <QTimer>
#include <QTransform>
#include <QtQml/qqmlregistration.h>

#include "arf/drawing.h"

// Canvas de animação: desenha traços nas camadas, mostra onion skin e reproduz.
// Gestos: 1 dedo/caneta desenha; 2 dedos fazem zoom, pan e rotação.
class DrawingView : public QQuickPaintedItem {
  Q_OBJECT
  QML_ELEMENT
  Q_PROPERTY(int frame READ frame WRITE setFrame NOTIFY frameChanged)
  Q_PROPERTY(int frameCount READ frameCount CONSTANT)
  Q_PROPERTY(int activeLayer READ activeLayer WRITE setActiveLayer NOTIFY layersChanged)
  Q_PROPERTY(QStringList layerNames READ layerNames NOTIFY layersChanged)
  Q_PROPERTY(QString tool READ tool WRITE setTool NOTIFY toolChanged)
  Q_PROPERTY(QColor color READ color WRITE setColor NOTIFY colorChanged)
  Q_PROPERTY(qreal brushSize READ brushSize WRITE setBrushSize NOTIFY brushSizeChanged)
  Q_PROPERTY(bool onionSkin READ onionSkin WRITE setOnionSkin NOTIFY onionSkinChanged)
  Q_PROPERTY(bool playing READ playing NOTIFY playingChanged)
  Q_PROPERTY(bool canUndo READ canUndo NOTIFY revisionChanged)
  Q_PROPERTY(bool canRedo READ canRedo NOTIFY revisionChanged)
  Q_PROPERTY(int revision READ revision NOTIFY revisionChanged)
  Q_PROPERTY(qreal zoom READ zoom NOTIFY viewChanged)

public:
  explicit DrawingView(QQuickItem* parent = nullptr);

  int frame() const { return frame_; }
  int frameCount() const { return anim_.frameCount; }
  int activeLayer() const { return activeLayer_; }
  QStringList layerNames() const;
  QString tool() const { return tool_; }
  QColor color() const { return color_; }
  qreal brushSize() const { return brushSize_; }
  bool onionSkin() const { return onion_; }
  bool playing() const { return playing_; }
  bool canUndo() const { return anim_.canUndo(); }
  bool canRedo() const { return anim_.canRedo(); }
  int revision() const { return revision_; }
  qreal zoom() const { return zoom_; }

  void setFrame(int f);
  void setActiveLayer(int i);
  void setTool(const QString& t);
  void setColor(const QColor& c);
  void setBrushSize(qreal s);
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

  void paint(QPainter* painter) override;

signals:
  void frameChanged();
  void layersChanged();
  void toolChanged();
  void colorChanged();
  void brushSizeChanged();
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
  void endStroke();
  void cancelStroke();
  QImage compose(int frame);
  QImage onionImage(int frame, const QColor& tint);
  void invalidate();

  arf::Animation anim_;
  arf::Stroke current_;
  QHash<int, QImage> cache_;
  QTimer playTimer_;
  int frame_ = 1;
  int activeLayer_ = 0;
  int revision_ = 0;
  QString tool_ = "Pencil";
  QColor color_ = QColor("#111111");
  qreal brushSize_ = 6;
  bool onion_ = true;
  bool playing_ = false;
  bool drawing_ = false;

  // Vista (zoom/pan/rotação), relativa ao ajuste automático da página na tela.
  qreal zoom_ = 1.0;
  qreal rotation_ = 0.0;  // graus
  QPointF pan_;
  bool gesture_ = false;
  bool panning_ = false;
  QPointF lastMouse_;
};
