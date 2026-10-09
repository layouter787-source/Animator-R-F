#pragma once
#include <QIODevice>
#include <QImage>
#include <QString>
#include <functional>

// Exporta vídeo MP4 (H.264). As dimensões são ajustadas para múltiplos de 16 (corte central de
// poucos pixels), como o codificador exige. frameAt(i) devolve o quadro i (0-based) em RGB32.
bool mp4Available();
bool writeMp4(QIODevice* out, int width, int height, int fps, int frameCount,
              const std::function<QImage(int)>& frameAt, QString* error);
