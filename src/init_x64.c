#include "arena.h"

uint8_t sg_static_arena_buf[SG_STATIC_ARENA_BYTES] __attribute__((aligned(64)));
SgOutBuf sg_outbuf_global;

#define SG_STATIC_IN_BYTES (16u * 1024u * 1024u)
static uint8_t sg_static_in_buf[SG_STATIC_IN_BYTES + 128] __attribute__((aligned(64)));

void sg_solve_all_avx2(uint8_t *in_buf, size_t in_len, SgOutBuf *ob);
void sg_solve_all_sse2(uint8_t *in_buf, size_t in_len, SgOutBuf *ob);

__attribute__((visibility("hidden")))
void *memcpy(void *dst, const void *src, size_t n) {
    return sg_memcpy(dst, src, n);
}

__attribute__((visibility("hidden")))
void *memset(void *dst, int c, size_t n) {
    return sg_memset(dst, c, n);
}

__attribute__((visibility("hidden")))
void *memmove(void *dst, const void *src, size_t n) {
    return sg_memmove(dst, src, n);
}

__attribute__((visibility("hidden")))
int memcmp(const void *a, const void *b, size_t n) {
    return sg_memcmp(a, b, n);
}

__attribute__((visibility("hidden")))
size_t strlen(const char *s) {
    return sg_strlen(s);
}

static inline void sg_cpuid(uint32_t leaf, uint32_t sub, uint32_t *eax, uint32_t *ebx, uint32_t *ecx, uint32_t *edx) {
    __asm__ volatile("cpuid"
        : "=a"(*eax), "=b"(*ebx), "=c"(*ecx), "=d"(*edx)
        : "a"(leaf), "c"(sub));
}

static inline uint64_t sg_xgetbv0(void) {
    uint32_t eax, edx;
    __asm__ volatile("xgetbv" : "=a"(eax), "=d"(edx) : "c"(0));
    return ((uint64_t)edx << 32) | eax;
}

static bool sg_has_avx2_bmi2_popcnt(void) {
    uint32_t a, b, c, d;
    sg_cpuid(0, 0, &a, &b, &c, &d);
    if (a < 7) return false;
    sg_cpuid(1, 0, &a, &b, &c, &d);
    /* bit 23 POPCNT, bit 27 OSXSAVE, bit 28 AVX */
    const uint32_t req1 = (1u << 23) | (1u << 27) | (1u << 28);
    if ((c & req1) != req1) return false;
    if ((sg_xgetbv0() & 6u) != 6u) return false;
    sg_cpuid(7, 0, &a, &b, &c, &d);
    /* bit 3 BMI1, bit 5 AVX2, bit 8 BMI2 */
    const uint32_t req7 = (1u << 3) | (1u << 5) | (1u << 8);
    if ((b & req7) != req7) return false;
    /* Check LZCNT (leaf 0x80000001 ecx bit 5) if used */
    sg_cpuid(0x80000000u, 0, &a, &b, &c, &d);
    if (a < 0x80000001u) return false;
    sg_cpuid(0x80000001u, 0, &a, &b, &c, &d);
    if ((c & (1u << 5)) == 0) return false;
    return true;
}

__attribute__((constructor, visibility("hidden")))
static void sg_init_x64(void) {
    int in_fd = sg_open_ro("/input/cases.json");
    const char *out_path = "/output/answers.json";
    char in_path_buf[512];
    char out_path_buf[512];

    if (SG_UNLIKELY(in_fd < 0)) {
        char in_dir[384], out_dir[384];
        sg_read_env_dirs(in_dir, sizeof(in_dir), out_dir, sizeof(out_dir));
        if (in_dir[0] == '\0' || out_dir[0] == '\0') return;
        size_t idl = sg_strlen(in_dir);
        sg_memcpy(in_path_buf, in_dir, idl);
        sg_memcpy(in_path_buf + idl, "/cases.json", 12);
        in_fd = sg_open_ro(in_path_buf);
        if (in_fd < 0) return;

        size_t odl = sg_strlen(out_dir);
        sg_memcpy(out_path_buf, out_dir, odl);
        sg_memcpy(out_path_buf + odl, "/answers.json", 14);
        out_path = out_path_buf;
    }

    int64_t fsz = sg_lseek_end(in_fd);
    if (fsz <= 0) { sg_close(in_fd); return; }
    size_t len = (size_t)fsz;
    uint8_t *in_buf = sg_static_in_buf;
    bool mapped = false;
    if (SG_UNLIKELY(len > SG_STATIC_IN_BYTES)) {
        in_buf = (uint8_t *)sg_mmap_anon(len + 128);
        if (!in_buf) { sg_close(in_fd); sg_exit(3); }
        mapped = true;
    }
    if (sg_pread_all(in_fd, in_buf, len) < 0) {
        sg_close(in_fd);
        if (mapped) sg_munmap(in_buf, len + 128);
        sg_exit(4);
    }
    sg_close(in_fd);
    sg_memset(in_buf + len, 0, 64);

    int out_fd = sg_open_wo(out_path);
    if (out_fd < 0) {
        if (mapped) sg_munmap(in_buf, len + 128);
        sg_exit(5);
    }

    sg_outbuf_global.fd = out_fd;
    sg_outbuf_global.pos = 0;

    if (sg_has_avx2_bmi2_popcnt()) {
        sg_solve_all_avx2(in_buf, len, &sg_outbuf_global);
    } else {
        sg_solve_all_sse2(in_buf, len, &sg_outbuf_global);
    }

    sg_close(out_fd);
    if (mapped) sg_munmap(in_buf, len + 128);
    sg_exit(0);
}
