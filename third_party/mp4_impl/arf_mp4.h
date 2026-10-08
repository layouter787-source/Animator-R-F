#pragma once
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// Codificador de vídeo MP4 (H.264) simples: minih264 (CC0) + minimp4 (CC0).
// As dimensões precisam ser múltiplas de 16.

typedef struct ArfMp4 ArfMp4;

// Função de gravação: devolve 0 se deu certo.
typedef int (*ArfMp4Write)(int64_t offset, const void* data, size_t size, void* token);

// qp: qualidade (10 = melhor, 51 = pior); 24 a 28 é um bom intervalo para desenho.
ArfMp4* arf_mp4_open(int width, int height, int fps, int qp, ArfMp4Write write, void* token);

// Planos I420 (Y inteiro; U e V com metade da largura e da altura). Devolve 0 se deu certo.
int arf_mp4_add_frame(ArfMp4* m, const uint8_t* y, const uint8_t* u, const uint8_t* v);

// Finaliza o arquivo e libera a memória. Devolve 0 se deu certo.
int arf_mp4_close(ArfMp4* m);

#ifdef __cplusplus
}
#endif
