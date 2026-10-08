#pragma once
#include <QString>

#include "arf/drawing.h"

// Formato de projeto do app (.arf): binário compacto, gravado de forma atômica
// (o arquivo antigo só é trocado quando o novo está completo).
namespace projectio {

bool save(const QString& path, const arf::Animation& anim, const QString& name);
bool load(const QString& path, arf::Animation& anim, QString* name);

}  // namespace projectio
