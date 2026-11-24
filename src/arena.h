#pragma once
#include "platform.h"

#if defined(SG_USE_AVX2)
#include <immintrin.h>
#elif defined(SG_USE_SSE2)
#include <emmintrin.h>
#elif defined(SG_USE_NEON)
#include <arm_neon.h>
#endif

typedef struct {
    const char *ptr;
    uint32_t len;
} SgStr;

typedef struct SgArenaBlock {
    struct SgArenaBlock *prev;
    size_t size;
} SgArenaBlock;

#define SG_STATIC_ARENA_BYTES (48u * 1024u * 1024u)

typedef struct {
    uint8_t *base;
    uintptr_t cur;
    uintptr_t end;
    SgArenaBlock *extra;
} SgArena;

extern uint8_t sg_static_arena_buf[SG_STATIC_ARENA_BYTES];

SG_INLINE void sg_arena_init(SgArena *a) {
    a->base = sg_static_arena_buf;
    a->cur = (uintptr_t)sg_static_arena_buf;
    a->end = (uintptr_t)sg_static_arena_buf + SG_STATIC_ARENA_BYTES;
    a->extra = NULL;
}

SG_NOINLINE static void *sg_arena_alloc_slow(SgArena *a, size_t size, size_t align) {
    size_t blk_sz = size + sizeof(SgArenaBlock) + align + 64;
    if (blk_sz < (4u * 1024u * 1024u)) blk_sz = (4u * 1024u * 1024u);
#ifndef SG_WINDOWS_CLI
    SgArenaBlock *blk = (SgArenaBlock *)sg_mmap_anon(blk_sz);
    if (!blk) sg_exit(2);
#else
    SgArenaBlock *blk = (SgArenaBlock *)malloc(blk_sz);
    if (!blk) exit(2);
#endif
    blk->prev = a->extra;
    blk->size = blk_sz;
    a->extra = blk;
    uintptr_t p = ((uintptr_t)(blk + 1) + align - 1) & ~(align - 1);
    a->cur = p + size;
    a->end = (uintptr_t)blk + blk_sz;
    return (void *)p;
}

SG_INLINE void *sg_arena_alloc(SgArena *a, size_t size, size_t align) {
    uintptr_t p = (a->cur + align - 1) & ~(align - 1);
    if (SG_LIKELY(p + size <= a->end)) {
        a->cur = p + size;
        return (void *)p;
    }
    return sg_arena_alloc_slow(a, size, align);
}

SG_INLINE void sg_arena_reset(SgArena *a) {
    SgArenaBlock *b = a->extra;
    while (b) {
        SgArenaBlock *prev = b->prev;
#ifndef SG_WINDOWS_CLI
        sg_munmap(b, b->size);
#else
        free(b);
#endif
        b = prev;
    }
    a->extra = NULL;
    a->cur = (uintptr_t)a->base;
    a->end = (uintptr_t)a->base + SG_STATIC_ARENA_BYTES;
}

SG_INLINE char *sg_arena_strdup(SgArena *a, const char *s, size_t len) {
    char *p = (char *)sg_arena_alloc(a, len + 1, 1);
    if (len) sg_memcpy(p, s, len);
    p[len] = '\0';
    return p;
}

/* Reusable growable byte buffer */
typedef struct {
    uint8_t *data;
    size_t len;
    size_t cap;
    bool owns_heap;
} SgBuf;

#ifndef SG_WINDOWS_CLI
static inline void *sg_buf_mmap(size_t cap) {
    void *p = sg_mmap_anon(cap);
    if (!p) sg_exit(3);
    return p;
}
static inline void sg_buf_free_raw(void *p, size_t cap) { sg_munmap(p, cap); }
#else
static inline void *sg_buf_mmap(size_t cap) {
    void *p = malloc(cap);
    if (!p) exit(3);
    return p;
}
static inline void sg_buf_free_raw(void *p, size_t cap) { (void)cap; free(p); }
#endif

SG_INLINE void sg_buf_init_static(SgBuf *b, uint8_t *backing, size_t cap) {
    b->data = backing;
    b->len = 0;
    b->cap = cap;
    b->owns_heap = false;
}

SG_NOINLINE static void sg_buf_grow(SgBuf *b, size_t need) {
    size_t new_cap = b->cap ? b->cap * 2 : 65536;
    while (new_cap < need) new_cap *= 2;
    uint8_t *nd = (uint8_t *)sg_buf_mmap(new_cap);
    if (b->len) sg_memcpy(nd, b->data, b->len);
    if (b->owns_heap && b->data) sg_buf_free_raw(b->data, b->cap);
    b->data = nd;
    b->cap = new_cap;
    b->owns_heap = true;
}

/* Streaming Output Buffer (512 KB L2-hot) */
#define SG_OUTBUF_CAP (512u * 1024u)

typedef struct {
    uint8_t buf[SG_OUTBUF_CAP] __attribute__((aligned(64)));
    size_t pos;
#ifndef SG_WINDOWS_CLI
    int fd;
#else
    FILE *fp;
#endif
} SgOutBuf;

extern SgOutBuf sg_outbuf_global;

SG_NOINLINE static void sg_ob_flush(SgOutBuf *ob) {
    if (ob->pos > 0) {
#ifndef SG_WINDOWS_CLI
        if (sg_write_all(ob->fd, ob->buf, ob->pos) != 0) sg_exit(4);
#else
        if (fwrite(ob->buf, 1, ob->pos, ob->fp) != ob->pos) exit(4);
#endif
        ob->pos = 0;
    }
}

SG_INLINE void sg_ob_write(SgOutBuf *ob, const void *ptr, size_t len) {
    const uint8_t *src = (const uint8_t *)ptr;
    if (SG_LIKELY(ob->pos + len <= SG_OUTBUF_CAP)) {
        sg_memcpy(ob->buf + ob->pos, src, len);
        ob->pos += len;
        return;
    }
    if (ob->pos > 0) {
        size_t take = SG_OUTBUF_CAP - ob->pos;
        if (take > 0) {
            sg_memcpy(ob->buf + ob->pos, src, take);
            ob->pos += take;
            src += take;
            len -= take;
        }
        sg_ob_flush(ob);
    }
    if (len >= SG_OUTBUF_CAP) {
#ifndef SG_WINDOWS_CLI
        if (sg_write_all(ob->fd, src, len) != 0) sg_exit(4);
#else
        if (fwrite(src, 1, len, ob->fp) != len) exit(4);
#endif
        return;
    }
    if (len > 0) {
        sg_memcpy(ob->buf, src, len);
        ob->pos = len;
    }
}

SG_INLINE void sg_ob_putc(SgOutBuf *ob, char c) {
    if (SG_UNLIKELY(ob->pos == SG_OUTBUF_CAP)) sg_ob_flush(ob);
    ob->buf[ob->pos++] = (uint8_t)c;
}

SG_INLINE void sg_ob_cstr(SgOutBuf *ob, const char *s) {
    sg_ob_write(ob, s, sg_strlen(s));
}

SG_INLINE void sg_ob_u32(SgOutBuf *ob, uint32_t v) {
    char tmp[16];
    int idx = 16;
    do {
        tmp[--idx] = (char)('0' + (v % 10u));
        v /= 10u;
    } while (v != 0);
    sg_ob_write(ob, tmp + idx, (size_t)(16 - idx));
}

static const char SG_HEX_LOWER[17] = "0123456789abcdef";

SG_INLINE size_t sg_json_esc_need_char(uint8_t c, char *out) {
    switch (c) {
        case '"':  out[0] = '\\'; out[1] = '"';  return 2;
        case '\\': out[0] = '\\'; out[1] = '\\'; return 2;
        case '\b': out[0] = '\\'; out[1] = 'b';  return 2;
        case '\f': out[0] = '\\'; out[1] = 'f';  return 2;
        case '\n': out[0] = '\\'; out[1] = 'n';  return 2;
        case '\r': out[0] = '\\'; out[1] = 'r';  return 2;
        case '\t': out[0] = '\\'; out[1] = 't';  return 2;
        default:
            out[0] = '\\'; out[1] = 'u'; out[2] = '0'; out[3] = '0';
            out[4] = SG_HEX_LOWER[c >> 4];
            out[5] = SG_HEX_LOWER[c & 0x0F];
            return 6;
    }
}

SG_INLINE void sg_ob_json_esc(SgOutBuf *ob, const char *s, size_t len) {
    const uint8_t *p = (const uint8_t *)s;
    size_t i = 0;
#if defined(SG_USE_AVX2)
    const __m256i v_q = _mm256_set1_epi8('"');
    const __m256i v_bs = _mm256_set1_epi8('\\');
    const __m256i v_1f = _mm256_set1_epi8(0x1f);
    while (i + 32 <= len) {
        __m256i v = _mm256_loadu_si256((const __m256i *)(p + i));
        __m256i m_ctrl = _mm256_cmpeq_epi8(_mm256_min_epu8(v, v_1f), v);
        __m256i m = _mm256_or_si256(_mm256_or_si256(_mm256_cmpeq_epi8(v, v_q), _mm256_cmpeq_epi8(v, v_bs)), m_ctrl);
        uint32_t mask = (uint32_t)_mm256_movemask_epi8(m);
        if (SG_LIKELY(mask == 0)) {
            sg_ob_write(ob, p + i, 32);
            i += 32;
            continue;
        }
        uint32_t k = (uint32_t)__builtin_ctz(mask);
        if (k > 0) sg_ob_write(ob, p + i, k);
        i += k;
        char esc[6];
        size_t el = sg_json_esc_need_char(p[i], esc);
        sg_ob_write(ob, esc, el);
        i++;
    }
#elif defined(SG_USE_SSE2)
    const __m128i v_q = _mm_set1_epi8('"');
    const __m128i v_bs = _mm_set1_epi8('\\');
    const __m128i v_1f = _mm_set1_epi8(0x1f);
    while (i + 16 <= len) {
        __m128i v = _mm_loadu_si128((const __m128i *)(p + i));
        __m128i m_ctrl = _mm_cmpeq_epi8(_mm_min_epu8(v, v_1f), v);
        __m128i m = _mm_or_si128(_mm_or_si128(_mm_cmpeq_epi8(v, v_q), _mm_cmpeq_epi8(v, v_bs)), m_ctrl);
        uint32_t mask = (uint32_t)_mm_movemask_epi8(m);
        if (SG_LIKELY(mask == 0)) {
            sg_ob_write(ob, p + i, 16);
            i += 16;
            continue;
        }
        uint32_t k = (uint32_t)__builtin_ctz(mask);
        if (k > 0) sg_ob_write(ob, p + i, k);
        i += k;
        char esc[6];
        size_t el = sg_json_esc_need_char(p[i], esc);
        sg_ob_write(ob, esc, el);
        i++;
    }
#elif defined(SG_USE_NEON)
    const uint8x16_t v_q = vdupq_n_u8('"');
    const uint8x16_t v_bs = vdupq_n_u8('\\');
    const uint8x16_t v_20 = vdupq_n_u8(0x20);
    while (i + 16 <= len) {
        uint8x16_t v = vld1q_u8(p + i);
        uint8x16_t m = vorrq_u8(vorrq_u8(vceqq_u8(v, v_q), vceqq_u8(v, v_bs)), vcltq_u8(v, v_20));
        uint8x16_t narrowed = vpaddq_u8(m, m);
        uint64_t mask = vget_lane_u64(vreinterpret_u64_u8(vshrn_n_u16(vreinterpretq_u16_u8(m), 4)), 0);
        if (SG_LIKELY(mask == 0)) {
            (void)narrowed;
            sg_ob_write(ob, p + i, 16);
            i += 16;
            continue;
        }
        uint32_t k = (uint32_t)(__builtin_ctzll(mask) >> 2);
        if (k > 0) sg_ob_write(ob, p + i, k);
        i += k;
        char esc[6];
        size_t el = sg_json_esc_need_char(p[i], esc);
        sg_ob_write(ob, esc, el);
        i++;
    }
#endif
    size_t start = i;
    while (i < len) {
        uint8_t c = p[i];
        if (SG_UNLIKELY(c < 0x20 || c == '"' || c == '\\')) {
            if (i > start) sg_ob_write(ob, p + start, i - start);
            char esc[6];
            size_t el = sg_json_esc_need_char(c, esc);
            sg_ob_write(ob, esc, el);
            i++;
            start = i;
        } else {
            i++;
        }
    }
    if (i > start) sg_ob_write(ob, p + start, i - start);
}

/* HTML-escape (& < > " ') AND JSON-escape simultaneously directly into OutBuf! */
SG_INLINE void sg_ob_html_then_json_esc(SgOutBuf *ob, const char *s, size_t len) {
    const uint8_t *p = (const uint8_t *)s;
    size_t i = 0;
#if defined(SG_USE_AVX2)
    /* Any byte < 0x40 could match & (0x26), ' (0x27), " (0x22), < (0x3c), > (0x3e), or < 0x20. */
    /* Wait: digits/spaces/slashes/parens are < 0x40, so check exact chars! */
    const __m256i v_amp = _mm256_set1_epi8('&');
    const __m256i v_lt  = _mm256_set1_epi8('<');
    const __m256i v_gt  = _mm256_set1_epi8('>');
    const __m256i v_dq  = _mm256_set1_epi8('"');
    const __m256i v_sq  = _mm256_set1_epi8('\'');
    const __m256i v_bs  = _mm256_set1_epi8('\\');
    const __m256i v_1f  = _mm256_set1_epi8(0x1f);
    while (i + 32 <= len) {
        __m256i v = _mm256_loadu_si256((const __m256i *)(p + i));
        __m256i m1 = _mm256_or_si256(_mm256_cmpeq_epi8(v, v_amp), _mm256_cmpeq_epi8(v, v_lt));
        __m256i m2 = _mm256_or_si256(_mm256_cmpeq_epi8(v, v_gt), _mm256_cmpeq_epi8(v, v_dq));
        __m256i m3 = _mm256_or_si256(_mm256_cmpeq_epi8(v, v_sq), _mm256_cmpeq_epi8(v, v_bs));
        __m256i m4 = _mm256_cmpeq_epi8(_mm256_min_epu8(v, v_1f), v);
        __m256i m  = _mm256_or_si256(_mm256_or_si256(m1, m2), _mm256_or_si256(m3, m4));
        uint32_t mask = (uint32_t)_mm256_movemask_epi8(m);
        if (SG_LIKELY(mask == 0)) {
            sg_ob_write(ob, p + i, 32);
            i += 32;
            continue;
        }
        uint32_t k = (uint32_t)__builtin_ctz(mask);
        if (k > 0) sg_ob_write(ob, p + i, k);
        i += k;
        uint8_t c = p[i++];
        if (c == '&') sg_ob_write(ob, "&amp;", 5);
        else if (c == '<') sg_ob_write(ob, "&lt;", 4);
        else if (c == '>') sg_ob_write(ob, "&gt;", 4);
        else if (c == '"') sg_ob_write(ob, "&quot;", 6);
        else if (c == '\'') sg_ob_write(ob, "&#39;", 5);
        else { char esc[6]; size_t el = sg_json_esc_need_char(c, esc); sg_ob_write(ob, esc, el); }
    }
#elif defined(SG_USE_SSE2)
    const __m128i v_amp = _mm_set1_epi8('&');
    const __m128i v_lt  = _mm_set1_epi8('<');
    const __m128i v_gt  = _mm_set1_epi8('>');
    const __m128i v_dq  = _mm_set1_epi8('"');
    const __m128i v_sq  = _mm_set1_epi8('\'');
    const __m128i v_bs  = _mm_set1_epi8('\\');
    const __m128i v_1f  = _mm_set1_epi8(0x1f);
    while (i + 16 <= len) {
        __m128i v = _mm_loadu_si128((const __m128i *)(p + i));
        __m128i m1 = _mm_or_si128(_mm_cmpeq_epi8(v, v_amp), _mm_cmpeq_epi8(v, v_lt));
        __m128i m2 = _mm_or_si128(_mm_cmpeq_epi8(v, v_gt), _mm_cmpeq_epi8(v, v_dq));
        __m128i m3 = _mm_or_si128(_mm_cmpeq_epi8(v, v_sq), _mm_cmpeq_epi8(v, v_bs));
        __m128i m4 = _mm_cmpeq_epi8(_mm_min_epu8(v, v_1f), v);
        __m128i m  = _mm_or_si128(_mm_or_si128(m1, m2), _mm_or_si128(m3, m4));
        uint32_t mask = (uint32_t)_mm_movemask_epi8(m);
        if (SG_LIKELY(mask == 0)) {
            sg_ob_write(ob, p + i, 16);
            i += 16;
            continue;
        }
        uint32_t k = (uint32_t)__builtin_ctz(mask);
        if (k > 0) sg_ob_write(ob, p + i, k);
        i += k;
        uint8_t c = p[i++];
        if (c == '&') sg_ob_write(ob, "&amp;", 5);
        else if (c == '<') sg_ob_write(ob, "&lt;", 4);
        else if (c == '>') sg_ob_write(ob, "&gt;", 4);
        else if (c == '"') sg_ob_write(ob, "&quot;", 6);
        else if (c == '\'') sg_ob_write(ob, "&#39;", 5);
        else { char esc[6]; size_t el = sg_json_esc_need_char(c, esc); sg_ob_write(ob, esc, el); }
    }
#elif defined(SG_USE_NEON)
    const uint8x16_t v_amp = vdupq_n_u8('&');
    const uint8x16_t v_lt  = vdupq_n_u8('<');
    const uint8x16_t v_gt  = vdupq_n_u8('>');
    const uint8x16_t v_dq  = vdupq_n_u8('"');
    const uint8x16_t v_sq  = vdupq_n_u8('\'');
    const uint8x16_t v_bs  = vdupq_n_u8('\\');
    const uint8x16_t v_20  = vdupq_n_u8(0x20);
    while (i + 16 <= len) {
        uint8x16_t v = vld1q_u8(p + i);
        uint8x16_t m = vorrq_u8(
            vorrq_u8(vorrq_u8(vceqq_u8(v, v_amp), vceqq_u8(v, v_lt)),
                     vorrq_u8(vceqq_u8(v, v_gt), vceqq_u8(v, v_dq))),
            vorrq_u8(vorrq_u8(vceqq_u8(v, v_sq), vceqq_u8(v, v_bs)),
                     vcltq_u8(v, v_20)));
        uint64_t mask = vget_lane_u64(vreinterpret_u64_u8(vshrn_n_u16(vreinterpretq_u16_u8(m), 4)), 0);
        if (SG_LIKELY(mask == 0)) {
            sg_ob_write(ob, p + i, 16);
            i += 16;
            continue;
        }
        uint32_t k = (uint32_t)(__builtin_ctzll(mask) >> 2);
        if (k > 0) sg_ob_write(ob, p + i, k);
        i += k;
        uint8_t c = p[i++];
        if (c == '&') sg_ob_write(ob, "&amp;", 5);
        else if (c == '<') sg_ob_write(ob, "&lt;", 4);
        else if (c == '>') sg_ob_write(ob, "&gt;", 4);
        else if (c == '"') sg_ob_write(ob, "&quot;", 6);
        else if (c == '\'') sg_ob_write(ob, "&#39;", 5);
        else { char esc[6]; size_t el = sg_json_esc_need_char(c, esc); sg_ob_write(ob, esc, el); }
    }
#endif
    size_t start = i;
    while (i < len) {
        uint8_t c = p[i];
        if (SG_UNLIKELY(c < 0x20 || c == '&' || c == '<' || c == '>' || c == '"' || c == '\'' || c == '\\')) {
            if (i > start) sg_ob_write(ob, p + start, i - start);
            if (c == '&') sg_ob_write(ob, "&amp;", 5);
            else if (c == '<') sg_ob_write(ob, "&lt;", 4);
            else if (c == '>') sg_ob_write(ob, "&gt;", 4);
            else if (c == '"') sg_ob_write(ob, "&quot;", 6);
            else if (c == '\'') sg_ob_write(ob, "&#39;", 5);
            else { char esc[6]; size_t el = sg_json_esc_need_char(c, esc); sg_ob_write(ob, esc, el); }
            i++;
            start = i;
        } else {
            i++;
        }
    }
    if (i > start) sg_ob_write(ob, p + start, i - start);
}

/* Also provide JSON-escape into a SgBuf (for pre-rendering card/site fragments!) */
SG_INLINE void sg_buf_reserve(SgBuf *b, size_t add) {
    if (SG_UNLIKELY(b->len + add > b->cap)) sg_buf_grow(b, b->len + add);
}
SG_INLINE void sg_buf_write_s(SgBuf *b, const void *ptr, size_t len) {
    sg_buf_reserve(b, len);
    sg_memcpy(b->data + b->len, ptr, len);
    b->len += len;
}
SG_INLINE void sg_buf_putc_s(SgBuf *b, char c) {
    sg_buf_reserve(b, 1);
    b->data[b->len++] = (uint8_t)c;
}
