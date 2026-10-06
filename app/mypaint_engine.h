#pragma once
#include <QImage>
#include <QString>
#include <QVariantList>
#include <memory>

#include "arf/drawing.h"

// Ponte entre o app e o libmypaint (motor de pincéis do MyPaint, licença ISC).
// Os pincéis (.myb, CC0) ficam embutidos em ":/brushes".
namespace mp {

// true se este build inclui o motor MyPaint.
bool available();

// Lista de pincéis: [{id, name, group, preview}], ordenada por grupo e nome.
QVariantList presets();

bool hasPreset(const QString& id);

// Refaz um traço completo sobre a imagem da camada (usado ao compor e no desfazer).
void paintStroke(QImage& layer, const arf::Stroke& s);

// Traço em andamento: cada ponto novo pinta direto na imagem de destino.
class Live {
public:
  virtual ~Live() = default;
  virtual void add(const arf::Point& p) = 0;
};
std::unique_ptr<Live> beginLive(const arf::Stroke& s, QImage* target);

}  // namespace mp
