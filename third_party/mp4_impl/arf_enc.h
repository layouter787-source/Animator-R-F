#pragma once
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// Codificador H.264 (minih264). Fica num arquivo separado do gravador de MP4 porque os dois
// cabeçalhos definem funções com o mesmo nome e não podem ser compilados juntos.

typedef struct ArfEnc ArfEnc;

// Dimensões múltiplas de 16. qp: 10 (melhor) a 51 (pior).
ArfEnc* arf_enc_open(int width, int height, int fps, int qp);

// Codifica um quadro I420. Devolve 0 se deu certo; *out aponta para o fluxo Annex-B do quadro
// (válido até a próxima chamada).
int arf_enc_encode(ArfEnc* e, const uint8_t* y, const uint8_t* u, const uint8_t* v,
                   const uint8_t** out, int* out_size);

void arf_enc_close(ArfEnc* e);

#ifdef __cplusplus
}
#endif
