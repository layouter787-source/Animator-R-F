#pragma once
#include <QColor>
#include <QImage>
#include <QPainter>
#include <QRectF>
#include <vector>

#include "arf/drawing.h"

// Motor de pincéis: carimba "gotas" (dabs) redondas ao longo de uma curva suave
// (Catmull-Rom), com largura que acompanha a pressão e afinamento nas pontas (tinta).
namespace brush {

// Ajusta o QPainter conforme o traço (anti-serrilhado ligado/desligado).
void configure(QPainter& p, const arf::Stroke& s);

// Comprimento acumulado do traço em cada ponto.
std::vector<double> cumulativeLength(const arf::Stroke& s);

// Área ocupada pelo traço (em coordenadas do desenho).
QRectF strokeBounds(const arf::Stroke& s);

// Carimba o segmento i -> i+1. `total` < 0 desliga o afinamento do fim (pré-visualização).
void paintSegment(QPainter& p, const arf::Stroke& s, int i, const std::vector<double>& cum,
                  double total, const QColor& color);

// Uma gota no primeiro ponto (toque simples ou início da pré-visualização).
void paintDot(QPainter& p, const arf::Stroke& s, const QColor& color);

// Desenha o traço inteiro, opaco, no painter.
void paintStroke(QPainter& p, const arf::Stroke& s, const QColor& color);

// Aplica um traço pronto sobre a imagem de uma camada (borracha, opacidade e mistura).
void compositeStroke(QImage& layer, const arf::Stroke& s);

}  // namespace brush
