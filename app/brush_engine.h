#pragma once
#include <QColor>
#include <QImage>
#include <QPainter>
#include <QRect>
#include <QRectF>

#include "arf/drawing.h"

// Motor de pincéis: cada traço vira UMA forma contínua (uma fita de largura variável com
// pontas arredondadas), preenchida de uma só vez. Não há "bolinhas": a borda é uma curva
// única e lisa, a opacidade é uniforme e a largura acompanha a pressão.
namespace brush {

// Ajusta o QPainter conforme o traço (anti-serrilhado ligado/desligado).
void configure(QPainter& p, const arf::Stroke& s);

// Área ocupada pelo traço (em coordenadas do desenho).
QRectF strokeBounds(const arf::Stroke& s);

// Desenha o traço inteiro, opaco, no painter. `finalEnd` = false desliga o afinamento do fim.
void paintStroke(QPainter& p, const arf::Stroke& s, const QColor& color, bool finalEnd = true);

// Pré-visualização do traço em andamento (respeita a opacidade do pincel).
void paintLive(QPainter& p, const arf::Stroke& s, const QColor& color, const QRect& docRect);

// Aplica um traço pronto sobre a imagem de uma camada (borracha, opacidade e mistura).
void compositeStroke(QImage& layer, const arf::Stroke& s);

}  // namespace brush
