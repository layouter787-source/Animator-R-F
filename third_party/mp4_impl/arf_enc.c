#include "arf_enc.h"

#include <stdlib.h>
#include <string.h>

#define MINIH264_IMPLEMENTATION
#include "minih264e.h"

struct ArfEnc {
  H264E_persist_t* enc;
  H264E_scratch_t* scratch;
  void* enc_raw;      // ponteiros originais (antes do alinhamento)
  void* scratch_raw;
  H264E_create_param_t cp;
  H264E_run_param_t rp;
  int width, qp;
};

static void* aligned_alloc64(size_t size, void** raw) {
  uint8_t* p = (uint8_t*)malloc(size + 64);
  *raw = p;
  if (!p) return 0;
  return (void*)(((uintptr_t)p + 63) & ~(uintptr_t)63);
}

ArfEnc* arf_enc_open(int width, int height, int fps, int qp) {
  if (width < 16 || height < 16 || (width & 15) || (height & 15) || fps < 1) return 0;

  ArfEnc* e = (ArfEnc*)calloc(1, sizeof(ArfEnc));
  if (!e) return 0;
  e->width = width;
  e->qp = qp < 10 ? 10 : (qp > 51 ? 51 : qp);

  e->cp.enableNEON = 1;
#if H264E_SVC_API
  e->cp.num_layers = 1;
  e->cp.inter_layer_pred_flag = 0;
#endif
  e->cp.gop = fps * 2;
  e->cp.width = width;
  e->cp.height = height;
  e->cp.max_long_term_reference_frames = 0;
  e->cp.fine_rate_control_flag = 0;
  e->cp.const_input_flag = 1;
  e->cp.temporal_denoise_flag = 0;

  int sizeof_persist = 0, sizeof_scratch = 0;
  if (H264E_sizeof(&e->cp, &sizeof_persist, &sizeof_scratch)) {
    free(e);
    return 0;
  }
  e->enc = (H264E_persist_t*)aligned_alloc64((size_t)sizeof_persist, &e->enc_raw);
  e->scratch = (H264E_scratch_t*)aligned_alloc64((size_t)sizeof_scratch, &e->scratch_raw);
  if (!e->enc || !e->scratch || H264E_init(e->enc, &e->cp)) {
    free(e->enc_raw);
    free(e->scratch_raw);
    free(e);
    return 0;
  }
  return e;
}

int arf_enc_encode(ArfEnc* e, const uint8_t* y, const uint8_t* u, const uint8_t* v,
                   const uint8_t** out, int* out_size) {
  if (!e) return 1;
  H264E_io_yuv_t yuv;
  memset(&yuv, 0, sizeof(yuv));
  yuv.yuv[0] = (unsigned char*)y;
  yuv.stride[0] = e->width;
  yuv.yuv[1] = (unsigned char*)u;
  yuv.stride[1] = e->width / 2;
  yuv.yuv[2] = (unsigned char*)v;
  yuv.stride[2] = e->width / 2;

  e->rp.frame_type = 0;
  e->rp.encode_speed = 3;
  e->rp.qp_min = e->rp.qp_max = e->qp;

  unsigned char* coded = 0;
  int coded_size = 0;
  if (H264E_encode(e->enc, e->scratch, &e->rp, &yuv, &coded, &coded_size)) return 2;
  *out = coded;
  *out_size = coded_size;
  return 0;
}

void arf_enc_close(ArfEnc* e) {
  if (!e) return;
  free(e->enc_raw);
  free(e->scratch_raw);
  free(e);
}
