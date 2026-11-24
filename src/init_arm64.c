#include "arena.h"

uint8_t sg_static_arena_buf[SG_STATIC_ARENA_BYTES] __attribute__((aligned(64)));
SgOutBuf sg_outbuf_global;

#define SG_STATIC_IN_BYTES (16u * 1024u * 1024u)
static uint8_t sg_static_in_buf[SG_STATIC_IN_BYTES + 128] __attribute__((aligned(64)));

void sg_solve_all_neon(uint8_t *in_buf, size_t in_len, SgOutBuf *ob);

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

__attribute__((constructor, visibility("hidden")))
static void sg_init_arm64(void) {
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

    sg_solve_all_neon(in_buf, len, &sg_outbuf_global);

    sg_close(out_fd);
    if (mapped) sg_munmap(in_buf, len + 128);
    sg_exit(0);
}
