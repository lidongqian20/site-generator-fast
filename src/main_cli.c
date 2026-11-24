#define SG_WINDOWS_CLI 1
#define SG_USE_AVX2 1
#include "engine_impl.h"

uint8_t sg_static_arena_buf[SG_STATIC_ARENA_BYTES] __attribute__((aligned(64)));
SgOutBuf sg_outbuf_global;

int main(int argc, char **argv) {
    if (argc < 3) {
        fprintf(stderr, "Usage: %s <cases.json> <answers.json> [bench_iters]\n", argv[0]);
        return 1;
    }
    FILE *fi = fopen(argv[1], "rb");
    if (!fi) { perror("fopen in"); return 2; }
    fseek(fi, 0, SEEK_END);
    long sz = ftell(fi);
    fseek(fi, 0, SEEK_SET);
    size_t len = (size_t)sz;
    uint8_t *in_buf = (uint8_t *)calloc(len + 128, 1);
    if (!in_buf) return 3;
    if (fread(in_buf, 1, len, fi) != len) { fclose(fi); return 4; }
    fclose(fi);

    int iters = (argc >= 4) ? atoi(argv[3]) : 1;
    for (int it = 0; it < iters; it++) {
        FILE *fo = fopen(argv[2], "wb");
        if (!fo) { perror("fopen out"); return 5; }
        sg_outbuf_global.pos = 0;
        sg_outbuf_global.fp = fo;
        sg_solve_all_avx2(in_buf, len, &sg_outbuf_global);
        fclose(fo);
    }
    free(in_buf);
    return 0;
}
