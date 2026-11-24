#pragma once
#include "arena.h"

typedef struct {
    bool has_error;
    char error[256];
    uint32_t error_len;

    SgStr title;
    SgStr date;
    bool has_date;
    bool draft;
    bool has_draft;
    SgStr layout;
    bool has_layout;
    SgStr slug;
    bool has_slug;
    SgStr summary;
    bool has_summary;
    SgStr *tags;
    uint32_t tag_count;
    bool has_tags;

    SgStr body;
} SgFrontMatter;

SG_INLINE bool sg_is_hws(char c) { return c == ' ' || c == '\t'; }

static void sg_fm_err(SgFrontMatter *fm, const char *s1, const char *s2, size_t l2, const char *s3) {
    fm->has_error = true;
    size_t l1 = sg_strlen(s1);
    size_t l3 = s3 ? sg_strlen(s3) : 0;
    size_t p = 0;
    if (l1 > 250) l1 = 250;
    sg_memcpy(fm->error + p, s1, l1); p += l1;
    if (l2 > 250 - p) l2 = 250 - p;
    if (l2) { sg_memcpy(fm->error + p, s2, l2); p += l2; }
    if (l3 > 250 - p) l3 = 250 - p;
    if (l3) { sg_memcpy(fm->error + p, s3, l3); p += l3; }
    fm->error[p] = '\0';
    fm->error_len = (uint32_t)p;
}

static void sg_fm_err_u32(SgFrontMatter *fm, uint32_t line_no) {
    char num[16];
    int idx = 16;
    uint32_t v = line_no;
    do { num[--idx] = (char)('0' + (v % 10u)); v /= 10u; } while (v);
    fm->has_error = true;
    size_t p = 0;
    sg_memcpy(fm->error, "line ", 5); p += 5;
    size_t nl = (size_t)(16 - idx);
    sg_memcpy(fm->error + p, num + idx, nl); p += nl;
    const char *suf = " is not \"key: value\"";
    size_t sl = sg_strlen(suf);
    sg_memcpy(fm->error + p, suf, sl); p += sl;
    fm->error[p] = '\0';
    fm->error_len = (uint32_t)p;
}

SG_INLINE int sg_days_in(int y, int m) {
    if (m == 2) return ((y % 4 == 0 && y % 100 != 0) || y % 400 == 0) ? 29 : 28;
    if (m == 4 || m == 6 || m == 9 || m == 11) return 30;
    return 31;
}

static bool sg_fm_string_of(SgArena *a, const char *raw, size_t len, const char *key, size_t klen, SgFrontMatter *fm, SgStr *out) {
    if (raw[0] == '"') {
        if (len < 2 || raw[len - 1] != '"') {
            fm->has_error = true;
            sg_fm_err(fm, "key \"", key, klen, "\": a quoted string is not closed");
            return false;
        }
        /* Fast check for backslash or stray quote inside */
        char *buf = (char *)sg_arena_alloc(a, len, 1);
        size_t o = 0;
        for (size_t i = 1; i < len - 1; i++) {
            char c = raw[i];
            if (c == '\\') {
                if (i + 1 >= len - 1) {
                    sg_fm_err(fm, "key \"", key, klen, "\": a quoted string holds an escape other than \\\" or \\\\");
                    return false;
                }
                char n = raw[i + 1];
                if (n != '"' && n != '\\') {
                    sg_fm_err(fm, "key \"", key, klen, "\": a quoted string holds an escape other than \\\" or \\\\");
                    return false;
                }
                buf[o++] = n;
                i++;
            } else if (c == '"') {
                sg_fm_err(fm, "key \"", key, klen, "\": a quoted string holds a stray quote");
                return false;
            } else {
                buf[o++] = c;
            }
        }
        out->ptr = buf;
        out->len = (uint32_t)o;
        return true;
    }
    if (raw[0] == '[') {
        sg_fm_err(fm, "key \"", key, klen, "\": a bare string opens on [");
        return false;
    }
    out->ptr = raw;
    out->len = (uint32_t)len;
    return true;
}

/* Frontmatter keys */
enum {
    FM_KEY_UNKNOWN = -1,
    FM_KEY_TITLE = 0,
    FM_KEY_DATE = 1,
    FM_KEY_TAGS = 2,
    FM_KEY_DRAFT = 3,
    FM_KEY_LAYOUT = 4,
    FM_KEY_SLUG = 5,
    FM_KEY_SUMMARY = 6
};

SG_INLINE int sg_fm_classify_key(const char *k, size_t len) {
    if (len == 4) {
        if (sg_memcmp(k, "date", 4) == 0) return FM_KEY_DATE;
        if (sg_memcmp(k, "tags", 4) == 0) return FM_KEY_TAGS;
        if (sg_memcmp(k, "slug", 4) == 0) return FM_KEY_SLUG;
    } else if (len == 5) {
        if (sg_memcmp(k, "title", 5) == 0) return FM_KEY_TITLE;
        if (sg_memcmp(k, "draft", 5) == 0) return FM_KEY_DRAFT;
    } else if (len == 6) {
        if (sg_memcmp(k, "layout", 6) == 0) return FM_KEY_LAYOUT;
    } else if (len == 7) {
        if (sg_memcmp(k, "summary", 7) == 0) return FM_KEY_SUMMARY;
    }
    return FM_KEY_UNKNOWN;
}

/* Check frontmatter WITHOUT needing body; text already normalized (\n only) */
static void sg_parse_frontmatter(SgArena *a, const char *text, size_t len, SgFrontMatter *fm) {
    sg_memset(fm, 0, sizeof(*fm));

    /* Find first newline */
    const char *nl0 = NULL;
    for (size_t i = 0; i < len; i++) {
        if (text[i] == '\n') { nl0 = text + i; break; }
    }
    size_t l0 = nl0 ? (size_t)(nl0 - text) : len;
    if (l0 != 3 || text[0] != '-' || text[1] != '-' || text[2] != '-') {
        sg_fm_err(fm, "no front matter", "", 0, NULL);
        return;
    }
    if (!nl0) {
        sg_fm_err(fm, "the front matter is not closed", "", 0, NULL);
        return;
    }

    size_t pos = l0 + 1;
    uint32_t line_no = 2; /* 1-based line index of current line (lines[1] = line 2) */
    uint8_t seen_mask = 0;

    while (pos <= len) {
        size_t lstart = pos;
        size_t lend = lstart;
        while (lend < len && text[lend] != '\n') lend++;
        size_t llen = lend - lstart;
        const char *line = text + lstart;

        if (llen == 3 && line[0] == '-' && line[1] == '-' && line[2] == '-') {
            if (!((seen_mask >> FM_KEY_TITLE) & 1)) {
                sg_fm_err(fm, "no title", "", 0, NULL);
                return;
            }
            if (lend < len) {
                fm->body.ptr = text + lend + 1;
                fm->body.len = (uint32_t)(len - (lend + 1));
            } else {
                fm->body.ptr = text + len;
                fm->body.len = 0;
            }
            return;
        }

        /* Check blank /^[ \t]*$/ or comment startsWith('#') */
        if (llen > 0 && line[0] != '#') {
            bool all_ws = true;
            for (size_t i = 0; i < llen; i++) {
                if (!sg_is_hws(line[i])) { all_ws = false; break; }
            }
            if (!all_ws) {
                /* Match /^([a-z_]+):(?: (.*))?$/ */
                size_t klen = 0;
                while (klen < llen && ((uint8_t)(line[klen] - 'a') <= 25u || line[klen] == '_')) klen++;
                if (klen == 0 || klen >= llen || line[klen] != ':') {
                    sg_fm_err_u32(fm, line_no);
                    return;
                }
                const char *raw = "";
                size_t rlen = 0;
                if (klen + 1 < llen) {
                    if (line[klen + 1] != ' ') {
                        sg_fm_err_u32(fm, line_no);
                        return;
                    }
                    raw = line + klen + 2;
                    rlen = llen - (klen + 2);
                    while (rlen > 0 && sg_is_hws(raw[rlen - 1])) rlen--;
                }

                int kid = sg_fm_classify_key(line, klen);
                if (kid == FM_KEY_UNKNOWN) {
                    sg_fm_err(fm, "unknown key \"", line, klen, "\"");
                    return;
                }
                if ((seen_mask >> kid) & 1) {
                    sg_fm_err(fm, "key \"", line, klen, "\" is given twice");
                    return;
                }
                if (rlen == 0) {
                    sg_fm_err(fm, "key \"", line, klen, "\" has no value");
                    return;
                }
                seen_mask |= (uint8_t)(1u << kid);

                switch (kid) {
                    case FM_KEY_TITLE:
                        if (!sg_fm_string_of(a, raw, rlen, "title", 5, fm, &fm->title)) return;
                        break;
                    case FM_KEY_SUMMARY:
                        if (!sg_fm_string_of(a, raw, rlen, "summary", 7, fm, &fm->summary)) return;
                        fm->has_summary = true;
                        break;
                    case FM_KEY_DATE: {
                        bool ok = (rlen == 10 && raw[4] == '-' && raw[7] == '-');
                        int y = 0, mo = 0, d = 0;
                        if (ok) {
                            for (int k = 0; k < 10; k++) {
                                if (k == 4 || k == 7) continue;
                                if ((uint8_t)(raw[k] - '0') > 9u) { ok = false; break; }
                            }
                        }
                        if (ok) {
                            y = (raw[0]-'0')*1000 + (raw[1]-'0')*100 + (raw[2]-'0')*10 + (raw[3]-'0');
                            mo = (raw[5]-'0')*10 + (raw[6]-'0');
                            d = (raw[8]-'0')*10 + (raw[9]-'0');
                            if (y < 1970 || y > 2099 || mo < 1 || mo > 12 || d < 1 || d > sg_days_in(y, mo)) ok = false;
                        }
                        if (!ok) {
                            sg_fm_err(fm, "key \"date\": not a date from 1970-01-01 to 2099-12-31 (YYYY-MM-DD)", "", 0, NULL);
                            return;
                        }
                        fm->date.ptr = raw;
                        fm->date.len = 10;
                        fm->has_date = true;
                        break;
                    }
                    case FM_KEY_TAGS: {
                        if (rlen < 2 || raw[0] != '[' || raw[rlen - 1] != ']') {
                            sg_fm_err(fm, "key \"tags\": not a list [a, b]", "", 0, NULL);
                            return;
                        }
                        const char *in = raw + 1;
                        size_t in_len = rlen - 2;
                        size_t ts = 0, te = in_len;
                        while (ts < te && (in[ts] == ' ' || in[ts] == '\t' || in[ts] == '\v' || in[ts] == '\f')) ts++;
                        while (te > ts && (in[te - 1] == ' ' || in[te - 1] == '\t' || in[te - 1] == '\v' || in[te - 1] == '\f')) te--;
                        fm->has_tags = true;
                        if (ts == te) {
                            fm->tags = NULL;
                            fm->tag_count = 0;
                            break;
                        }
                        uint32_t commas = 0;
                        for (size_t i = 0; i < in_len; i++) if (in[i] == ',') commas++;
                        SgStr *tarr = (SgStr *)sg_arena_alloc(a, (size_t)(commas + 1) * sizeof(SgStr), 8);
                        uint32_t tc = 0;
                        size_t seg_s = 0;
                        for (size_t i = 0; i <= in_len; i++) {
                            if (i == in_len || in[i] == ',') {
                                size_t a0 = seg_s, b0 = i;
                                while (a0 < b0 && (in[a0] == ' ' || in[a0] == '\t' || in[a0] == '\v' || in[a0] == '\f')) a0++;
                                while (b0 > a0 && (in[b0 - 1] == ' ' || in[b0 - 1] == '\t' || in[b0 - 1] == '\v' || in[b0 - 1] == '\f')) b0--;
                                if (a0 == b0) {
                                    sg_fm_err(fm, "key \"tags\": an empty tag, or one holding \" [ or ]", "", 0, NULL);
                                    return;
                                }
                                for (size_t j = a0; j < b0; j++) {
                                    char ch = in[j];
                                    if (ch == '"' || ch == '[' || ch == ']') {
                                        sg_fm_err(fm, "key \"tags\": an empty tag, or one holding \" [ or ]", "", 0, NULL);
                                        return;
                                    }
                                }
                                tarr[tc].ptr = in + a0;
                                tarr[tc].len = (uint32_t)(b0 - a0);
                                tc++;
                                seg_s = i + 1;
                            }
                        }
                        fm->tags = tarr;
                        fm->tag_count = tc;
                        break;
                    }
                    case FM_KEY_DRAFT:
                        if (rlen == 4 && sg_memcmp(raw, "true", 4) == 0) {
                            fm->draft = true;
                            fm->has_draft = true;
                        } else if (rlen == 5 && sg_memcmp(raw, "false", 5) == 0) {
                            fm->draft = false;
                            fm->has_draft = true;
                        } else {
                            sg_fm_err(fm, "key \"draft\": true or false", "", 0, NULL);
                            return;
                        }
                        break;
                    case FM_KEY_LAYOUT: {
                        bool ok = (rlen > 0);
                        for (size_t i = 0; i < rlen; i++) {
                            char c = raw[i];
                            if (!((uint8_t)(c - 'a') <= 25u || (uint8_t)(c - '0') <= 9u || c == '-')) { ok = false; break; }
                        }
                        if (!ok) {
                            sg_fm_err(fm, "key \"layout\": not a template name", "", 0, NULL);
                            return;
                        }
                        fm->layout.ptr = raw;
                        fm->layout.len = (uint32_t)rlen;
                        fm->has_layout = true;
                        break;
                    }
                    case FM_KEY_SLUG: {
                        bool ok = (rlen >= 1 && rlen <= 64);
                        if (ok) {
                            bool prev_dash = true; /* forbid leading dash */
                            for (size_t i = 0; i < rlen; i++) {
                                char c = raw[i];
                                if ((uint8_t)(c - 'a') <= 25u || (uint8_t)(c - '0') <= 9u) {
                                    prev_dash = false;
                                } else if (c == '-' && !prev_dash) {
                                    prev_dash = true;
                                } else {
                                    ok = false; break;
                                }
                            }
                            if (prev_dash) ok = false;
                        }
                        if (!ok) {
                            sg_fm_err(fm, "key \"slug\": not a slug", "", 0, NULL);
                            return;
                        }
                        fm->slug.ptr = raw;
                        fm->slug.len = (uint32_t)rlen;
                        fm->has_slug = true;
                        break;
                    }
                }
            }
        }

        if (lend >= len) break;
        pos = lend + 1;
        line_no++;
    }

    sg_fm_err(fm, "the front matter is not closed", "", 0, NULL);
}
