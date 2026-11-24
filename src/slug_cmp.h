#pragma once
#include "arena.h"
#include "nfkd_table.h"

/* Exact JavaScript UTF-16 code-unit lexicographic comparison for UTF-8 strings.
   UTF-8 byte order matches UTF-16 code unit order for ALL BMP chars (U+0000..U+FFFF)
   AND for supplementary chars vs U+0000..U+D7FF. Only supplementary chars (>= U+10000,
   UTF-8 lead >= 0xF0, UTF-16 surrogate 0xD800..0xDFFF) vs private-use/high BMP
   (U+E000..U+FFFF, UTF-8 lead 0xEE..0xEF) differ! */
SG_INLINE uint32_t sg_decode_utf8_at(const uint8_t *s, size_t len, size_t *pos) {
    uint8_t b0 = s[*pos];
    if (b0 < 0x80) { (*pos)++; return b0; }
    if ((b0 & 0xE0) == 0xC0 && *pos + 1 < len) {
        uint32_t cp = ((uint32_t)(b0 & 0x1F) << 6) | (uint32_t)(s[*pos + 1] & 0x3F);
        *pos += 2;
        return cp;
    }
    if ((b0 & 0xF0) == 0xE0 && *pos + 2 < len) {
        uint32_t cp = ((uint32_t)(b0 & 0x0F) << 12) | ((uint32_t)(s[*pos + 1] & 0x3F) << 6) | (uint32_t)(s[*pos + 2] & 0x3F);
        *pos += 3;
        return cp;
    }
    if ((b0 & 0xF8) == 0xF0 && *pos + 3 < len) {
        uint32_t cp = ((uint32_t)(b0 & 0x07) << 18) | ((uint32_t)(s[*pos + 1] & 0x3F) << 12) |
                      ((uint32_t)(s[*pos + 2] & 0x3F) << 6) | (uint32_t)(s[*pos + 3] & 0x3F);
        *pos += 4;
        return cp;
    }
    (*pos)++;
    return 0xFFFD;
}

static inline int sg_by_code_units(const char *a, size_t alen, const char *b, size_t blen) {
    const uint8_t *pa = (const uint8_t *)a;
    const uint8_t *pb = (const uint8_t *)b;
    size_t minlen = alen < blen ? alen : blen;
    size_t i = 0;
    while (i + 8 <= minlen) {
        uint64_t va, vb;
        __builtin_memcpy(&va, pa + i, 8);
        __builtin_memcpy(&vb, pb + i, 8);
        if (va != vb) {
            uint64_t diff = va ^ vb;
            i += (size_t)(__builtin_ctzll(diff) >> 3);
            goto diverge;
        }
        i += 8;
    }
    while (i < minlen) {
        if (pa[i] != pb[i]) goto diverge;
        i++;
    }
    return alen < blen ? -1 : (alen > blen ? 1 : 0);

diverge:
    /* Fast path: if mismatching bytes (and preceding byte if continuation) are both < 0xEE */
    if (SG_LIKELY(pa[i] < 0x80 && pb[i] < 0x80)) {
        return pa[i] < pb[i] ? -1 : 1;
    }
    /* Step back to start of UTF-8 codepoint */
    size_t sa = i, sb = i;
    while (sa > 0 && (pa[sa] & 0xC0) == 0x80) sa--;
    while (sb > 0 && (pb[sb] & 0xC0) == 0x80) sb--;
    if (sa == sb && pa[sa] < 0xEE && pb[sb] < 0xEE) {
        return pa[i] < pb[i] ? -1 : 1;
    }
    /* Full exact UTF-16 code unit step from min(sa, sb) (if both identical before sa, start at min) */
    size_t ia = sa < sb ? sa : sb;
    size_t ib = ia;
    while (ia < alen && ib < blen) {
        uint32_t ca = sg_decode_utf8_at(pa, alen, &ia);
        uint32_t cb = sg_decode_utf8_at(pb, blen, &ib);
        if (ca == cb) continue;
        uint16_t u16a = (ca <= 0xFFFF) ? (uint16_t)ca : (uint16_t)(0xD800u + ((ca - 0x10000u) >> 10));
        uint16_t u16b = (cb <= 0xFFFF) ? (uint16_t)cb : (uint16_t)(0xD800u + ((cb - 0x10000u) >> 10));
        return u16a < u16b ? -1 : 1;
    }
    return ia < alen ? 1 : (ib < blen ? -1 : 0);
}

/* Exact makefun-site slugify() (SLUG_MAX = 64) */
SG_INLINE bool sg_is_skip_cp(uint32_t cp) {
    size_t lo = 0, hi = SG_SLUG_SKIP_COUNT;
    while (lo < hi) {
        size_t mid = (lo + hi) >> 1;
        if (cp < SG_SLUG_SKIP[mid].start) hi = mid;
        else if (cp > SG_SLUG_SKIP[mid].end) lo = mid + 1;
        else return true;
    }
    return false;
}

SG_INLINE uint16_t sg_lookup_map_cp(uint32_t cp) {
    size_t lo = 0, hi = SG_SLUG_MAP_COUNT;
    while (lo < hi) {
        size_t mid = (lo + hi) >> 1;
        uint32_t mc = SG_SLUG_MAP_CP[mid];
        if (cp < mc) hi = mid;
        else if (cp > mc) lo = mid + 1;
        else return SG_SLUG_MAP_VAL[mid];
    }
    return 0xFFFE; /* SEP */
}

static size_t sg_slugify(const char *text, size_t len, char *out65) {
    const uint8_t *p = (const uint8_t *)text;
    size_t i = 0;
    size_t olen = 0;
    bool gap = false;

    while (i < len) {
        uint8_t b = p[i];
        if (SG_LIKELY(b < 0x80)) {
            i++;
            if ((uint8_t)(b - 'a') <= 25u || (uint8_t)(b - '0') <= 9u) {
                if (gap) {
                    if (olen >= 63) break;
                    out65[olen++] = '-';
                }
                gap = false;
                out65[olen++] = (char)b;
                if (olen == 64) break;
            } else if ((uint8_t)(b - 'A') <= 25u) {
                if (gap) {
                    if (olen >= 63) break;
                    out65[olen++] = '-';
                }
                gap = false;
                out65[olen++] = (char)(b + 32);
                if (olen == 64) break;
            } else {
                if (olen > 0) gap = true;
            }
            continue;
        }
        uint32_t cp = sg_decode_utf8_at(p, len, &i);
        uint16_t val;
        if (cp < 0x100) {
            val = SG_SLUG_LATIN1[cp - 0x80];
        } else {
            if (sg_is_skip_cp(cp)) continue;
            val = sg_lookup_map_cp(cp);
        }
        if (val == 0xFFFF) continue;
        if (val == 0xFFFE) {
            if (olen > 0) gap = true;
            continue;
        }
        uint32_t plen = val >> 12;
        uint32_t poff = val & 0x0FFFu;
        for (uint32_t k = 0; k < plen; k++) {
            char pc = SG_SLUG_POOL[poff + k];
            if (pc == '-') {
                if (olen > 0) gap = true;
            } else {
                if (gap) {
                    if (olen >= 63) goto done;
                    out65[olen++] = '-';
                }
                gap = false;
                out65[olen++] = pc;
                if (olen == 64) goto done;
            }
        }
    }
done:
    if (olen == 0) {
        sg_memcpy(out65, "untitled", 8);
        out65[8] = '\0';
        return 8;
    }
    out65[olen] = '\0';
    return olen;
}

/* Open-addressed string hash set / map with FNV-1a */
SG_INLINE uint64_t sg_fnv1a(const char *s, size_t len) {
    uint64_t h = 14695981039346656037ULL;
    for (size_t i = 0; i < len; i++) {
        h ^= (uint8_t)s[i];
        h *= 1099511628211ULL;
    }
    return h;
}
