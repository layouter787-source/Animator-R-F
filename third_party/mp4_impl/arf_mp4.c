#include "arf_mp4.h"

#include <stdlib.h>
#include <string.h>
#include <sys/types.h>

#include "arf_enc.h"

#ifdef _WIN32
#include <stddef.h>
typedef size_t ssize_t;
#endif
#define MINIMP4_IMPLEMENTATION
#include "minimp4.h"

struct ArfMp4 {
  MP4E_mux_t* mux;
  mp4_h26x_writer_t wr;
  ArfEnc* enc;
  int fps;
  int mux_open;
  int wr_open;
};

// Tamanho de um NAL dentro do fluxo Annex-B (inclui o código de início).
static ssize_t nal_size_of(const uint8_t* buf, ssize_t size) {
  ssize_t pos = 3;
  while ((size - pos) > 3) {
    if (buf[pos] == 0 && buf[pos + 1] == 0 && buf[pos + 2] == 1) return pos;
    if (buf[pos] == 0 && buf[pos + 1] == 0 && buf[pos + 2] == 0 && buf[pos + 3] == 1) return pos;
    pos++;
  }
  return size;
}

ArfMp4* arf_mp4_open(int width, int height, int fps, int qp, ArfMp4Write write, void* token) {
  if (!write) return 0;
  ArfMp4* m = (ArfMp4*)calloc(1, sizeof(ArfMp4));
  if (!m) return 0;
  m->fps = fps;

  m->enc = arf_enc_open(width, height, fps, qp);
  if (!m->enc) {
    free(m);
    return 0;
  }

  // Modo sequencial: não precisa voltar atrás no arquivo ao gravar.
  m->mux = MP4E_open(1, 0, token, write);
  m->mux_open = m->mux != 0;
  if (!m->mux_open || MP4E_STATUS_OK != mp4_h26x_write_init(&m->wr, m->mux, width, height, 0)) {
    if (m->mux_open) MP4E_close(m->mux);
    arf_enc_close(m->enc);
    free(m);
    return 0;
  }
  m->wr_open = 1;
  return m;
}

int arf_mp4_add_frame(ArfMp4* m, const uint8_t* y, const uint8_t* u, const uint8_t* v) {
  if (!m) return 1;
  const uint8_t* p = 0;
  int coded_size = 0;
  if (arf_enc_encode(m->enc, y, u, v, &p, &coded_size)) return 2;

  ssize_t left = coded_size;
  while (left > 0) {
    const ssize_t n = nal_size_of(p, left);
    if (n < 4) {
      p += 1;
      left -= 1;
      continue;
    }
    if (MP4E_STATUS_OK != mp4_h26x_write_nal(&m->wr, p, (int)n, 90000 / m->fps)) return 3;
    p += n;
    left -= n;
  }
  return 0;
}

int arf_mp4_close(ArfMp4* m) {
  if (!m) return 1;
  int rc = 0;
  if (m->mux_open && MP4E_STATUS_OK != MP4E_close(m->mux)) rc = 2;
  if (m->wr_open) mp4_h26x_write_close(&m->wr);
  arf_enc_close(m->enc);
  free(m);
  return rc;
}
