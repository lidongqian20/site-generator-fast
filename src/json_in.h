#pragma once
#include "arena.h"

/* Fast SIMD-assisted in-place JSON parser specifically for makefun-site cases.json */

typedef struct {
    SgStr name;
    SgStr src;
} SgRawTemplate;

typedef struct {
    SgStr path;
    /* For static files: raw slice INSIDE quotes from input JSON (valid UTF-8 JSON escape content, zero copy!).
       For .md files: decoded & \r\n->\n normalized content pointer & len (or lazy range). */
    const char *raw_ptr;
    uint32_t raw_len;
    bool is_md;
    SgStr md_text;
} SgRawFile;

typedef struct {
    SgStr site_title;
    SgStr site_base_url;
    uint32_t per_page;
    uint32_t recent;

    SgRawTemplate *templates;
    uint32_t tpl_count;

    SgRawFile *files;
    uint32_t file_count;
} SgCaseInput;

SG_INLINE uint32_t sg_hex4(const uint8_t *p) {
    uint32_t v = 0;
    for (int i = 0; i < 4; i++) {
        uint8_t c = p[i];
        v <<= 4;
        if ((uint8_t)(c - '0') <= 9u) v |= (uint32_t)(c - '0');
        else if ((uint8_t)(c - 'a') <= 5u) v |= (uint32_t)(c - 'a' + 10);
        else v |= (uint32_t)(c - 'A' + 10);
    }
    return v;
}

SG_INLINE size_t sg_encode_utf8(uint32_t cp, uint8_t *out) {
    if (cp < 0x80) { out[0] = (uint8_t)cp; return 1; }
    if (cp < 0x800) { out[0] = (uint8_t)(0xC0 | (cp >> 6)); out[1] = (uint8_t)(0x80 | (cp & 0x3F)); return 2; }
    if (cp < 0x10000) {
        out[0] = (uint8_t)(0xE0 | (cp >> 12));
        out[1] = (uint8_t)(0x80 | ((cp >> 6) & 0x3F));
        out[2] = (uint8_t)(0x80 | (cp & 0x3F));
        return 3;
    }
    out[0] = (uint8_t)(0xF0 | (cp >> 18));
    out[1] = (uint8_t)(0x80 | ((cp >> 12) & 0x3F));
    out[2] = (uint8_t)(0x80 | ((cp >> 6) & 0x3F));
    out[3] = (uint8_t)(0x80 | (cp & 0x3F));
    return 4;
}

/* Skip JSON whitespace */
SG_INLINE const uint8_t *sg_skip_ws(const uint8_t *p, const uint8_t *end) {
    while (p < end && (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r')) p++;
    return p;
}

/* Skip a JSON string without modifying buffer, returning pointer right after closing '"' */
SG_INLINE const uint8_t *sg_skip_json_str(const uint8_t *p, const uint8_t *end, const uint8_t **out_inner_start, size_t *out_inner_len) {
    /* *p == '"' */
    p++;
    const uint8_t *start = p;
#if defined(SG_USE_AVX2)
    const __m256i v_q = _mm256_set1_epi8('"');
    const __m256i v_bs = _mm256_set1_epi8('\\');
    while (p + 32 <= end) {
        __m256i v = _mm256_loadu_si256((const __m256i *)p);
        uint32_t mask = (uint32_t)_mm256_movemask_epi8(_mm256_or_si256(_mm256_cmpeq_epi8(v, v_q), _mm256_cmpeq_epi8(v, v_bs)));
        if (mask == 0) { p += 32; continue; }
        uint32_t k = (uint32_t)__builtin_ctz(mask);
        p += k;
        if (*p == '"') { *out_inner_start = start; *out_inner_len = (size_t)(p - start); return p + 1; }
        p += 2;
    }
#elif defined(SG_USE_SSE2)
    const __m128i v_q = _mm_set1_epi8('"');
    const __m128i v_bs = _mm_set1_epi8('\\');
    while (p + 16 <= end) {
        __m128i v = _mm_loadu_si128((const __m128i *)p);
        uint32_t mask = (uint32_t)_mm_movemask_epi8(_mm_or_si128(_mm_cmpeq_epi8(v, v_q), _mm_cmpeq_epi8(v, v_bs)));
        if (mask == 0) { p += 16; continue; }
        uint32_t k = (uint32_t)__builtin_ctz(mask);
        p += k;
        if (*p == '"') { *out_inner_start = start; *out_inner_len = (size_t)(p - start); return p + 1; }
        p += 2;
    }
#elif defined(SG_USE_NEON)
    const uint8x16_t v_q = vdupq_n_u8('"');
    const uint8x16_t v_bs = vdupq_n_u8('\\');
    while (p + 16 <= end) {
        uint8x16_t v = vld1q_u8(p);
        uint8x16_t m = vorrq_u8(vceqq_u8(v, v_q), vceqq_u8(v, v_bs));
        uint64_t mask = vget_lane_u64(vreinterpret_u64_u8(vshrn_n_u16(vreinterpretq_u16_u8(m), 4)), 0);
        if (mask == 0) { p += 16; continue; }
        p += (size_t)(__builtin_ctzll(mask) >> 2);
        if (*p == '"') { *out_inner_start = start; *out_inner_len = (size_t)(p - start); return p + 1; }
        p += 2;
    }
#endif
    while (p < end) {
        uint8_t c = *p;
        if (c == '"') {
            *out_inner_start = start;
            *out_inner_len = (size_t)(p - start);
            return p + 1;
        }
        if (c == '\\') p += 2;
        else p++;
    }
    *out_inner_start = start;
    *out_inner_len = (size_t)(p - start);
    return p;
}

/* Decode raw inner JSON string into arena (or normalize \r\n? -> \n on the fly if normalize_eol is true!).
   Note: raw slice must NOT contain terminating '"' unless handled carefully.
   Wait: most fast strings don't have '\' at all! For static inspect or no-escape short strings we scan first or decode directly. */
static const uint8_t *sg_parse_json_str_to_arena(SgArena *a, const uint8_t *p, const uint8_t *end, bool normalize_eol, SgStr *out) {
    /* *p == '"' */
    const uint8_t *inner_start;
    size_t raw_len;
    const uint8_t *after = sg_skip_json_str(p, end, &inner_start, &raw_len);
    char *dst = (char *)sg_arena_alloc(a, raw_len + 1, 1);
    size_t d = 0;
    const uint8_t *s = inner_start;
    const uint8_t *send = inner_start + raw_len;

    if (!normalize_eol) {
        /* Copy clean spans via SIMD memchr for '\' */
        while (s < send) {
            const uint8_t *chunk = s;
#if defined(SG_USE_AVX2)
            const __m256i v_bs = _mm256_set1_epi8('\\');
            while (s + 32 <= send) {
                __m256i v = _mm256_loadu_si256((const __m256i *)s);
                uint32_t m = (uint32_t)_mm256_movemask_epi8(_mm256_cmpeq_epi8(v, v_bs));
                if (m == 0) { s += 32; continue; }
                s += (uint32_t)__builtin_ctz(m);
                break;
            }
#elif defined(SG_USE_SSE2)
            const __m128i v_bs = _mm_set1_epi8('\\');
            while (s + 16 <= send) {
                __m128i v = _mm_loadu_si128((const __m128i *)s);
                uint32_t m = (uint32_t)_mm_movemask_epi8(_mm_cmpeq_epi8(v, v_bs));
                if (m == 0) { s += 16; continue; }
                s += (uint32_t)__builtin_ctz(m);
                break;
            }
#elif defined(SG_USE_NEON)
            const uint8x16_t v_bs = vdupq_n_u8('\\');
            while (s + 16 <= send) {
                uint8x16_t v = vld1q_u8(s);
                uint8x16_t m = vceqq_u8(v, v_bs);
                uint64_t mask = vget_lane_u64(vreinterpret_u64_u8(vshrn_n_u16(vreinterpretq_u16_u8(m), 4)), 0);
                if (mask == 0) { s += 16; continue; }
                s += (size_t)(__builtin_ctzll(mask) >> 2);
                break;
            }
#endif
            while (s < send && *s != '\\') s++;
            size_t clen = (size_t)(s - chunk);
            if (clen) { sg_memcpy(dst + d, chunk, clen); d += clen; }
            if (s >= send) break;
            s++; /* skip '\' */
            if (s >= send) break;
            uint8_t esc = *s++;
            switch (esc) {
                case '"':  dst[d++] = '"';  break;
                case '\\': dst[d++] = '\\'; break;
                case '/':  dst[d++] = '/';  break;
                case 'b':  dst[d++] = '\b'; break;
                case 'f':  dst[d++] = '\f'; break;
                case 'n':  dst[d++] = '\n'; break;
                case 'r':  dst[d++] = '\r'; break;
                case 't':  dst[d++] = '\t'; break;
                case 'u':
                    if (s + 4 <= send) {
                        uint32_t cp = sg_hex4(s); s += 4;
                        if (cp >= 0xD800 && cp <= 0xDBFF && s + 6 <= send && s[0] == '\\' && s[1] == 'u') {
                            uint32_t lo = sg_hex4(s + 2);
                            if (lo >= 0xDC00 && lo <= 0xDFFF) {
                                cp = 0x10000 + (((cp - 0xD800) << 10) | (lo - 0xDC00));
                                s += 6;
                            }
                        }
                        d += sg_encode_utf8(cp, (uint8_t *)(dst + d));
                    }
                    break;
                default: dst[d++] = (char)esc; break;
            }
        }
    } else {
        /* Fused JSON decode + \r\n? -> \n normalization */
        bool prev_cr = false;
        while (s < send) {
            const uint8_t *chunk = s;
#if defined(SG_USE_AVX2)
            const __m256i v_bs = _mm256_set1_epi8('\\');
            const __m256i v_cr = _mm256_set1_epi8('\r');
            while (s + 32 <= send) {
                __m256i v = _mm256_loadu_si256((const __m256i *)s);
                uint32_t m = (uint32_t)_mm256_movemask_epi8(_mm256_or_si256(_mm256_cmpeq_epi8(v, v_bs), _mm256_cmpeq_epi8(v, v_cr)));
                if (m == 0) { s += 32; continue; }
                s += (uint32_t)__builtin_ctz(m);
                break;
            }
#elif defined(SG_USE_SSE2)
            const __m128i v_bs = _mm_set1_epi8('\\');
            const __m128i v_cr = _mm_set1_epi8('\r');
            while (s + 16 <= send) {
                __m128i v = _mm_loadu_si128((const __m128i *)s);
                uint32_t m = (uint32_t)_mm_movemask_epi8(_mm_or_si128(_mm_cmpeq_epi8(v, v_bs), _mm_cmpeq_epi8(v, v_cr)));
                if (m == 0) { s += 16; continue; }
                s += (uint32_t)__builtin_ctz(m);
                break;
            }
#elif defined(SG_USE_NEON)
            const uint8x16_t v_bs = vdupq_n_u8('\\');
            const uint8x16_t v_cr = vdupq_n_u8('\r');
            while (s + 16 <= send) {
                uint8x16_t v = vld1q_u8(s);
                uint8x16_t m = vorrq_u8(vceqq_u8(v, v_bs), vceqq_u8(v, v_cr));
                uint64_t mask = vget_lane_u64(vreinterpret_u64_u8(vshrn_n_u16(vreinterpretq_u16_u8(m), 4)), 0);
                if (mask == 0) { s += 16; continue; }
                s += (size_t)(__builtin_ctzll(mask) >> 2);
                break;
            }
#endif
            while (s < send && *s != '\\' && *s != '\r') s++;
            size_t clen = (size_t)(s - chunk);
            if (clen) {
                if (prev_cr) { dst[d++] = (chunk[0] == '\n') ? '\n' : '\n'; if (chunk[0] == '\n') { chunk++; clen--; } prev_cr = false; }
                if (clen) { sg_memcpy(dst + d, chunk, clen); d += clen; }
            }
            if (s >= send) break;
            uint8_t b = *s++;
            if (b == '\r') {
                if (prev_cr) dst[d++] = '\n';
                prev_cr = true;
                continue;
            }
            /* b == '\\' */
            if (s >= send) break;
            uint8_t esc = *s++;
            uint8_t ch;
            switch (esc) {
                case '"':  ch = '"';  break;
                case '\\': ch = '\\'; break;
                case '/':  ch = '/';  break;
                case 'b':  ch = '\b'; break;
                case 'f':  ch = '\f'; break;
                case 'n':  ch = '\n'; break;
                case 'r':  ch = '\r'; break;
                case 't':  ch = '\t'; break;
                case 'u':
                    if (s + 4 <= send) {
                        uint32_t cp = sg_hex4(s); s += 4;
                        if (cp >= 0xD800 && cp <= 0xDBFF && s + 6 <= send && s[0] == '\\' && s[1] == 'u') {
                            uint32_t lo = sg_hex4(s + 2);
                            if (lo >= 0xDC00 && lo <= 0xDFFF) {
                                cp = 0x10000 + (((cp - 0xD800) << 10) | (lo - 0xDC00));
                                s += 6;
                            }
                        }
                        if (prev_cr) { dst[d++] = '\n'; prev_cr = false; }
                        d += sg_encode_utf8(cp, (uint8_t *)(dst + d));
                        continue;
                    }
                    ch = 'u';
                    break;
                default: ch = esc; break;
            }
            if (ch == '\r') {
                if (prev_cr) dst[d++] = '\n';
                prev_cr = true;
            } else if (ch == '\n') {
                dst[d++] = '\n';
                prev_cr = false;
            } else {
                if (prev_cr) { dst[d++] = '\n'; prev_cr = false; }
                dst[d++] = (char)ch;
            }
        }
        if (prev_cr) dst[d++] = '\n';
    }
    dst[d] = '\0';
    out->ptr = dst;
    out->len = (uint32_t)d;
    return after;
}

static const uint8_t *sg_skip_json_value(const uint8_t *p, const uint8_t *end) {
    p = sg_skip_ws(p, end);
    if (p >= end) return p;
    uint8_t c = *p;
    if (c == '"') {
        const uint8_t *s; size_t l;
        return sg_skip_json_str(p, end, &s, &l);
    }
    if (c == '{' || c == '[') {
        int depth = 1;
        p++;
        while (p < end && depth > 0) {
            uint8_t b = *p;
            if (b == '"') {
                const uint8_t *s; size_t l;
                p = sg_skip_json_str(p, end, &s, &l);
            } else if (b == '{' || b == '[') {
                depth++; p++;
            } else if (b == '}' || b == ']') {
                depth--; p++;
            } else {
                p++;
            }
        }
        return p;
    }
    while (p < end && *p != ',' && *p != '}' && *p != ']' && *p != ' ' && *p != '\t' && *p != '\n' && *p != '\r') p++;
    return p;
}

static const uint8_t *sg_parse_uint(const uint8_t *p, const uint8_t *end, uint32_t *out) {
    p = sg_skip_ws(p, end);
    uint32_t v = 0;
    while (p < end && (uint8_t)(*p - '0') <= 9u) {
        v = v * 10u + (uint32_t)(*p - '0');
        p++;
    }
    *out = v;
    return p;
}

static const uint8_t *sg_parse_case_input(SgArena *a, const uint8_t *p, const uint8_t *end, SgCaseInput *out) {
    sg_memset(out, 0, sizeof(*out));
    p = sg_skip_ws(p, end);
    if (p >= end || *p != '{') return p;
    p++;

    while (p < end) {
        p = sg_skip_ws(p, end);
        if (p >= end) break;
        if (*p == '}') { p++; break; }
        if (*p == ',') { p++; continue; }

        SgStr key = {0};
        p = sg_parse_json_str_to_arena(a, p, end, false, &key);
        p = sg_skip_ws(p, end);
        if (p < end && *p == ':') p++;
        p = sg_skip_ws(p, end);

        if (key.len == 4 && sg_memcmp(key.ptr, "site", 4) == 0) {
            if (p < end && *p == '{') {
                p++;
                while (p < end) {
                    p = sg_skip_ws(p, end);
                    if (*p == '}') { p++; break; }
                    if (*p == ',') { p++; continue; }
                    SgStr sk = {0};
                    p = sg_parse_json_str_to_arena(a, p, end, false, &sk);
                    p = sg_skip_ws(p, end);
                    if (*p == ':') p++;
                    p = sg_skip_ws(p, end);
                    if (sk.len == 5 && sg_memcmp(sk.ptr, "title", 5) == 0) {
                        p = sg_parse_json_str_to_arena(a, p, end, false, &out->site_title);
                    } else if (sk.len == 8 && sg_memcmp(sk.ptr, "base_url", 8) == 0) {
                        p = sg_parse_json_str_to_arena(a, p, end, false, &out->site_base_url);
                    } else if (sk.len == 8 && sg_memcmp(sk.ptr, "per_page", 8) == 0) {
                        p = sg_parse_uint(p, end, &out->per_page);
                    } else if (sk.len == 6 && sg_memcmp(sk.ptr, "recent", 6) == 0) {
                        p = sg_parse_uint(p, end, &out->recent);
                    } else {
                        p = sg_skip_json_value(p, end);
                    }
                }
            }
        } else if (key.len == 9 && sg_memcmp(key.ptr, "templates", 9) == 0) {
            if (p < end && *p == '{') {
                p++;
                uint32_t cap = 64;
                out->templates = (SgRawTemplate *)sg_arena_alloc(a, cap * sizeof(SgRawTemplate), 8);
                while (p < end) {
                    p = sg_skip_ws(p, end);
                    if (*p == '}') { p++; break; }
                    if (*p == ',') { p++; continue; }
                    if (out->tpl_count == cap) {
                        uint32_t ncap = cap * 2;
                        SgRawTemplate *nt = (SgRawTemplate *)sg_arena_alloc(a, ncap * sizeof(SgRawTemplate), 8);
                        sg_memcpy(nt, out->templates, cap * sizeof(SgRawTemplate));
                        out->templates = nt;
                        cap = ncap;
                    }
                    SgRawTemplate *t = &out->templates[out->tpl_count++];
                    p = sg_parse_json_str_to_arena(a, p, end, false, &t->name);
                    p = sg_skip_ws(p, end);
                    if (*p == ':') p++;
                    p = sg_skip_ws(p, end);
                    p = sg_parse_json_str_to_arena(a, p, end, false, &t->src);
                }
            }
        } else if (key.len == 5 && sg_memcmp(key.ptr, "files", 5) == 0) {
            if (p < end && *p == '[') {
                p++;
                uint32_t cap = 1024;
                out->files = (SgRawFile *)sg_arena_alloc(a, cap * sizeof(SgRawFile), 8);
                while (p < end) {
                    p = sg_skip_ws(p, end);
                    if (*p == ']') { p++; break; }
                    if (*p == ',') { p++; continue; }
                    if (out->file_count == cap) {
                        uint32_t ncap = cap * 2;
                        SgRawFile *nf = (SgRawFile *)sg_arena_alloc(a, ncap * sizeof(SgRawFile), 8);
                        sg_memcpy(nf, out->files, cap * sizeof(SgRawFile));
                        out->files = nf;
                        cap = ncap;
                    }
                    SgRawFile *f = &out->files[out->file_count++];
                    sg_memset(f, 0, sizeof(*f));
                    const uint8_t *deferred_raw_start = NULL;
                    size_t deferred_raw_len = 0;
                    bool got_path = false;
                    if (*p == '{') {
                        p++;
                        while (p < end) {
                            p = sg_skip_ws(p, end);
                            if (*p == '}') { p++; break; }
                            if (*p == ',') { p++; continue; }
                            SgStr fk = {0};
                            p = sg_parse_json_str_to_arena(a, p, end, false, &fk);
                            p = sg_skip_ws(p, end);
                            if (*p == ':') p++;
                            p = sg_skip_ws(p, end);
                            if (fk.len == 4 && sg_memcmp(fk.ptr, "path", 4) == 0) {
                                p = sg_parse_json_str_to_arena(a, p, end, false, &f->path);
                                got_path = true;
                                f->is_md = (f->path.len >= 3 && f->path.ptr[f->path.len - 3] == '.' &&
                                            f->path.ptr[f->path.len - 2] == 'm' && f->path.ptr[f->path.len - 1] == 'd');
                            } else if (fk.len == 7 && sg_memcmp(fk.ptr, "content", 7) == 0) {
                                if (got_path && f->is_md) {
                                    p = sg_parse_json_str_to_arena(a, p, end, true, &f->md_text);
                                } else {
                                    const uint8_t *inn = NULL; size_t ilen = 0;
                                    const uint8_t *qstart = p;
                                    p = sg_skip_json_str(p, end, &inn, &ilen);
                                    deferred_raw_start = qstart;
                                    deferred_raw_len = (size_t)(p - qstart);
                                    f->raw_ptr = (const char *)inn;
                                    f->raw_len = (uint32_t)ilen;
                                }
                            } else {
                                p = sg_skip_json_value(p, end);
                            }
                        }
                        if (f->is_md && f->md_text.ptr == NULL && deferred_raw_start != NULL) {
                            sg_parse_json_str_to_arena(a, deferred_raw_start, deferred_raw_start + deferred_raw_len, true, &f->md_text);
                        }
                    }
                }
            }
        } else {
            p = sg_skip_json_value(p, end);
        }
    }
    return p;
}
