#include "arf_mp4.h"

#include <stdlib.h>
#include <string.h>
#include <sys/types.h>

#define MINIH264_IMPLEMENTATION
#include "minih264e.h"

#ifdef _WIN32
#include <stddef.h>
typedef size_t ssize_t;
#endif
#define MINIMP4_IMPLEMENTATION
#include "minimp4.h"

struct ArfMp4 {
  MP4E_mux_t* mux;
  mp4_h26x_writer_t wr;
  H264E_persist_t* enc;
  H264E_scratch_t* scratch;
  void* enc_raw;      // ponteiros originais (antes do alinhamento)
  void* scratch_raw;
  H264E_create_param_t cp;
  H264E_run_param_t rp;
  int width, height, fps, qp;
  int mux_open;
  int wr_open;
};

static void* aligned_alloc64(size_t size, void** raw) {
  uint8_t* p = (uint8_t*)malloc(size + 64);
  *raw = p;
  if (!p) return 0;
  return (void*)(((uintptr_t)p + 63) & ~(uintptr_t)63);
}

// Tamanho de um NAL dentro do fluxo Annex-B (inclui o código de início).
static ssize_t nal_size(const uint8_t* buf, ssize_t size) {
  ssize_t pos = 3;
  while ((size - pos) > 3) {
    if (buf[pos] == 0 && buf[pos + 1] == 0 && buf[pos + 2] == 1) return pos;
    if (buf[pos] == 0 && buf[pos + 1] == 0 && buf[pos + 2] == 0 && buf[pos + 3] == 1) return pos;
    pos++;
  }
  return size;
}

ArfMp4* arf_mp4_open(int width, int height, int fps, int qp, ArfMp4Write write, void* token) {
  if (width < 16 || height < 16 || (width & 15) || (height & 15) || fps < 1 || !write) return 0;

  ArfMp4* m = (ArfMp4*)calloc(1, sizeof(ArfMp4));
  if (!m) return 0;
  m->width = width;
  m->height = height;
  m->fps = fps;
  m->qp = qp < 10 ? 10 : (qp > 51 ? 51 : qp);

  m->cp.enableNEON = 1;
#if H264E_SVC_API
  m->cp.num_layers = 1;
  m->cp.inter_layer_pred_flag = 0;
#endif
  m->cp.gop = fps * 2;
  m->cp.width = width;
  m->cp.height = height;
  m->cp.max_long_term_reference_frames = 0;
  m->cp.fine_rate_control_flag = 0;
  m->cp.const_input_flag = 1;
  m->cp.temporal_denoise_flag = 0;

  int sizeof_persist = 0, sizeof_scratch = 0;
  if (H264E_sizeof(&m->cp, &sizeof_persist, &sizeof_scratch)) {
    free(m);
    return 0;
  }
  m->enc = (H264E_persist_t*)aligned_alloc64((size_t)sizeof_persist, &m->enc_raw);
  m->scratch = (H264E_scratch_t*)aligned_alloc64((size_t)sizeof_scratch, &m->scratch_raw);
  if (!m->enc || !m->scratch || H264E_init(m->enc, &m->cp)) {
    free(m->enc_raw);
    free(m->scratch_raw);
    free(m);
    return 0;
  }

  // Modo sequencial: não precisa voltar atrás no arquivo ao gravar.
  m->mux = MP4E_open(1, 0, token, write);
  m->mux_open = m->mux != 0;
  if (!m->mux_open || MP4E_STATUS_OK != mp4_h26x_write_init(&m->wr, m->mux, width, height, 0)) {
    if (m->mux_open) MP4E_close(m->mux);
    free(m->enc_raw);
    free(m->scratch_raw);
    free(m);
    return 0;
  }
  m->wr_open = 1;
  return m;
}

int arf_mp4_add_frame(ArfMp4* m, const uint8_t* y, const uint8_t* u, const uint8_t* v) {
  if (!m) return 1;
  H264E_io_yuv_t yuv;
  memset(&yuv, 0, sizeof(yuv));
  yuv.yuv[0] = (unsigned char*)y;
  yuv.stride[0] = m->width;
  yuv.yuv[1] = (unsigned char*)u;
  yuv.stride[1] = m->width / 2;
  yuv.yuv[2] = (unsigned char*)v;
  yuv.stride[2] = m->width / 2;

  m->rp.frame_type = 0;
  m->rp.encode_speed = 3;
  m->rp.qp_min = m->rp.qp_max = m->qp;

  unsigned char* coded = 0;
  int coded_size = 0;
  if (H264E_encode(m->enc, m->scratch, &m->rp, &yuv, &coded, &coded_size)) return 2;

  const uint8_t* p = coded;
  ssize_t left = coded_size;
  while (left > 0) {
    const ssize_t n = nal_size(p, left);
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
  free(m->enc_raw);
  free(m->scratch_raw);
  free(m);
  return rc;
}
