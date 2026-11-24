#pragma once
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#if defined(__GNUC__) || defined(__clang__)
#define SG_INLINE static inline __attribute__((always_inline))
#define SG_NOINLINE __attribute__((noinline))
#define SG_LIKELY(x) __builtin_expect(!!(x), 1)
#define SG_UNLIKELY(x) __builtin_expect(!!(x), 0)
#else
#define SG_INLINE static inline
#define SG_NOINLINE
#define SG_LIKELY(x) (x)
#define SG_UNLIKELY(x) (x)
#endif

#ifndef SG_WINDOWS_CLI
/* Freestanding Linux implementation using direct syscalls */

#if defined(__x86_64__)
#define SG_SYS_read       0
#define SG_SYS_write      1
#define SG_SYS_close      3
#define SG_SYS_lseek      8
#define SG_SYS_mmap       9
#define SG_SYS_munmap     11
#define SG_SYS_pread64    17
#define SG_SYS_fchmod     91
#define SG_SYS_exit_group 231
#define SG_SYS_openat     257

static inline int64_t sg_syscall1(int64_t n, int64_t a1) {
    int64_t r;
    __asm__ volatile("syscall" : "=a"(r) : "a"(n), "D"(a1) : "rcx", "r11", "memory");
    return r;
}
static inline int64_t sg_syscall2(int64_t n, int64_t a1, int64_t a2) {
    int64_t r;
    __asm__ volatile("syscall" : "=a"(r) : "a"(n), "D"(a1), "S"(a2) : "rcx", "r11", "memory");
    return r;
}
static inline int64_t sg_syscall3(int64_t n, int64_t a1, int64_t a2, int64_t a3) {
    int64_t r;
    __asm__ volatile("syscall" : "=a"(r) : "a"(n), "D"(a1), "S"(a2), "d"(a3) : "rcx", "r11", "memory");
    return r;
}
static inline int64_t sg_syscall4(int64_t n, int64_t a1, int64_t a2, int64_t a3, int64_t a4) {
    int64_t r;
    register int64_t r10 __asm__("r10") = a4;
    __asm__ volatile("syscall" : "=a"(r) : "a"(n), "D"(a1), "S"(a2), "d"(a3), "r"(r10) : "rcx", "r11", "memory");
    return r;
}
static inline int64_t sg_syscall6(int64_t n, int64_t a1, int64_t a2, int64_t a3, int64_t a4, int64_t a5, int64_t a6) {
    int64_t r;
    register int64_t r10 __asm__("r10") = a4;
    register int64_t r8  __asm__("r8")  = a5;
    register int64_t r9  __asm__("r9")  = a6;
    __asm__ volatile("syscall" : "=a"(r) : "a"(n), "D"(a1), "S"(a2), "d"(a3), "r"(r10), "r"(r8), "r"(r9) : "rcx", "r11", "memory");
    return r;
}

#elif defined(__aarch64__)
#define SG_SYS_fchmod     52
#define SG_SYS_openat     56
#define SG_SYS_close      57
#define SG_SYS_lseek      62
#define SG_SYS_read       63
#define SG_SYS_write      64
#define SG_SYS_pread64    67
#define SG_SYS_exit_group 94
#define SG_SYS_munmap     215
#define SG_SYS_mmap       222

static inline int64_t sg_syscall1(int64_t n, int64_t a1) {
    register int64_t x8 __asm__("x8") = n;
    register int64_t x0 __asm__("x0") = a1;
    __asm__ volatile("svc #0" : "+r"(x0) : "r"(x8) : "memory");
    return x0;
}
static inline int64_t sg_syscall2(int64_t n, int64_t a1, int64_t a2) {
    register int64_t x8 __asm__("x8") = n;
    register int64_t x0 __asm__("x0") = a1;
    register int64_t x1 __asm__("x1") = a2;
    __asm__ volatile("svc #0" : "+r"(x0) : "r"(x8), "r"(x1) : "memory");
    return x0;
}
static inline int64_t sg_syscall3(int64_t n, int64_t a1, int64_t a2, int64_t a3) {
    register int64_t x8 __asm__("x8") = n;
    register int64_t x0 __asm__("x0") = a1;
    register int64_t x1 __asm__("x1") = a2;
    register int64_t x2 __asm__("x2") = a3;
    __asm__ volatile("svc #0" : "+r"(x0) : "r"(x8), "r"(x1), "r"(x2) : "memory");
    return x0;
}
static inline int64_t sg_syscall4(int64_t n, int64_t a1, int64_t a2, int64_t a3, int64_t a4) {
    register int64_t x8 __asm__("x8") = n;
    register int64_t x0 __asm__("x0") = a1;
    register int64_t x1 __asm__("x1") = a2;
    register int64_t x2 __asm__("x2") = a3;
    register int64_t x3 __asm__("x3") = a4;
    __asm__ volatile("svc #0" : "+r"(x0) : "r"(x8), "r"(x1), "r"(x2), "r"(x3) : "memory");
    return x0;
}
static inline int64_t sg_syscall6(int64_t n, int64_t a1, int64_t a2, int64_t a3, int64_t a4, int64_t a5, int64_t a6) {
    register int64_t x8 __asm__("x8") = n;
    register int64_t x0 __asm__("x0") = a1;
    register int64_t x1 __asm__("x1") = a2;
    register int64_t x2 __asm__("x2") = a3;
    register int64_t x3 __asm__("x3") = a4;
    register int64_t x4 __asm__("x4") = a5;
    register int64_t x5 __asm__("x5") = a6;
    __asm__ volatile("svc #0" : "+r"(x0) : "r"(x8), "r"(x1), "r"(x2), "r"(x3), "r"(x4), "r"(x5) : "memory");
    return x0;
}
#endif

#define SG_AT_FDCWD    (-100)
#define SG_O_RDONLY    0
#define SG_O_WRONLY    1
#define SG_O_CREAT     0100
#define SG_O_TRUNC     01000
#define SG_SEEK_END    2
#define SG_PROT_READ   1
#define SG_PROT_WRITE  2
#define SG_MAP_PRIVATE 0x02
#define SG_MAP_ANON    0x20
#define SG_EINTR       4

static inline void sg_exit(int code) {
    sg_syscall1(SG_SYS_exit_group, code);
    __builtin_unreachable();
}

static inline void *sg_mmap_anon(size_t size) {
    int64_t r = sg_syscall6(SG_SYS_mmap, 0, (int64_t)size, SG_PROT_READ | SG_PROT_WRITE,
                            SG_MAP_PRIVATE | SG_MAP_ANON, -1, 0);
    if (r < 0 && r > -4096) return NULL;
    return (void *)(uintptr_t)r;
}

static inline void sg_munmap(void *ptr, size_t size) {
    sg_syscall2(SG_SYS_munmap, (int64_t)(uintptr_t)ptr, (int64_t)size);
}

static inline int sg_open_ro(const char *path) {
    int64_t r;
    do {
        r = sg_syscall4(SG_SYS_openat, SG_AT_FDCWD, (int64_t)(uintptr_t)path, SG_O_RDONLY, 0);
    } while (r == -SG_EINTR);
    return (int)r;
}

static inline int sg_open_wo(const char *path) {
    int64_t r;
    do {
        r = sg_syscall4(SG_SYS_openat, SG_AT_FDCWD, (int64_t)(uintptr_t)path,
                        SG_O_WRONLY | SG_O_CREAT | SG_O_TRUNC, 0644);
    } while (r == -SG_EINTR);
    if (r >= 0) sg_syscall2(SG_SYS_fchmod, r, 0644);
    return (int)r;
}

static inline int64_t sg_lseek_end(int fd) {
    return sg_syscall3(SG_SYS_lseek, fd, 0, SG_SEEK_END);
}

static inline int64_t sg_pread_all(int fd, uint8_t *buf, size_t len) {
    size_t done = 0;
    while (done < len) {
        int64_t r = sg_syscall4(SG_SYS_pread64, fd, (int64_t)(uintptr_t)(buf + done),
                                (int64_t)(len - done), (int64_t)done);
        if (r == -SG_EINTR) continue;
        if (r <= 0) return -1;
        done += (size_t)r;
    }
    return (int64_t)done;
}

static inline int64_t sg_read_all_stream(int fd, uint8_t *buf, size_t cap, size_t *out_len) {
    size_t done = 0;
    while (done < cap) {
        int64_t r = sg_syscall3(SG_SYS_read, fd, (int64_t)(uintptr_t)(buf + done), (int64_t)(cap - done));
        if (r == -SG_EINTR) continue;
        if (r < 0) return -1;
        if (r == 0) break;
        done += (size_t)r;
    }
    *out_len = done;
    return 0;
}

static inline int sg_write_all(int fd, const uint8_t *buf, size_t len) {
    size_t done = 0;
    while (done < len) {
        int64_t r = sg_syscall3(SG_SYS_write, fd, (int64_t)(uintptr_t)(buf + done), (int64_t)(len - done));
        if (r == -SG_EINTR) continue;
        if (r <= 0) return -1;
        done += (size_t)r;
    }
    return 0;
}

static inline void sg_close(int fd) {
    if (fd >= 0) sg_syscall1(SG_SYS_close, fd);
}

/* Provide freestanding mem* symbols so clang -O3 -nostdlib never misses them */
#ifndef SG_MEMFUNCS_DEFINED
#define SG_MEMFUNCS_DEFINED
static inline size_t sg_strlen(const char *s) {
    const char *p = s;
    while (*p) p++;
    return (size_t)(p - s);
}
static inline int sg_memcmp(const void *a, const void *b, size_t n) {
    const uint8_t *pa = (const uint8_t *)a;
    const uint8_t *pb = (const uint8_t *)b;
    for (size_t i = 0; i < n; i++) {
        if (pa[i] != pb[i]) return (int)pa[i] - (int)pb[i];
    }
    return 0;
}
static inline void *sg_memcpy(void *dst, const void *src, size_t n) {
    uint8_t *d = (uint8_t *)dst;
    const uint8_t *s = (const uint8_t *)src;
    while (n >= 8) {
        __builtin_memcpy(d, s, 8);
        d += 8; s += 8; n -= 8;
    }
    while (n--) *d++ = *s++;
    return dst;
}
static inline void *sg_memset(void *dst, int c, size_t n) {
    uint8_t *d = (uint8_t *)dst;
    uint8_t b = (uint8_t)c;
    uint64_t b8 = (uint64_t)b * 0x0101010101010101ULL;
    while (n >= 8) {
        __builtin_memcpy(d, &b8, 8);
        d += 8; n -= 8;
    }
    while (n--) *d++ = b;
    return dst;
}
static inline void *sg_memmove(void *dst, const void *src, size_t n) {
    uint8_t *d = (uint8_t *)dst;
    const uint8_t *s = (const uint8_t *)src;
    if (d == s || n == 0) return dst;
    if (d < s) {
        while (n--) *d++ = *s++;
    } else {
        d += n; s += n;
        while (n--) *--d = *--s;
    }
    return dst;
}
#endif

static inline void sg_read_env_dirs(char *in_dir, size_t in_cap, char *out_dir, size_t out_cap) {
    in_dir[0] = '\0';
    out_dir[0] = '\0';
    int fd = sg_open_ro("/proc/self/environ");
    if (fd < 0) return;
    uint8_t buf[16384];
    size_t len = 0;
    sg_read_all_stream(fd, buf, sizeof(buf) - 1, &len);
    sg_close(fd);
    buf[len] = '\0';
    size_t pos = 0;
    while (pos < len) {
        const char *entry = (const char *)(buf + pos);
        size_t elen = sg_strlen(entry);
        if (elen > 17 && sg_memcmp(entry, "MAKEFUN_INPUT_DIR=", 18) == 0) {
            size_t vlen = elen - 18;
            if (vlen < in_cap) { sg_memcpy(in_dir, entry + 18, vlen + 1); }
        } else if (elen > 18 && sg_memcmp(entry, "MAKEFUN_OUTPUT_DIR=", 19) == 0) {
            size_t vlen = elen - 19;
            if (vlen < out_cap) { sg_memcpy(out_dir, entry + 19, vlen + 1); }
        }
        pos += elen + 1;
    }
}

#else
/* Windows / Hosted CLI build for testing, differential fuzzing, and benchmarking */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define sg_strlen  strlen
#define sg_memcmp  memcmp
#define sg_memcpy  memcpy
#define sg_memset  memset
#define sg_memmove memmove

static inline void sg_mmap_free_noop(void) {}
#endif
