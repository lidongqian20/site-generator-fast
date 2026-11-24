#pragma once
#include "slug_cmp.h"

/* Exact makefun-site lib/markdown.js implementation */

typedef struct {
    uint32_t level;
    SgStr id;
    SgStr html;
} SgTocItem;

typedef struct {
    SgStr *keys;
    uint32_t *vals; /* index in sources array + 1 */
    uint32_t cap;   /* power of 2 */
} SgSourceMap;

typedef struct {
    SgStr *keys;
    uint32_t cap;
    uint32_t count;
} SgStrSet;

SG_INLINE bool sg_strset_insert(SgArena *a, SgStrSet *set, const char *s, size_t len) {
    if (SG_UNLIKELY((set->count + 1) * 2 >= set->cap)) {
        uint32_t ncap = set->cap ? set->cap * 2 : 64;
        SgStr *nk = (SgStr *)sg_arena_alloc(a, ncap * sizeof(SgStr), 8);
        sg_memset(nk, 0, ncap * sizeof(SgStr));
        for (uint32_t i = 0; i < set->cap; i++) {
            if (set->keys[i].ptr != NULL) {
                uint64_t h = sg_fnv1a(set->keys[i].ptr, set->keys[i].len);
                uint32_t idx = (uint32_t)h & (ncap - 1);
                while (nk[idx].ptr != NULL) idx = (idx + 1) & (ncap - 1);
                nk[idx] = set->keys[i];
            }
        }
        set->keys = nk;
        set->cap = ncap;
    }
    uint64_t h = sg_fnv1a(s, len);
    uint32_t idx = (uint32_t)h & (set->cap - 1);
    while (set->keys[idx].ptr != NULL) {
        if (set->keys[idx].len == len && sg_memcmp(set->keys[idx].ptr, s, len) == 0) return false;
        idx = (idx + 1) & (set->cap - 1);
    }
    set->keys[idx].ptr = sg_arena_strdup(a, s, len);
    set->keys[idx].len = (uint32_t)len;
    set->count++;
    return true;
}

SG_INLINE bool sg_strset_has(const SgStrSet *set, const char *s, size_t len) {
    if (set->cap == 0) return false;
    uint64_t h = sg_fnv1a(s, len);
    uint32_t idx = (uint32_t)h & (set->cap - 1);
    while (set->keys[idx].ptr != NULL) {
        if (set->keys[idx].len == len && sg_memcmp(set->keys[idx].ptr, s, len) == 0) return true;
        idx = (idx + 1) & (set->cap - 1);
    }
    return false;
}

SG_INLINE uint32_t sg_srcmap_get(const SgSourceMap *m, const char *s, size_t len) {
    if (m->cap == 0) return 0;
    uint64_t h = sg_fnv1a(s, len);
    uint32_t idx = (uint32_t)h & (m->cap - 1);
    while (m->keys[idx].ptr != NULL) {
        if (m->keys[idx].len == len && sg_memcmp(m->keys[idx].ptr, s, len) == 0) return m->vals[idx];
        idx = (idx + 1) & (m->cap - 1);
    }
    return 0;
}

typedef struct {
    SgStr src_path;
    SgStr src_dir;
    SgStr base;
    const SgSourceMap *by_source;
    const SgStr *src_urls;
    const SgStrSet *static_paths;
    SgStrSet *broken;
    SgArena *arena;
    char resolve_tmp[4096];
} SgMdEnv;

static inline int sg_resolve_rel_path(SgStr dir, const char *rel, size_t rlen, char *out) {
    uint16_t seg_ends[128];
    int depth = 0;
    size_t olen = 0;
    if (dir.len > 0) {
        sg_memcpy(out, dir.ptr, dir.len);
        olen = dir.len;
        for (size_t i = 0; i < dir.len; i++) {
            if (dir.ptr[i] == '/') seg_ends[depth++] = (uint16_t)i;
        }
        seg_ends[depth++] = (uint16_t)olen;
    }
    size_t i = 0;
    while (i < rlen) {
        while (i < rlen && rel[i] == '/') i++;
        if (i >= rlen) break;
        size_t s = i;
        while (i < rlen && rel[i] != '/') i++;
        size_t slen = i - s;
        if (slen == 1 && rel[s] == '.') continue;
        if (slen == 2 && rel[s] == '.' && rel[s + 1] == '.') {
            if (depth == 0) return -1;
            depth--;
            olen = (depth == 0) ? 0 : seg_ends[depth - 1];
            continue;
        }
        if (olen > 0) out[olen++] = '/';
        sg_memcpy(out + olen, rel + s, slen);
        olen += slen;
        if (depth < 128) seg_ends[depth++] = (uint16_t)olen;
    }
    return (int)olen;
}

SG_INLINE bool sg_has_scheme(const char *s, size_t len) {
    if (len < 2) return false;
    uint8_t c0 = (uint8_t)s[0];
    if (!((uint8_t)(c0 - 'A') <= 25u || (uint8_t)(c0 - 'a') <= 25u)) return false;
    for (size_t i = 1; i < len; i++) {
        uint8_t c = (uint8_t)s[i];
        if (c == ':') return true;
        if (!((uint8_t)(c - 'A') <= 25u || (uint8_t)(c - 'a') <= 25u ||
              (uint8_t)(c - '0') <= 9u || c == '+' || c == '.' || c == '-')) return false;
    }
    return false;
}

static SgStr sg_env_link(SgMdEnv *env, const char *target, size_t tlen, char *out_buf, size_t out_cap) {
    if (tlen > 0 && (target[0] == '/' || target[0] == '#')) return (SgStr){ target, (uint32_t)tlen };
    if (sg_has_scheme(target, tlen)) return (SgStr){ target, (uint32_t)tlen };
    size_t hash = tlen;
    for (size_t i = 0; i < tlen; i++) { if (target[i] == '#') { hash = i; break; } }
    size_t rlen = hash;
    size_t flen = tlen - hash;
    const char *frag = target + hash;

    int plen = -1;
    if (rlen > 0 && env->src_dir.len + 1 + rlen < sizeof(env->resolve_tmp)) {
        plen = sg_resolve_rel_path(env->src_dir, target, rlen, env->resolve_tmp);
    }
    if (plen >= 0) {
        uint32_t sidx = sg_srcmap_get(env->by_source, env->resolve_tmp, (size_t)plen);
        if (sidx != 0) {
            SgStr u = env->src_urls[sidx - 1];
            if (u.len + flen <= out_cap) {
                sg_memcpy(out_buf, u.ptr, u.len);
                if (flen) sg_memcpy(out_buf + u.len, frag, flen);
                return (SgStr){ out_buf, u.len + (uint32_t)flen };
            }
        }
        if (sg_strset_has(env->static_paths, env->resolve_tmp, (size_t)plen)) {
            size_t need = env->base.len + 1 + (size_t)plen + flen;
            if (need <= out_cap) {
                size_t p = 0;
                sg_memcpy(out_buf + p, env->base.ptr, env->base.len); p += env->base.len;
                out_buf[p++] = '/';
                sg_memcpy(out_buf + p, env->resolve_tmp, (size_t)plen); p += (size_t)plen;
                if (flen) { sg_memcpy(out_buf + p, frag, flen); p += flen; }
                return (SgStr){ out_buf, (uint32_t)p };
            }
        }
    }
    size_t blen = 12 + env->src_path.len + 4 + tlen;
    char *bstr = (char *)sg_arena_alloc(env->arena, blen + 1, 1);
    size_t bp = 0;
    sg_memcpy(bstr + bp, "broken-link ", 12); bp += 12;
    sg_memcpy(bstr + bp, env->src_path.ptr, env->src_path.len); bp += env->src_path.len;
    sg_memcpy(bstr + bp, " -> ", 4); bp += 4;
    sg_memcpy(bstr + bp, target, tlen); bp += tlen;
    bstr[bp] = '\0';
    sg_strset_insert(env->arena, env->broken, bstr, blen);
    return (SgStr){ target, (uint32_t)tlen };
}

static const uint8_t SG_MD_ESCAPABLE[256] = {
    ['\\'] = 1, ['`'] = 1, ['*'] = 1, ['_'] = 1, ['['] = 1, [']'] = 1, ['('] = 1,
    [')'] = 1, ['#'] = 1, ['+'] = 1, ['-'] = 1, ['.'] = 1, ['!'] = 1, ['>'] = 1
};

SG_INLINE bool sg_is_js_space_utf8(const uint8_t *p, size_t len, size_t *step) {
    uint8_t c = *p;
    if (c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\v' || c == '\f') {
        *step = 1; return true;
    }
    if (c < 0x80) { *step = 1; return false; }
    size_t pos = 0;
    uint32_t cp = (len >= 4) ? sg_decode_utf8_at(p, 4, &pos) : sg_decode_utf8_at(p, len, &pos);
    *step = pos;
    return cp == 0x00A0 || cp == 0x1680 || (cp >= 0x2000 && cp <= 0x200A) ||
           cp == 0x2028 || cp == 0x2029 || cp == 0x202F || cp == 0x205F || cp == 0x3000 || cp == 0xFEFF;
}

SG_INLINE void sg_html_esc_buf(SgBuf *out, const char *s, size_t len) {
    const uint8_t *p = (const uint8_t *)s;
    size_t i = 0;
    sg_buf_reserve(out, len * 6 + 64);
#if defined(SG_USE_AVX2)
    const __m256i v_amp = _mm256_set1_epi8('&');
    const __m256i v_lt  = _mm256_set1_epi8('<');
    const __m256i v_gt  = _mm256_set1_epi8('>');
    const __m256i v_dq  = _mm256_set1_epi8('"');
    const __m256i v_sq  = _mm256_set1_epi8('\'');
    while (i + 32 <= len) {
        __m256i v = _mm256_loadu_si256((const __m256i *)(p + i));
        __m256i m1 = _mm256_or_si256(_mm256_cmpeq_epi8(v, v_amp), _mm256_cmpeq_epi8(v, v_lt));
        __m256i m2 = _mm256_or_si256(_mm256_cmpeq_epi8(v, v_gt), _mm256_cmpeq_epi8(v, v_dq));
        __m256i m  = _mm256_or_si256(_mm256_or_si256(m1, m2), _mm256_cmpeq_epi8(v, v_sq));
        uint32_t mask = (uint32_t)_mm256_movemask_epi8(m);
        if (SG_LIKELY(mask == 0)) {
            sg_memcpy(out->data + out->len, p + i, 32);
            out->len += 32;
            i += 32;
            continue;
        }
        uint32_t k = (uint32_t)__builtin_ctz(mask);
        if (k > 0) {
            sg_memcpy(out->data + out->len, p + i, k);
            out->len += k;
            i += k;
        }
        uint8_t c = p[i++];
        if (c == '&') { sg_memcpy(out->data + out->len, "&amp;", 5); out->len += 5; }
        else if (c == '<') { sg_memcpy(out->data + out->len, "&lt;", 4); out->len += 4; }
        else if (c == '>') { sg_memcpy(out->data + out->len, "&gt;", 4); out->len += 4; }
        else if (c == '"') { sg_memcpy(out->data + out->len, "&quot;", 6); out->len += 6; }
        else { sg_memcpy(out->data + out->len, "&#39;", 5); out->len += 5; }
    }
#elif defined(SG_USE_SSE2)
    const __m128i v_amp = _mm_set1_epi8('&');
    const __m128i v_lt  = _mm_set1_epi8('<');
    const __m128i v_gt  = _mm_set1_epi8('>');
    const __m128i v_dq  = _mm_set1_epi8('"');
    const __m128i v_sq  = _mm_set1_epi8('\'');
    while (i + 16 <= len) {
        __m128i v = _mm_loadu_si128((const __m128i *)(p + i));
        __m128i m1 = _mm_or_si128(_mm_cmpeq_epi8(v, v_amp), _mm_cmpeq_epi8(v, v_lt));
        __m128i m2 = _mm_or_si128(_mm_cmpeq_epi8(v, v_gt), _mm_cmpeq_epi8(v, v_dq));
        __m128i m  = _mm_or_si128(_mm_or_si128(m1, m2), _mm_cmpeq_epi8(v, v_sq));
        uint32_t mask = (uint32_t)_mm_movemask_epi8(m);
        if (SG_LIKELY(mask == 0)) {
            sg_memcpy(out->data + out->len, p + i, 16);
            out->len += 16;
            i += 16;
            continue;
        }
        uint32_t k = (uint32_t)__builtin_ctz(mask);
        if (k > 0) {
            sg_memcpy(out->data + out->len, p + i, k);
            out->len += k;
            i += k;
        }
        uint8_t c = p[i++];
        if (c == '&') { sg_memcpy(out->data + out->len, "&amp;", 5); out->len += 5; }
        else if (c == '<') { sg_memcpy(out->data + out->len, "&lt;", 4); out->len += 4; }
        else if (c == '>') { sg_memcpy(out->data + out->len, "&gt;", 4); out->len += 4; }
        else if (c == '"') { sg_memcpy(out->data + out->len, "&quot;", 6); out->len += 6; }
        else { sg_memcpy(out->data + out->len, "&#39;", 5); out->len += 5; }
    }
#elif defined(SG_USE_NEON)
    const uint8x16_t v_amp = vdupq_n_u8('&');
    const uint8x16_t v_lt  = vdupq_n_u8('<');
    const uint8x16_t v_gt  = vdupq_n_u8('>');
    const uint8x16_t v_dq  = vdupq_n_u8('"');
    const uint8x16_t v_sq  = vdupq_n_u8('\'');
    while (i + 16 <= len) {
        uint8x16_t v = vld1q_u8(p + i);
        uint8x16_t m = vorrq_u8(vorrq_u8(vceqq_u8(v, v_amp), vceqq_u8(v, v_lt)),
                                vorrq_u8(vorrq_u8(vceqq_u8(v, v_gt), vceqq_u8(v, v_dq)), vceqq_u8(v, v_sq)));
        uint64_t mask = vget_lane_u64(vreinterpret_u64_u8(vshrn_n_u16(vreinterpretq_u16_u8(m), 4)), 0);
        if (SG_LIKELY(mask == 0)) {
            sg_memcpy(out->data + out->len, p + i, 16);
            out->len += 16;
            i += 16;
            continue;
        }
        uint32_t k = (uint32_t)(__builtin_ctzll(mask) >> 2);
        if (k > 0) {
            sg_memcpy(out->data + out->len, p + i, k);
            out->len += k;
            i += k;
        }
        uint8_t c = p[i++];
        if (c == '&') { sg_memcpy(out->data + out->len, "&amp;", 5); out->len += 5; }
        else if (c == '<') { sg_memcpy(out->data + out->len, "&lt;", 4); out->len += 4; }
        else if (c == '>') { sg_memcpy(out->data + out->len, "&gt;", 4); out->len += 4; }
        else if (c == '"') { sg_memcpy(out->data + out->len, "&quot;", 6); out->len += 6; }
        else { sg_memcpy(out->data + out->len, "&#39;", 5); out->len += 5; }
    }
#endif
    while (i < len) {
        uint8_t c = p[i++];
        if (c == '&') { sg_memcpy(out->data + out->len, "&amp;", 5); out->len += 5; }
        else if (c == '<') { sg_memcpy(out->data + out->len, "&lt;", 4); out->len += 4; }
        else if (c == '>') { sg_memcpy(out->data + out->len, "&gt;", 4); out->len += 4; }
        else if (c == '"') { sg_memcpy(out->data + out->len, "&quot;", 6); out->len += 6; }
        else if (c == '\'') { sg_memcpy(out->data + out->len, "&#39;", 5); out->len += 5; }
        else out->data[out->len++] = c;
    }
}

SG_INLINE ptrdiff_t sg_find_unescaped_ch(const char *s, size_t len, char target, size_t from) {
    for (size_t j = from; j < len; j++) {
        uint8_t c = (uint8_t)s[j];
        if (c == '\\' && j + 1 < len && SG_MD_ESCAPABLE[(uint8_t)s[j + 1]]) { j++; continue; }
        if (c == (uint8_t)target) return (ptrdiff_t)j;
    }
    return -1;
}

SG_INLINE ptrdiff_t sg_find_unescaped_dblstar(const char *s, size_t len, size_t from) {
    for (size_t j = from; j + 1 < len; j++) {
        uint8_t c = (uint8_t)s[j];
        if (c == '\\' && j + 1 < len && SG_MD_ESCAPABLE[(uint8_t)s[j + 1]]) { j++; continue; }
        if (c == '*' && s[j + 1] == '*') return (ptrdiff_t)j;
    }
    return -1;
}

SG_INLINE ptrdiff_t sg_find_single_star(const char *s, size_t len, size_t from) {
    for (size_t j = from; j < len; j++) {
        uint8_t c = (uint8_t)s[j];
        if (c == '\\' && j + 1 < len && SG_MD_ESCAPABLE[(uint8_t)s[j + 1]]) { j++; continue; }
        if (c != '*') continue;
        if (j + 1 < len && s[j + 1] == '*') { j++; continue; }
        return (ptrdiff_t)j;
    }
    return -1;
}

static void sg_md_inline(const char *s, size_t len, SgMdEnv *env, SgBuf *out) {
    const uint8_t *p = (const uint8_t *)s;
    size_t i = 0;
    while (i < len) {
#if defined(SG_USE_AVX2)
        const __m256i v_bs   = _mm256_set1_epi8('\\');
        const __m256i v_tick = _mm256_set1_epi8('`');
        const __m256i v_star = _mm256_set1_epi8('*');
        const __m256i v_lbr  = _mm256_set1_epi8('[');
        const __m256i v_amp  = _mm256_set1_epi8('&');
        const __m256i v_lt   = _mm256_set1_epi8('<');
        const __m256i v_gt   = _mm256_set1_epi8('>');
        const __m256i v_dq   = _mm256_set1_epi8('"');
        const __m256i v_sq   = _mm256_set1_epi8('\'');
        while (i + 32 <= len) {
            __m256i v = _mm256_loadu_si256((const __m256i *)(p + i));
            __m256i m1 = _mm256_or_si256(_mm256_cmpeq_epi8(v, v_bs), _mm256_cmpeq_epi8(v, v_tick));
            __m256i m2 = _mm256_or_si256(_mm256_cmpeq_epi8(v, v_star), _mm256_cmpeq_epi8(v, v_lbr));
            __m256i m3 = _mm256_or_si256(_mm256_cmpeq_epi8(v, v_amp), _mm256_cmpeq_epi8(v, v_lt));
            __m256i m4 = _mm256_or_si256(_mm256_cmpeq_epi8(v, v_gt), _mm256_cmpeq_epi8(v, v_dq));
            __m256i m  = _mm256_or_si256(_mm256_or_si256(m1, m2), _mm256_or_si256(_mm256_or_si256(m3, m4), _mm256_cmpeq_epi8(v, v_sq)));
            uint32_t mask = (uint32_t)_mm256_movemask_epi8(m);
            if (SG_LIKELY(mask == 0)) {
                sg_buf_write_s(out, p + i, 32);
                i += 32;
                continue;
            }
            uint32_t k = (uint32_t)__builtin_ctz(mask);
            if (k > 0) {
                sg_buf_write_s(out, p + i, k);
                i += k;
            }
            break;
        }
#elif defined(SG_USE_SSE2)
        const __m128i v_bs   = _mm_set1_epi8('\\');
        const __m128i v_tick = _mm_set1_epi8('`');
        const __m128i v_star = _mm_set1_epi8('*');
        const __m128i v_lbr  = _mm_set1_epi8('[');
        const __m128i v_amp  = _mm_set1_epi8('&');
        const __m128i v_lt   = _mm_set1_epi8('<');
        const __m128i v_gt   = _mm_set1_epi8('>');
        const __m128i v_dq   = _mm_set1_epi8('"');
        const __m128i v_sq   = _mm_set1_epi8('\'');
        while (i + 16 <= len) {
            __m128i v = _mm_loadu_si128((const __m128i *)(p + i));
            __m128i m1 = _mm_or_si128(_mm_cmpeq_epi8(v, v_bs), _mm_cmpeq_epi8(v, v_tick));
            __m128i m2 = _mm_or_si128(_mm_cmpeq_epi8(v, v_star), _mm_cmpeq_epi8(v, v_lbr));
            __m128i m3 = _mm_or_si128(_mm_cmpeq_epi8(v, v_amp), _mm_cmpeq_epi8(v, v_lt));
            __m128i m4 = _mm_or_si128(_mm_cmpeq_epi8(v, v_gt), _mm_cmpeq_epi8(v, v_dq));
            __m128i m  = _mm_or_si128(_mm_or_si128(m1, m2), _mm_or_si128(_mm_or_si128(m3, m4), _mm_cmpeq_epi8(v, v_sq)));
            uint32_t mask = (uint32_t)_mm_movemask_epi8(m);
            if (SG_LIKELY(mask == 0)) {
                sg_buf_write_s(out, p + i, 16);
                i += 16;
                continue;
            }
            uint32_t k = (uint32_t)__builtin_ctz(mask);
            if (k > 0) {
                sg_buf_write_s(out, p + i, k);
                i += k;
            }
            break;
        }
#elif defined(SG_USE_NEON)
        const uint8x16_t v_bs   = vdupq_n_u8('\\');
        const uint8x16_t v_tick = vdupq_n_u8('`');
        const uint8x16_t v_star = vdupq_n_u8('*');
        const uint8x16_t v_lbr  = vdupq_n_u8('[');
        const uint8x16_t v_amp  = vdupq_n_u8('&');
        const uint8x16_t v_lt   = vdupq_n_u8('<');
        const uint8x16_t v_gt   = vdupq_n_u8('>');
        const uint8x16_t v_dq   = vdupq_n_u8('"');
        const uint8x16_t v_sq   = vdupq_n_u8('\'');
        while (i + 16 <= len) {
            uint8x16_t v = vld1q_u8(p + i);
            uint8x16_t m1 = vorrq_u8(vceqq_u8(v, v_bs), vceqq_u8(v, v_tick));
            uint8x16_t m2 = vorrq_u8(vceqq_u8(v, v_star), vceqq_u8(v, v_lbr));
            uint8x16_t m3 = vorrq_u8(vceqq_u8(v, v_amp), vceqq_u8(v, v_lt));
            uint8x16_t m4 = vorrq_u8(vceqq_u8(v, v_gt), vceqq_u8(v, v_dq));
            uint8x16_t m  = vorrq_u8(vorrq_u8(m1, m2), vorrq_u8(vorrq_u8(m3, m4), vceqq_u8(v, v_sq)));
            uint64_t mask = vget_lane_u64(vreinterpret_u64_u8(vshrn_n_u16(vreinterpretq_u16_u8(m), 4)), 0);
            if (SG_LIKELY(mask == 0)) {
                sg_buf_write_s(out, p + i, 16);
                i += 16;
                continue;
            }
            uint32_t k = (uint32_t)(__builtin_ctzll(mask) >> 2);
            if (k > 0) {
                sg_buf_write_s(out, p + i, k);
                i += k;
            }
            break;
        }
#endif
        if (i >= len) break;
        uint8_t c = p[i];
        if (c == '\\' && i + 1 < len && SG_MD_ESCAPABLE[p[i + 1]]) {
            uint8_t n = p[i + 1];
            if (n == '>') sg_buf_write_s(out, "&gt;", 4);
            else sg_buf_putc_s(out, (char)n);
            i += 2;
            continue;
        }
        if (c == '`') {
            ptrdiff_t j = -1;
            for (size_t q = i + 1; q < len; q++) { if (p[q] == '`') { j = (ptrdiff_t)q; break; } }
            if (j > (ptrdiff_t)(i + 1)) {
                sg_buf_write_s(out, "<code>", 6);
                sg_html_esc_buf(out, s + i + 1, (size_t)j - (i + 1));
                sg_buf_write_s(out, "</code>", 7);
                i = (size_t)j + 1;
            } else {
                sg_buf_putc_s(out, '`');
                i++;
            }
            continue;
        }
        if (c == '*') {
            if (i + 1 < len && p[i + 1] == '*') {
                ptrdiff_t j = sg_find_unescaped_dblstar(s, len, i + 2);
                if (j > (ptrdiff_t)(i + 2)) {
                    sg_buf_write_s(out, "<strong>", 8);
                    sg_md_inline(s + i + 2, (size_t)j - (i + 2), env, out);
                    sg_buf_write_s(out, "</strong>", 9);
                    i = (size_t)j + 2;
                } else {
                    sg_buf_write_s(out, "**", 2);
                    i += 2;
                }
                continue;
            }
            ptrdiff_t j = sg_find_single_star(s, len, i + 1);
            if (j > (ptrdiff_t)(i + 1)) {
                sg_buf_write_s(out, "<em>", 4);
                sg_md_inline(s + i + 1, (size_t)j - (i + 1), env, out);
                sg_buf_write_s(out, "</em>", 5);
                i = (size_t)j + 1;
            } else {
                sg_buf_putc_s(out, '*');
                i++;
            }
            continue;
        }
        if (c == '[') {
            ptrdiff_t k = sg_find_unescaped_ch(s, len, ']', i + 1);
            ptrdiff_t m = (k != -1 && (size_t)k + 1 < len && s[k + 1] == '(') ? -1 : -2;
            if (m == -1) {
                for (size_t q = (size_t)k + 2; q < len; q++) { if (s[q] == ')') { m = (ptrdiff_t)q; break; } }
            }
            if (m > (ptrdiff_t)(k + 2)) {
                const char *tgt = s + k + 2;
                size_t tlen = (size_t)m - (size_t)(k + 2);
                bool has_sp = false;
                for (size_t q = 0; q < tlen; ) {
                    size_t st = 1;
                    if (sg_is_js_space_utf8((const uint8_t *)(tgt + q), tlen - q, &st)) { has_sp = true; break; }
                    q += st;
                }
                if (!has_sp) {
                    char href_buf[4096];
                    SgStr href = sg_env_link(env, tgt, tlen, href_buf, sizeof(href_buf));
                    sg_buf_write_s(out, "<a href=\"", 9);
                    sg_html_esc_buf(out, href.ptr, href.len);
                    sg_buf_write_s(out, "\">", 2);
                    sg_md_inline(s + i + 1, (size_t)k - (i + 1), env, out);
                    sg_buf_write_s(out, "</a>", 4);
                    i = (size_t)m + 1;
                    continue;
                }
            }
            sg_buf_putc_s(out, '[');
            i++;
            continue;
        }
        if (c == '&') sg_buf_write_s(out, "&amp;", 5);
        else if (c == '<') sg_buf_write_s(out, "&lt;", 4);
        else if (c == '>') sg_buf_write_s(out, "&gt;", 4);
        else if (c == '"') sg_buf_write_s(out, "&quot;", 6);
        else if (c == '\'') sg_buf_write_s(out, "&#39;", 5);
        else sg_buf_putc_s(out, (char)c);
        i++;
        size_t run_s = i;
        while (i < len) {
            uint8_t b = p[i];
            if (b == '\\' || b == '`' || b == '*' || b == '[' || b == '&' || b == '<' || b == '>' || b == '"' || b == '\'') break;
            i++;
        }
        if (i > run_s) sg_buf_write_s(out, p + run_s, i - run_s);
    }
}

SG_INLINE bool sg_md_is_blank(const char *l, size_t len) {
    for (size_t i = 0; i < len; i++) {
        if (l[i] != ' ' && l[i] != '\t') return false;
    }
    return true;
}

SG_INLINE bool sg_md_is_fence_open(const char *l, size_t len, SgStr *lang) {
    if (len < 3 || l[0] != '`' || l[1] != '`' || l[2] != '`') return false;
    size_t i = 3;
    while (i < len) {
        char c = l[i];
        if ((uint8_t)(c - 'a') <= 25u || (uint8_t)(c - 'A') <= 25u || (uint8_t)(c - '0') <= 9u ||
            c == '_' || c == '+' || c == '-') i++;
        else break;
    }
    lang->ptr = l + 3;
    lang->len = (uint32_t)(i - 3);
    while (i < len) { if (l[i] != ' ' && l[i] != '\t') return false; i++; }
    return true;
}

SG_INLINE bool sg_md_is_fence_close(const char *l, size_t len) {
    if (len < 3 || l[0] != '`' || l[1] != '`' || l[2] != '`') return false;
    for (size_t i = 3; i < len; i++) { if (l[i] != ' ' && l[i] != '\t') return false; }
    return true;
}

SG_INLINE bool sg_md_heading_of(const char *l, size_t len, uint32_t *depth, SgStr *text) {
    if (len == 0 || l[0] != '#') return false;
    size_t d = 0;
    while (d < 6 && d < len && l[d] == '#') d++;
    if (d == len || (l[d] != ' ' && l[d] != '\t')) return false;
    size_t s = d + 1;
    while (s < len && (l[s] == ' ' || l[s] == '\t')) s++;
    size_t e = len;
    while (e > s && (l[e - 1] == ' ' || l[e - 1] == '\t')) e--;
    if (e == s) return false;
    *depth = (uint32_t)d;
    text->ptr = l + s;
    text->len = (uint32_t)(e - s);
    return true;
}

SG_INLINE bool sg_md_is_hr(const char *l, size_t len) {
    if (len < 3) return false;
    if (!((l[0] == '-' && l[1] == '-' && l[2] == '-') ||
          (l[0] == '*' && l[1] == '*' && l[2] == '*'))) return false;
    for (size_t i = 3; i < len; i++) { if (l[i] != ' ' && l[i] != '\t') return false; }
    return true;
}

SG_INLINE bool sg_md_is_bullet(const char *l, size_t len, SgStr *rest) {
    if (len < 2 || (l[0] != '-' && l[0] != '*') || l[1] != ' ') return false;
    rest->ptr = l + 2;
    rest->len = (uint32_t)(len - 2);
    return true;
}

SG_INLINE bool sg_md_is_ordered(const char *l, size_t len, uint32_t *num, SgStr *rest) {
    size_t d = 0;
    uint32_t v = 0;
    while (d < 9 && d < len && (uint8_t)(l[d] - '0') <= 9u) {
        v = v * 10u + (uint32_t)(l[d] - '0');
        d++;
    }
    if (d == 0 || d + 1 >= len || l[d] != '.' || l[d + 1] != ' ') return false;
    *num = v;
    rest->ptr = l + d + 2;
    rest->len = (uint32_t)(len - (d + 2));
    return true;
}

SG_INLINE bool sg_md_is_continued(const char *l, size_t len) {
    if (len < 3 || l[0] != ' ' || l[1] != ' ') return false;
    size_t i = 2;
    while (i < len && l[i] == ' ') i++;
    if (i >= len) return false;
    size_t st = 1;
    return !sg_is_js_space_utf8((const uint8_t *)(l + i), len - i, &st);
}

SG_INLINE SgStr sg_md_trim_line(const char *l, size_t len) {
    size_t s = 0, e = len;
    while (s < e && (l[s] == ' ' || l[s] == '\t')) s++;
    while (e > s && (l[e - 1] == ' ' || l[e - 1] == '\t')) e--;
    return (SgStr){ l + s, (uint32_t)(e - s) };
}

typedef struct {
    SgBuf html_buf;
    SgBuf join_buf;
    SgTocItem *toc;
    uint32_t toc_cap;
    SgStr *heading_ids;
    uint32_t heading_id_cap;
} SgMdScratch;

static inline bool sg_md_id_taken(const SgStr *ids, uint32_t count, const char *s, size_t len) {
    for (uint32_t i = 0; i < count; i++) {
        if (ids[i].len == len && sg_memcmp(ids[i].ptr, s, len) == 0) return true;
    }
    return false;
}

typedef struct {
    SgStr html;
    SgStr excerpt;
    SgTocItem *toc;
    uint32_t toc_count;
} SgMdResult;

static void sg_render_markdown(SgMdScratch *sc, SgMdEnv *env, const char *src, size_t len, SgMdResult *res) {
    sc->html_buf.len = 0;
    SgArena *a = env->arena;
    uint32_t toc_count = 0;
    uint32_t id_count = 0;
    bool has_excerpt = false;
    res->excerpt = (SgStr){ "", 0 };

    size_t pos = 0;
    while (pos < len) {
        size_t ls = pos;
        size_t le = ls;
        while (le < len && src[le] != '\n') le++;
        size_t llen = le - ls;
        const char *line = src + ls;

        if (sg_md_is_blank(line, llen)) {
            pos = le + 1;
            continue;
        }
        SgStr lang;
        if (sg_md_is_fence_open(line, llen, &lang)) {
            pos = le + 1;
            sg_buf_write_s(&sc->html_buf, "<pre><code", 10);
            if (lang.len > 0) {
                sg_buf_write_s(&sc->html_buf, " class=\"language-", 17);
                sg_html_esc_buf(&sc->html_buf, lang.ptr, lang.len);
                sg_buf_putc_s(&sc->html_buf, '"');
            }
            sg_buf_putc_s(&sc->html_buf, '>');
            bool fence_closed = false;
            while (pos < len) {
                size_t bs = pos, be = bs;
                while (be < len && src[be] != '\n') be++;
                if (sg_md_is_fence_close(src + bs, be - bs)) {
                    pos = be + 1;
                    fence_closed = true;
                    break;
                }
                sg_html_esc_buf(&sc->html_buf, src + bs, be - bs);
                sg_buf_putc_s(&sc->html_buf, '\n');
                pos = be + 1;
            }
            if (!fence_closed && len > 0 && src[len - 1] == '\n') {
                sg_buf_putc_s(&sc->html_buf, '\n');
            }
            sg_buf_write_s(&sc->html_buf, "</code></pre>\n", 14);
            continue;
        }
        uint32_t depth;
        SgStr htext;
        if (sg_md_heading_of(line, llen, &depth, &htext)) {
            pos = le + 1;
            char slug_base[65];
            size_t slen = sg_slugify(htext.ptr, htext.len, slug_base);
            char slug_full[80];
            sg_memcpy(slug_full, slug_base, slen);
            size_t ulen = slen;
            if (sg_md_id_taken(sc->heading_ids, id_count, slug_full, ulen)) {
                for (uint32_t n = 2; ; n++) {
                    slug_full[slen] = '-';
                    char nbuf[12]; int ni = 12; uint32_t x = n;
                    do { nbuf[--ni] = (char)('0' + (x % 10u)); x /= 10u; } while (x);
                    size_t nl = (size_t)(12 - ni);
                    sg_memcpy(slug_full + slen + 1, nbuf + ni, nl);
                    ulen = slen + 1 + nl;
                    if (!sg_md_id_taken(sc->heading_ids, id_count, slug_full, ulen)) break;
                }
            }
            if (id_count == sc->heading_id_cap) {
                uint32_t nc = sc->heading_id_cap ? sc->heading_id_cap * 2 : 64;
                SgStr *ni = (SgStr *)sg_buf_mmap(nc * sizeof(SgStr));
                if (sc->heading_ids) { sg_memcpy(ni, sc->heading_ids, id_count * sizeof(SgStr)); sg_buf_free_raw(sc->heading_ids, sc->heading_id_cap * sizeof(SgStr)); }
                sc->heading_ids = ni; sc->heading_id_cap = nc;
            }
            char *id_arena = sg_arena_strdup(a, slug_full, ulen);
            sc->heading_ids[id_count++] = (SgStr){ id_arena, (uint32_t)ulen };

            sg_buf_write_s(&sc->html_buf, "<h", 2);
            sg_buf_putc_s(&sc->html_buf, (char)('0' + depth));
            sg_buf_write_s(&sc->html_buf, " id=\"", 5);
            sg_buf_write_s(&sc->html_buf, id_arena, ulen);
            sg_buf_write_s(&sc->html_buf, "\">", 2);
            size_t ih_start = sc->html_buf.len;
            sg_md_inline(htext.ptr, htext.len, env, &sc->html_buf);
            size_t ih_len = sc->html_buf.len - ih_start;
            if (depth == 2 || depth == 3) {
                if (toc_count == sc->toc_cap) {
                    uint32_t nc = sc->toc_cap ? sc->toc_cap * 2 : 64;
                    SgTocItem *nt = (SgTocItem *)sg_buf_mmap(nc * sizeof(SgTocItem));
                    if (sc->toc) { sg_memcpy(nt, sc->toc, toc_count * sizeof(SgTocItem)); sg_buf_free_raw(sc->toc, sc->toc_cap * sizeof(SgTocItem)); }
                    sc->toc = nt; sc->toc_cap = nc;
                }
                sc->toc[toc_count].level = depth;
                sc->toc[toc_count].id = (SgStr){ id_arena, (uint32_t)ulen };
                sc->toc[toc_count].html = (SgStr){ sg_arena_strdup(a, (const char *)(sc->html_buf.data + ih_start), ih_len), (uint32_t)ih_len };
                toc_count++;
            }
            sg_buf_write_s(&sc->html_buf, "</h", 3);
            sg_buf_putc_s(&sc->html_buf, (char)('0' + depth));
            sg_buf_write_s(&sc->html_buf, ">\n", 2);
            continue;
        }
        if (sg_md_is_hr(line, llen)) {
            pos = le + 1;
            sg_buf_write_s(&sc->html_buf, "<hr>\n", 5);
            continue;
        }
        if (llen > 0 && line[0] == '>') {
            sc->join_buf.len = 0;
            while (pos < len) {
                size_t qs = pos, qe = qs;
                while (qe < len && src[qe] != '\n') qe++;
                if (qe == qs || src[qs] != '>') break;
                size_t skip = (qs + 1 < qe && src[qs + 1] == ' ') ? 2 : 1;
                SgStr tr = sg_md_trim_line(src + qs + skip, qe - (qs + skip));
                if (tr.len > 0) {
                    if (sc->join_buf.len > 0) sg_buf_putc_s(&sc->join_buf, '\n');
                    sg_buf_write_s(&sc->join_buf, tr.ptr, tr.len);
                }
                pos = qe + 1;
            }
            if (sc->join_buf.len == 0) {
                sg_buf_write_s(&sc->html_buf, "<blockquote>\n</blockquote>\n", 27);
            } else {
                sg_buf_write_s(&sc->html_buf, "<blockquote>\n<p>", 16);
                sg_md_inline((const char *)sc->join_buf.data, sc->join_buf.len, env, &sc->html_buf);
                sg_buf_write_s(&sc->html_buf, "</p>\n</blockquote>\n", 19);
            }
            continue;
        }
        SgStr lrest;
        uint32_t lstart_num = 1;
        bool is_bullet = sg_md_is_bullet(line, llen, &lrest);
        bool is_ord = (!is_bullet) && sg_md_is_ordered(line, llen, &lstart_num, &lrest);
        if (is_bullet || is_ord) {
            const char *tag = is_ord ? "ol" : "ul";
            if (is_ord && lstart_num != 1) {
                sg_buf_write_s(&sc->html_buf, "<ol start=\"", 11);
                char nbuf[16]; int ni = 16; uint32_t x = lstart_num;
                do { nbuf[--ni] = (char)('0' + (x % 10u)); x /= 10u; } while (x);
                sg_buf_write_s(&sc->html_buf, nbuf + ni, (size_t)(16 - ni));
                sg_buf_write_s(&sc->html_buf, "\">\n", 3);
            } else {
                sg_buf_putc_s(&sc->html_buf, '<');
                sg_buf_write_s(&sc->html_buf, tag, 2);
                sg_buf_write_s(&sc->html_buf, ">\n", 2);
            }
            sc->join_buf.len = 0;
            bool have_item = false;
            while (pos < len) {
                size_t ls2 = pos, le2 = ls2;
                while (le2 < len && src[le2] != '\n') le2++;
                const char *l2 = src + ls2;
                size_t ll2 = le2 - ls2;
                SgStr mrest; uint32_t dnum;
                bool matched = is_ord ? sg_md_is_ordered(l2, ll2, &dnum, &mrest)
                                      : sg_md_is_bullet(l2, ll2, &mrest);
                if (matched) {
                    if (have_item) {
                        sg_buf_write_s(&sc->html_buf, "<li>", 4);
                        sg_md_inline((const char *)sc->join_buf.data, sc->join_buf.len, env, &sc->html_buf);
                        sg_buf_write_s(&sc->html_buf, "</li>\n", 6);
                    }
                    sc->join_buf.len = 0;
                    SgStr tr = sg_md_trim_line(mrest.ptr, mrest.len);
                    sg_buf_write_s(&sc->join_buf, tr.ptr, tr.len);
                    have_item = true;
                    pos = le2 + 1;
                } else if (have_item && sg_md_is_continued(l2, ll2)) {
                    SgStr tr = sg_md_trim_line(l2, ll2);
                    sg_buf_putc_s(&sc->join_buf, '\n');
                    sg_buf_write_s(&sc->join_buf, tr.ptr, tr.len);
                    pos = le2 + 1;
                } else {
                    break;
                }
            }
            if (have_item) {
                sg_buf_write_s(&sc->html_buf, "<li>", 4);
                sg_md_inline((const char *)sc->join_buf.data, sc->join_buf.len, env, &sc->html_buf);
                sg_buf_write_s(&sc->html_buf, "</li>\n", 6);
            }
            sg_buf_write_s(&sc->html_buf, "</", 2);
            sg_buf_write_s(&sc->html_buf, tag, 2);
            sg_buf_write_s(&sc->html_buf, ">\n", 2);
            continue;
        }
        sc->join_buf.len = 0;
        SgStr p0 = sg_md_trim_line(line, llen);
        sg_buf_write_s(&sc->join_buf, p0.ptr, p0.len);
        pos = le + 1;
        while (pos < len) {
            size_t ps = pos, pe = ps;
            while (pe < len && src[pe] != '\n') pe++;
            const char *pl = src + ps;
            size_t pll = pe - ps;
            SgStr dlang, dht; uint32_t dd;
            if (sg_md_is_blank(pl, pll) || sg_md_is_fence_open(pl, pll, &dlang) ||
                sg_md_heading_of(pl, pll, &dd, &dht) || sg_md_is_hr(pl, pll)) break;
            SgStr tr = sg_md_trim_line(pl, pll);
            sg_buf_putc_s(&sc->join_buf, '\n');
            sg_buf_write_s(&sc->join_buf, tr.ptr, tr.len);
            pos = pe + 1;
        }
        sg_buf_write_s(&sc->html_buf, "<p>", 3);
        size_t p_start = sc->html_buf.len;
        sg_md_inline((const char *)sc->join_buf.data, sc->join_buf.len, env, &sc->html_buf);
        size_t p_len = sc->html_buf.len - p_start;
        if (!has_excerpt) {
            has_excerpt = true;
            res->excerpt.ptr = sg_arena_strdup(a, (const char *)(sc->html_buf.data + p_start), p_len);
            res->excerpt.len = (uint32_t)p_len;
        }
        sg_buf_write_s(&sc->html_buf, "</p>\n", 5);
    }

    res->html.ptr = (const char *)sc->html_buf.data;
    res->html.len = (uint32_t)sc->html_buf.len;
    if (toc_count > 0) {
        res->toc = (SgTocItem *)sg_arena_alloc(a, toc_count * sizeof(SgTocItem), 8);
        sg_memcpy(res->toc, sc->toc, toc_count * sizeof(SgTocItem));
    } else {
        res->toc = NULL;
    }
    res->toc_count = toc_count;
}
