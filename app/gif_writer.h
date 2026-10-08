#pragma once
#include <QByteArray>
#include <QImage>
#include <functional>

// Codificador de GIF animado (paleta adaptativa de 256 cores + LZW). Pede um quadro de cada vez,
// então a memória não cresce com o tamanho da animação.
// frameAt(i) deve devolver o quadro i (0-based) em RGB32, sempre do mesmo tamanho.
QByteArray encodeGif(int width, int height, int frameCount, int delayCentiseconds,
                     const std::function<QImage(int)>& frameAt);
