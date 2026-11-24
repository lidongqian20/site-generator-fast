#pragma once
#include "markdown.h"

/* Exact makefun-site lib/template.js compiler, include-inliner, site-cache, and JSON-streaming renderer */

#define SG_INCLUDE_DEPTH 8

typedef enum {
    ROOT_UNKNOWN = 0,
    ROOT_SITE,
    ROOT_TAGS,
    ROOT_RECENT,
    ROOT_PAGE,
    ROOT_POSTS,
    ROOT_PAGER,
    ROOT_TAG
} SgRootVar;

typedef enum {
    FLD_UNKNOWN = 0,
    /* site */
    FLD_TITLE,
    FLD_BASE_URL,
    /* tag / allTags / card tag ref */
    FLD_NAME,
    FLD_SLUG,
    FLD_URL,
    FLD_COUNT,
    FLD_POSTS,
    /* card / page */
    FLD_DATE,
    FLD_DATE_LONG,
    FLD_SUMMARY,
    FLD_EXCERPT,
    FLD_READING_MINUTES,
    FLD_TAGS,
    FLD_CONTENT,
    FLD_TOC,
    FLD_NEWER,
    FLD_OLDER,
    FLD_IS_POST,
    /* toc */
    FLD_LEVEL,
    FLD_ID,
    FLD_HTML,
    /* pager */
    FLD_NUMBER,
    FLD_TOTAL,
    FLD_NEWER_URL,
    FLD_OLDER_URL,
    /* loop */
    FLD_INDEX,
    FLD_FIRST,
    FLD_LAST,
    FLD_LENGTH
} SgFieldId;

typedef struct {
    SgStr raw_first;
    uint8_t root_hint; /* SgRootVar if matches built-in */
    uint8_t plen;      /* number of segments */
    SgFieldId fields[8]; /* path[1..] resolved to field IDs; overflow > 8 always returns UNDEF */
    SgStr extra_segs[8]; /* for rare path > 1 checking */
} SgTplPath;

typedef enum {
    TPL_NODE_TEXT = 0,     /* plain text + precomputed JSON-escaped text! */
    TPL_NODE_VAR,
    TPL_NODE_RAW,
    TPL_NODE_IF,
    TPL_NODE_FOR,
    TPL_NODE_INCLUDE       /* only kept if include depth/size limit falls back */
} SgTplNodeKind;

typedef struct SgTplNode {
    uint8_t kind;
    uint8_t if_not;
    bool is_pure_site;     /* true if depends ONLY on (site, tags, recent) + local loops */
    SgTplPath path;        /* for VAR, RAW, IF, FOR */
    SgStr text;            /* raw text for TEXT, or target name for INCLUDE */
    SgStr json_text;       /* JSON-escaped text for TEXT */
    SgStr loop_var;        /* for FOR: local variable name */
    uint32_t body_count;
    uint32_t else_count;
    struct SgTplNode *body;
    struct SgTplNode *else_body;
    /* Optional fast card cache key for FOR p IN posts / tag.posts */
    int32_t card_cache_slot; /* -1 if not cacheable by card */
} SgTplNode;

typedef struct {
    SgStr name;
    bool valid;
    SgTplNode *nodes;
    uint32_t node_count;
    bool uses_page_content_escaped; /* true if {{ page.content }} appears anywhere */
    int32_t card_slot_count;
} SgParsedTemplate;

/* ---- Lexer / Parser ---- */

SG_INLINE bool sg_is_ident_start(char c) {
    return (uint8_t)(c - 'A') <= 25u || (uint8_t)(c - 'a') <= 25u || c == '_';
}
SG_INLINE bool sg_is_ident_cont(char c) {
    return sg_is_ident_start(c) || (uint8_t)(c - '0') <= 9u;
}
SG_INLINE bool sg_is_tpl_name_char(char c) {
    return (uint8_t)(c - 'a') <= 25u || (uint8_t)(c - '0') <= 9u || c == '-';
}

static SgFieldId sg_classify_field(const char *s, size_t len) {
    switch (len) {
        case 2:
            if (s[0] == 'i' && s[1] == 'd') return FLD_ID;
            break;
        case 3:
            if (sg_memcmp(s, "url", 3) == 0) return FLD_URL;
            if (sg_memcmp(s, "toc", 3) == 0) return FLD_TOC;
            break;
        case 4:
            if (sg_memcmp(s, "name", 4) == 0) return FLD_NAME;
            if (sg_memcmp(s, "slug", 4) == 0) return FLD_SLUG;
            if (sg_memcmp(s, "date", 4) == 0) return FLD_DATE;
            if (sg_memcmp(s, "tags", 4) == 0) return FLD_TAGS;
            if (sg_memcmp(s, "html", 4) == 0) return FLD_HTML;
            if (sg_memcmp(s, "last", 4) == 0) return FLD_LAST;
            break;
        case 5:
            if (sg_memcmp(s, "title", 5) == 0) return FLD_TITLE;
            if (sg_memcmp(s, "count", 5) == 0) return FLD_COUNT;
            if (sg_memcmp(s, "posts", 5) == 0) return FLD_POSTS;
            if (sg_memcmp(s, "newer", 5) == 0) return FLD_NEWER;
            if (sg_memcmp(s, "older", 5) == 0) return FLD_OLDER;
            if (sg_memcmp(s, "level", 5) == 0) return FLD_LEVEL;
            if (sg_memcmp(s, "total", 5) == 0) return FLD_TOTAL;
            if (sg_memcmp(s, "index", 5) == 0) return FLD_INDEX;
            if (sg_memcmp(s, "first", 5) == 0) return FLD_FIRST;
            break;
        case 6:
            if (sg_memcmp(s, "number", 6) == 0) return FLD_NUMBER;
            if (sg_memcmp(s, "length", 6) == 0) return FLD_LENGTH;
            break;
        case 7:
            if (sg_memcmp(s, "summary", 7) == 0) return FLD_SUMMARY;
            if (sg_memcmp(s, "excerpt", 7) == 0) return FLD_EXCERPT;
            if (sg_memcmp(s, "content", 7) == 0) return FLD_CONTENT;
            if (sg_memcmp(s, "is_post", 7) == 0) return FLD_IS_POST;
            break;
        case 8:
            if (sg_memcmp(s, "base_url", 8) == 0) return FLD_BASE_URL;
            break;
        case 9:
            if (sg_memcmp(s, "date_long", 9) == 0) return FLD_DATE_LONG;
            if (sg_memcmp(s, "newer_url", 9) == 0) return FLD_NEWER_URL;
            if (sg_memcmp(s, "older_url", 9) == 0) return FLD_OLDER_URL;
            break;
        case 15:
            if (sg_memcmp(s, "reading_minutes", 15) == 0) return FLD_READING_MINUTES;
            break;
    }
    return FLD_UNKNOWN;
}

static uint8_t sg_classify_root(const char *s, size_t len) {
    if (len == 3 && sg_memcmp(s, "tag", 3) == 0) return ROOT_TAG;
    if (len == 4) {
        if (sg_memcmp(s, "site", 4) == 0) return ROOT_SITE;
        if (sg_memcmp(s, "tags", 4) == 0) return ROOT_TAGS;
        if (sg_memcmp(s, "page", 4) == 0) return ROOT_PAGE;
    }
    if (len == 5) {
        if (sg_memcmp(s, "posts", 5) == 0) return ROOT_POSTS;
        if (sg_memcmp(s, "pager", 5) == 0) return ROOT_PAGER;
    }
    if (len == 6 && sg_memcmp(s, "recent", 6) == 0) return ROOT_RECENT;
    return ROOT_UNKNOWN;
}

static void sg_tpl_format_err(SgArena *a, SgStr name, const char *msg, SgStr *err_out) {
    size_t ml = sg_strlen(msg);
    size_t tot = 12 + name.len + 3 + ml;
    char *b = (char *)sg_arena_alloc(a, tot + 1, 1);
    size_t p = 0;
    sg_memcpy(b + p, "template \"", 10); p += 10;
    sg_memcpy(b + p, name.ptr, name.len); p += name.len;
    sg_memcpy(b + p, "\": ", 3); p += 3;
    sg_memcpy(b + p, msg, ml); p += ml;
    b[p] = '\0';
    err_out->ptr = b;
    err_out->len = (uint32_t)p;
}

static bool sg_tpl_parse_path(SgArena *a, SgStr name, const char *txt, size_t len, SgTplPath *out, SgStr *err_out) {
    if (len == 0) goto bad;
    size_t i = 0;
    uint8_t segs = 0;
    while (i < len) {
        size_t s = i;
        if (!sg_is_ident_start(txt[i])) goto bad;
        i++;
        while (i < len && sg_is_ident_cont(txt[i])) i++;
        size_t slen = i - s;
        if (segs == 0) {
            out->raw_first.ptr = txt + s;
            out->raw_first.len = (uint32_t)slen;
            out->root_hint = sg_classify_root(txt + s, slen);
        } else if (segs - 1 < 8) {
            out->fields[segs - 1] = sg_classify_field(txt + s, slen);
            out->extra_segs[segs - 1] = (SgStr){ txt + s, (uint32_t)slen };
        }
        segs++;
        if (i < len) {
            if (txt[i] != '.' || i + 1 == len) goto bad;
            i++;
        }
    }
    out->plen = segs;
    return true;
bad: {
    SgBuf tmp;
    uint8_t stack[512];
    sg_buf_init_static(&tmp, stack, sizeof(stack));
    sg_buf_putc_s(&tmp, '"');
    for (size_t k = 0; k < len; k++) {
        char esc[6];
        uint8_t c = (uint8_t)txt[k];
        if (c < 0x20 || c == '"' || c == '\\') {
            size_t el = sg_json_esc_need_char(c, esc);
            sg_buf_write_s(&tmp, esc, el);
        } else {
            sg_buf_putc_s(&tmp, (char)c);
        }
    }
    sg_buf_write_s(&tmp, "\" is not a path", 15);
    sg_buf_putc_s(&tmp, '\0');
    sg_tpl_format_err(a, name, (const char *)tmp.data, err_out);
    if (tmp.owns_heap) sg_buf_free_raw(tmp.data, tmp.cap);
    return false;
}
}

static SgStr sg_trim_tpl_ws(const char *s, size_t len) {
    size_t a = 0, b = len;
    while (a < b && (s[a] == ' ' || s[a] == '\t' || s[a] == '\n' || s[a] == '\r' || s[a] == '\v' || s[a] == '\f')) a++;
    while (b > a && (s[b-1] == ' ' || s[b-1] == '\t' || s[b-1] == '\n' || s[b-1] == '\r' || s[b-1] == '\v' || s[b-1] == '\f')) b--;
    return (SgStr){ s + a, (uint32_t)(b - a) };
}

static SgStr sg_json_escape_to_arena(SgArena *a, const char *s, size_t len) {
    /* Fast scan first: if clean ASCII >= 0x20 without " or \, zero copy! */
    bool dirty = false;
    for (size_t i = 0; i < len; i++) {
        uint8_t c = (uint8_t)s[i];
        if (c < 0x20 || c == '"' || c == '\\') { dirty = true; break; }
    }
    if (!dirty) return (SgStr){ s, (uint32_t)len };
    char *dst = (char *)sg_arena_alloc(a, len * 6 + 1, 1);
    size_t d = 0;
    for (size_t i = 0; i < len; i++) {
        uint8_t c = (uint8_t)s[i];
        if (c < 0x20 || c == '"' || c == '\\') {
            d += sg_json_esc_need_char(c, dst + d);
        } else {
            dst[d++] = (char)c;
        }
    }
    dst[d] = '\0';
    return (SgStr){ dst, (uint32_t)d };
}

typedef struct {
    uint8_t kind; /* 0 = text, 1 = var, 2 = raw, 3 = tag */
    SgStr text;
    SgTplPath path;
} SgTplTok;

static bool sg_parse_template(SgArena *a, SgStr name, SgStr src_str, SgParsedTemplate *out, SgStr *err_out) {
    sg_memset(out, 0, sizeof(*out));
    out->name = name;

    const char *src = src_str.ptr;
    size_t len = src_str.len;

    uint32_t tok_cap = 128;
    uint32_t tok_cnt = 0;
    SgTplTok *toks = (SgTplTok *)sg_arena_alloc(a, tok_cap * sizeof(SgTplTok), 8);

    SgBuf text_buf;
    uint8_t text_init[2048];
    sg_buf_init_static(&text_buf, text_init, sizeof(text_init));

    #define TPL_PUSH_TOK(tk) do { \
        if (tok_cnt == tok_cap) { \
            uint32_t ncap = tok_cap * 2; \
            SgTplTok *nt = (SgTplTok *)sg_arena_alloc(a, ncap * sizeof(SgTplTok), 8); \
            sg_memcpy(nt, toks, tok_cap * sizeof(SgTplTok)); \
            toks = nt; tok_cap = ncap; \
        } \
        toks[tok_cnt++] = (tk); \
    } while (0)

    #define TPL_FLUSH_TEXT() do { \
        if (text_buf.len > 0) { \
            SgTplTok tt = { 0, { sg_arena_strdup(a, (const char *)text_buf.data, text_buf.len), (uint32_t)text_buf.len }, {0} }; \
            TPL_PUSH_TOK(tt); \
            text_buf.len = 0; \
        } \
    } while (0)

    size_t i = 0;
    while (i < len) {
        size_t open = i;
        while (open < len && src[open] != '{') open++;
        if (open >= len || open + 1 >= len) {
            if (len > i) {
                sg_buf_write_s(&text_buf, src + i, len - i);
            }
            break;
        }
        char next = src[open + 1];
        if (next != '{' && next != '%') {
            size_t add = open + 1 - i;
            sg_buf_write_s(&text_buf, src + i, add);
            i = open + 1;
            continue;
        }
        if (open > i) {
            size_t add = open - i;
            sg_buf_write_s(&text_buf, src + i, add);
        }
        if (next == '{') {
            bool is_raw = (open + 2 < len && src[open + 2] == '{');
            size_t cstart = open + (is_raw ? 3 : 2);
            const char *pat = is_raw ? "}}}" : "}}";
            size_t plen = is_raw ? 3 : 2;
            ptrdiff_t close = -1;
            for (size_t q = cstart; q + plen <= len; q++) {
                if (sg_memcmp(src + q, pat, plen) == 0) { close = (ptrdiff_t)q; break; }
            }
            if (close == -1) {
                sg_tpl_format_err(a, name, is_raw ? "a {{{ is not closed" : "a {{ is not closed", err_out);
                if (text_buf.owns_heap) sg_buf_free_raw(text_buf.data, text_buf.cap);
                return false;
            }
            TPL_FLUSH_TEXT();
            SgStr trimmed = sg_trim_tpl_ws(src + cstart, (size_t)close - cstart);
            SgTplTok tk = { is_raw ? 2 : 1, {0}, {0} };
            if (!sg_tpl_parse_path(a, name, trimmed.ptr, trimmed.len, &tk.path, err_out)) {
                if (text_buf.owns_heap) sg_buf_free_raw(text_buf.data, text_buf.cap);
                return false;
            }
            TPL_PUSH_TOK(tk);
            i = (size_t)close + plen;
            continue;
        }
        /* next == '%' */
        ptrdiff_t close = -1;
        for (size_t q = open + 2; q + 2 <= len; q++) {
            if (src[q] == '%' && src[q + 1] == '}') { close = (ptrdiff_t)q; break; }
        }
        if (close == -1) {
            sg_tpl_format_err(a, name, "a {% is not closed", err_out);
            if (text_buf.owns_heap) sg_buf_free_raw(text_buf.data, text_buf.cap);
            return false;
        }
        size_t end = (size_t)close + 2;
        size_t lineStart = 0;
        for (size_t q = open; q > 0; q--) { if (src[q - 1] == '\n') { lineStart = q; break; } }
        size_t nl = end;
        while (nl < len && src[nl] != '\n') nl++;
        bool standalone = true;
        for (size_t q = lineStart; q < open; q++) if (src[q] != ' ' && src[q] != '\t') { standalone = false; break; }
        if (standalone) {
            for (size_t q = end; q < nl; q++) if (src[q] != ' ' && src[q] != '\t') { standalone = false; break; }
        }
        if (standalone) text_buf.len -= (open - lineStart);
        TPL_FLUSH_TEXT();
        SgStr trimmed = sg_trim_tpl_ws(src + open + 2, (size_t)close - (open + 2));
        SgTplTok tk = { 3, trimmed, {0} };
        TPL_PUSH_TOK(tk);
        i = standalone ? (nl >= len ? len : nl + 1) : end;
    }
    TPL_FLUSH_TEXT();
    if (text_buf.owns_heap) sg_buf_free_raw(text_buf.data, text_buf.cap);

    /* Parse tokens into tree */
    typedef struct {
        uint8_t kind; /* 0 = root, 1 = for, 2 = if */
        SgTplNode *node;
        bool in_else;
        SgTplNode *items;
        uint32_t count;
        uint32_t cap;
    } ParseFrame;

    ParseFrame stack[64];
    int sp = 0;
    stack[0] = (ParseFrame){ 0, NULL, false, (SgTplNode *)sg_arena_alloc(a, 64 * sizeof(SgTplNode), 8), 0, 64 };

    #define FRAME_PUSH_NODE(fr, nd) do { \
        if ((fr)->count == (fr)->cap) { \
            uint32_t nc = (fr)->cap * 2; \
            SgTplNode *nn = (SgTplNode *)sg_arena_alloc(a, nc * sizeof(SgTplNode), 8); \
            sg_memcpy(nn, (fr)->items, (fr)->cap * sizeof(SgTplNode)); \
            (fr)->items = nn; (fr)->cap = nc; \
        } \
        (fr)->items[(fr)->count++] = (nd); \
    } while (0)

    for (uint32_t t = 0; t < tok_cnt; t++) {
        SgTplTok *tk = &toks[t];
        if (tk->kind == 0) {
            SgTplNode nd = {0};
            nd.kind = TPL_NODE_TEXT;
            nd.text = tk->text;
            nd.json_text = sg_json_escape_to_arena(a, tk->text.ptr, tk->text.len);
            FRAME_PUSH_NODE(&stack[sp], nd);
        } else if (tk->kind == 1 || tk->kind == 2) {
            SgTplNode nd = {0};
            nd.kind = (tk->kind == 1) ? TPL_NODE_VAR : TPL_NODE_RAW;
            nd.path = tk->path;
            if (tk->kind == 1 && tk->path.root_hint == ROOT_PAGE && tk->path.plen == 2 && tk->path.fields[0] == FLD_CONTENT) {
                out->uses_page_content_escaped = true;
            }
            FRAME_PUSH_NODE(&stack[sp], nd);
        } else {
            const char *tag = tk->text.ptr;
            size_t tlen = tk->text.len;
            if (tlen > 4 && sg_memcmp(tag, "for ", 4) == 0) {
                size_t q = 4;
                if (q >= tlen || !sg_is_ident_start(tag[q])) goto bad_tag;
                q++;
                while (q < tlen && sg_is_ident_cont(tag[q])) q++;
                if (q + 4 >= tlen || sg_memcmp(tag + q, " in ", 4) != 0) goto bad_tag;
                SgStr vname = { tag + 4, (uint32_t)(q - 4) };
                const char *pstr = tag + q + 4;
                size_t plen = tlen - (q + 4);
                /* Check no whitespace in pstr (\S+) */
                for (size_t k = 0; k < plen; k++) {
                    if (pstr[k] == ' ' || pstr[k] == '\t' || pstr[k] == '\n' || pstr[k] == '\r') goto bad_tag;
                }
                SgTplPath pth = {0};
                if (!sg_tpl_parse_path(a, name, pstr, plen, &pth, err_out)) return false;
                if (sp + 1 >= 64) { sg_tpl_format_err(a, name, "nesting too deep", err_out); return false; }
                SgTplNode nd = {0};
                nd.kind = TPL_NODE_FOR;
                nd.loop_var = vname;
                nd.path = pth;
                nd.card_cache_slot = -1;
                sp++;
                stack[sp] = (ParseFrame){ 1, NULL, false, (SgTplNode *)sg_arena_alloc(a, 32 * sizeof(SgTplNode), 8), 0, 32 };
                /* Save node header via copy on pop; store nd inside parent when popped? Wait: push to temp! */
                stack[sp].node = (SgTplNode *)sg_arena_alloc(a, sizeof(SgTplNode), 8);
                *stack[sp].node = nd;
            } else if (tlen > 3 && sg_memcmp(tag, "if ", 3) == 0) {
                size_t q = 3;
                bool is_not = false;
                if (tlen > 7 && sg_memcmp(tag + 3, "not ", 4) == 0) { is_not = true; q = 7; }
                const char *pstr = tag + q;
                size_t plen = tlen - q;
                if (plen == 0) goto bad_tag;
                for (size_t k = 0; k < plen; k++) {
                    if (pstr[k] == ' ' || pstr[k] == '\t' || pstr[k] == '\n' || pstr[k] == '\r') goto bad_tag;
                }
                SgTplPath pth = {0};
                if (!sg_tpl_parse_path(a, name, pstr, plen, &pth, err_out)) return false;
                if (sp + 1 >= 64) { sg_tpl_format_err(a, name, "nesting too deep", err_out); return false; }
                SgTplNode nd = {0};
                nd.kind = TPL_NODE_IF;
                nd.if_not = is_not ? 1 : 0;
                nd.path = pth;
                sp++;
                stack[sp] = (ParseFrame){ 2, (SgTplNode *)sg_arena_alloc(a, sizeof(SgTplNode), 8), false,
                                          (SgTplNode *)sg_arena_alloc(a, 32 * sizeof(SgTplNode), 8), 0, 32 };
                *stack[sp].node = nd;
            } else if (tlen == 4 && sg_memcmp(tag, "else", 4) == 0) {
                if (stack[sp].kind != 2 || stack[sp].in_else) {
                    sg_tpl_format_err(a, name, "an else outside an if", err_out);
                    return false;
                }
                stack[sp].node->body = stack[sp].items;
                stack[sp].node->body_count = stack[sp].count;
                stack[sp].in_else = true;
                stack[sp].items = (SgTplNode *)sg_arena_alloc(a, 32 * sizeof(SgTplNode), 8);
                stack[sp].count = 0;
                stack[sp].cap = 32;
            } else if (tlen == 6 && sg_memcmp(tag, "endfor", 6) == 0) {
                if (stack[sp].kind != 1) {
                    sg_tpl_format_err(a, name, "an endfor with no open for", err_out);
                    return false;
                }
                SgTplNode nd = *stack[sp].node;
                nd.body = stack[sp].items;
                nd.body_count = stack[sp].count;
                sp--;
                FRAME_PUSH_NODE(&stack[sp], nd);
            } else if (tlen == 5 && sg_memcmp(tag, "endif", 5) == 0) {
                if (stack[sp].kind != 2) {
                    sg_tpl_format_err(a, name, "an endif with no open if", err_out);
                    return false;
                }
                SgTplNode nd = *stack[sp].node;
                if (stack[sp].in_else) {
                    nd.else_body = stack[sp].items;
                    nd.else_count = stack[sp].count;
                } else {
                    nd.body = stack[sp].items;
                    nd.body_count = stack[sp].count;
                    nd.else_body = NULL;
                    nd.else_count = 0;
                }
                sp--;
                FRAME_PUSH_NODE(&stack[sp], nd);
            } else if (tlen >= 11 && sg_memcmp(tag, "include \"", 9) == 0 && tag[tlen - 1] == '"') {
                const char *inc = tag + 9;
                size_t ilen = tlen - 10;
                if (ilen == 0) goto bad_tag;
                for (size_t k = 0; k < ilen; k++) if (!sg_is_tpl_name_char(inc[k])) goto bad_tag;
                SgTplNode nd = {0};
                nd.kind = TPL_NODE_INCLUDE;
                nd.text = (SgStr){ inc, (uint32_t)ilen };
                FRAME_PUSH_NODE(&stack[sp], nd);
            } else {
            bad_tag: {
                SgBuf tmp;
                uint8_t sbuf[512];
                sg_buf_init_static(&tmp, sbuf, sizeof(sbuf));
                sg_buf_write_s(&tmp, "{% ", 3);
                sg_buf_write_s(&tmp, tag, tlen);
                sg_buf_write_s(&tmp, " %} is not a tag", 16);
                sg_buf_putc_s(&tmp, '\0');
                sg_tpl_format_err(a, name, (const char *)tmp.data, err_out);
                if (tmp.owns_heap) sg_buf_free_raw(tmp.data, tmp.cap);
                return false;
            }
            }
        }
    }

    if (sp > 0) {
        sg_tpl_format_err(a, name, stack[sp].kind == 1 ? "a for is not closed" : "a if is not closed", err_out);
        return false;
    }

    out->valid = true;
    out->nodes = stack[0].items;
    out->node_count = stack[0].count;
    return true;
}

/* ---- Runtime Data Model & Scope Evaluator ---- */

typedef struct {
    SgStr name;
    SgStr slug;
    SgStr url;
} SgTagRef;

typedef struct SgPostCard {
    SgStr title;
    SgStr url;
    SgStr date;
    SgStr date_long;
    SgStr summary;
    SgStr excerpt;
    uint32_t reading_minutes;
    SgTagRef *tags;
    uint32_t tag_count;
    /* Pre-rendered JSON-escaped card HTML cache (indexed by card_cache_slot, usually only slot 0) */
    SgStr cached_card_json[2];
    bool has_cached_card[2];
} SgPostCard;

typedef struct {
    SgStr name;
    SgStr slug;
    SgStr url;
    uint32_t count;
    SgPostCard **posts; /* pointers to cards of posts */
} SgTagObj;

typedef struct {
    SgStr title;
    SgStr url;
    SgStr date;
    SgStr date_long;
} SgNeighbour;

typedef struct {
    const SgPostCard *card;
    SgStr slug;
    SgStr content;       /* HTML (or pre-JSON-escaped if !uses_page_content_escaped) */
    bool content_is_json_esc;
    SgTocItem *toc;
    uint32_t toc_count;
    const SgNeighbour *newer;
    const SgNeighbour *older;
    bool is_post;
} SgPageObj;

typedef struct {
    uint32_t number;
    uint32_t total;
    SgStr url;
    SgStr newer_url;
    SgStr older_url;
} SgPagerObj;

typedef struct {
    uint32_t index;
    bool first;
    bool last;
    uint32_t length;
} SgLoopObj;

typedef enum {
    VK_UNDEF = 0,
    VK_NULL,
    VK_BOOL,
    VK_U32,
    VK_STR,
    VK_STR_JSON, /* pre-escaped JSON text (no HTML escaping needed, only used by {{{ page.content }}}) */
    VK_OBJ_SITE,
    VK_OBJ_ALL_TAG,
    VK_OBJ_FULL_TAG,
    VK_OBJ_TAG_REF,
    VK_OBJ_CARD,
    VK_OBJ_PAGE,
    VK_OBJ_NEIGHBOUR,
    VK_OBJ_TOC,
    VK_OBJ_PAGER,
    VK_OBJ_LOOP,
    VK_ARR_ALL_TAGS,
    VK_ARR_TAG_REFS,
    VK_ARR_CARDS,
    VK_ARR_TOC
} SgValKind;

typedef struct {
    uint8_t kind;
    union {
        bool b;
        uint32_t u;
        SgStr s;
        const void *p;
    } v;
    uint32_t count; /* for arrays */
} SgVal;

typedef struct {
    SgStr site_title;
    SgStr site_base;
    SgTagObj *all_tags;
    uint32_t all_tags_count;
    SgPostCard **recent;
    uint32_t recent_count;
    /* Layout-specific root bindings (NULL if absent from root scope!) */
    const SgPageObj *page;
    SgPostCard **posts;
    uint32_t posts_count;
    bool has_posts;
    const SgPagerObj *pager;
    const SgTagObj *tag;
    /* Active template registry & render state */
    SgParsedTemplate *tpls;
    uint32_t tpl_count;
    /* Active card loop cache recording */
    SgPostCard *card_cache_targets[8];
} SgRenderCtx;

typedef struct {
    SgStr var_name;
    SgVal var_val;
    SgLoopObj loop_val;
} SgLoopFrame;

SG_INLINE SgVal sg_prop_get(const SgRenderCtx *ctx, SgVal obj, SgFieldId fid) {
    SgVal u = { VK_UNDEF, {0}, 0 };
    if (fid == FLD_UNKNOWN) return u;
    switch (obj.kind) {
        case VK_OBJ_SITE:
            if (fid == FLD_TITLE) return (SgVal){ VK_STR, .v.s = ctx->site_title, 0 };
            if (fid == FLD_BASE_URL) return (SgVal){ VK_STR, .v.s = ctx->site_base, 0 };
            return u;
        case VK_OBJ_ALL_TAG: {
            const SgTagObj *t = (const SgTagObj *)obj.v.p;
            if (fid == FLD_NAME)  return (SgVal){ VK_STR, .v.s = t->name, 0 };
            if (fid == FLD_SLUG)  return (SgVal){ VK_STR, .v.s = t->slug, 0 };
            if (fid == FLD_URL)   return (SgVal){ VK_STR, .v.s = t->url, 0 };
            if (fid == FLD_COUNT) return (SgVal){ VK_U32, .v.u = t->count, 0 };
            return u;
        }
        case VK_OBJ_FULL_TAG: {
            const SgTagObj *t = (const SgTagObj *)obj.v.p;
            if (fid == FLD_NAME)  return (SgVal){ VK_STR, .v.s = t->name, 0 };
            if (fid == FLD_SLUG)  return (SgVal){ VK_STR, .v.s = t->slug, 0 };
            if (fid == FLD_URL)   return (SgVal){ VK_STR, .v.s = t->url, 0 };
            if (fid == FLD_COUNT) return (SgVal){ VK_U32, .v.u = t->count, 0 };
            if (fid == FLD_POSTS) return (SgVal){ VK_ARR_CARDS, .v.p = t->posts, t->count };
            return u;
        }
        case VK_OBJ_TAG_REF: {
            const SgTagRef *t = (const SgTagRef *)obj.v.p;
            if (fid == FLD_NAME) return (SgVal){ VK_STR, .v.s = t->name, 0 };
            if (fid == FLD_SLUG) return (SgVal){ VK_STR, .v.s = t->slug, 0 };
            if (fid == FLD_URL)  return (SgVal){ VK_STR, .v.s = t->url, 0 };
            return u;
        }
        case VK_OBJ_CARD: {
            const SgPostCard *c = (const SgPostCard *)obj.v.p;
            switch (fid) {
                case FLD_TITLE:           return (SgVal){ VK_STR, .v.s = c->title, 0 };
                case FLD_URL:             return (SgVal){ VK_STR, .v.s = c->url, 0 };
                case FLD_DATE:            return (SgVal){ VK_STR, .v.s = c->date, 0 };
                case FLD_DATE_LONG:       return (SgVal){ VK_STR, .v.s = c->date_long, 0 };
                case FLD_SUMMARY:         return (SgVal){ VK_STR, .v.s = c->summary, 0 };
                case FLD_EXCERPT:         return (SgVal){ VK_STR, .v.s = c->excerpt, 0 };
                case FLD_READING_MINUTES: return (SgVal){ VK_U32, .v.u = c->reading_minutes, 0 };
                case FLD_TAGS:            return (SgVal){ VK_ARR_TAG_REFS, .v.p = c->tags, c->tag_count };
                default: return u;
            }
        }
        case VK_OBJ_PAGE: {
            const SgPageObj *pg = (const SgPageObj *)obj.v.p;
            const SgPostCard *c = pg->card;
            switch (fid) {
                case FLD_TITLE:           return (SgVal){ VK_STR, .v.s = c->title, 0 };
                case FLD_URL:             return (SgVal){ VK_STR, .v.s = c->url, 0 };
                case FLD_DATE:            return (SgVal){ VK_STR, .v.s = c->date, 0 };
                case FLD_DATE_LONG:       return (SgVal){ VK_STR, .v.s = c->date_long, 0 };
                case FLD_SUMMARY:         return (SgVal){ VK_STR, .v.s = c->summary, 0 };
                case FLD_EXCERPT:         return (SgVal){ VK_STR, .v.s = c->excerpt, 0 };
                case FLD_READING_MINUTES: return (SgVal){ VK_U32, .v.u = c->reading_minutes, 0 };
                case FLD_TAGS:            return (SgVal){ VK_ARR_TAG_REFS, .v.p = c->tags, c->tag_count };
                case FLD_SLUG:            return (SgVal){ VK_STR, .v.s = pg->slug, 0 };
                case FLD_CONTENT:         return (SgVal){ pg->content_is_json_esc ? VK_STR_JSON : VK_STR, .v.s = pg->content, 0 };
                case FLD_TOC:             return (SgVal){ VK_ARR_TOC, .v.p = pg->toc, pg->toc_count };
                case FLD_NEWER:           return pg->newer ? (SgVal){ VK_OBJ_NEIGHBOUR, .v.p = pg->newer, 0 } : (SgVal){ VK_NULL, {0}, 0 };
                case FLD_OLDER:           return pg->older ? (SgVal){ VK_OBJ_NEIGHBOUR, .v.p = pg->older, 0 } : (SgVal){ VK_NULL, {0}, 0 };
                case FLD_IS_POST:         return (SgVal){ VK_BOOL, .v.b = pg->is_post, 0 };
                default: return u;
            }
        }
        case VK_OBJ_NEIGHBOUR: {
            const SgNeighbour *nb = (const SgNeighbour *)obj.v.p;
            if (fid == FLD_TITLE)     return (SgVal){ VK_STR, .v.s = nb->title, 0 };
            if (fid == FLD_URL)       return (SgVal){ VK_STR, .v.s = nb->url, 0 };
            if (fid == FLD_DATE)      return (SgVal){ VK_STR, .v.s = nb->date, 0 };
            if (fid == FLD_DATE_LONG) return (SgVal){ VK_STR, .v.s = nb->date_long, 0 };
            return u;
        }
        case VK_OBJ_TOC: {
            const SgTocItem *tc = (const SgTocItem *)obj.v.p;
            if (fid == FLD_LEVEL) return (SgVal){ VK_U32, .v.u = tc->level, 0 };
            if (fid == FLD_ID)    return (SgVal){ VK_STR, .v.s = tc->id, 0 };
            if (fid == FLD_HTML)  return (SgVal){ VK_STR, .v.s = tc->html, 0 };
            return u;
        }
        case VK_OBJ_PAGER: {
            const SgPagerObj *pr = (const SgPagerObj *)obj.v.p;
            if (fid == FLD_NUMBER)    return (SgVal){ VK_U32, .v.u = pr->number, 0 };
            if (fid == FLD_TOTAL)     return (SgVal){ VK_U32, .v.u = pr->total, 0 };
            if (fid == FLD_URL)       return (SgVal){ VK_STR, .v.s = pr->url, 0 };
            if (fid == FLD_NEWER_URL) return (SgVal){ VK_STR, .v.s = pr->newer_url, 0 };
            if (fid == FLD_OLDER_URL) return (SgVal){ VK_STR, .v.s = pr->older_url, 0 };
            return u;
        }
        case VK_OBJ_LOOP: {
            const SgLoopObj *lp = (const SgLoopObj *)obj.v.p;
            if (fid == FLD_INDEX)  return (SgVal){ VK_U32, .v.u = lp->index, 0 };
            if (fid == FLD_FIRST)  return (SgVal){ VK_BOOL, .v.b = lp->first, 0 };
            if (fid == FLD_LAST)   return (SgVal){ VK_BOOL, .v.b = lp->last, 0 };
            if (fid == FLD_LENGTH) return (SgVal){ VK_U32, .v.u = lp->length, 0 };
            return u;
        }
        case VK_ARR_ALL_TAGS:
        case VK_ARR_TAG_REFS:
        case VK_ARR_CARDS:
        case VK_ARR_TOC:
            if (fid == FLD_LENGTH) return (SgVal){ VK_U32, .v.u = obj.count, 0 };
            return u;
        default:
            return u;
    }
}

SG_INLINE SgVal sg_lookup_path(const SgRenderCtx *ctx, const SgLoopFrame *frames, int nf, const SgTplPath *p) {
    SgVal cur = { VK_UNDEF, {0}, 0 };
    const char *r = p->raw_first.ptr;
    uint32_t rl = p->raw_first.len;
    bool found = false;

    /* Walk loop scope stack top-down */
    for (int f = nf - 1; f >= 0; f--) {
        if (rl == 4 && sg_memcmp(r, "loop", 4) == 0) {
            cur = (SgVal){ VK_OBJ_LOOP, .v.p = &frames[f].loop_val, 0 };
            found = true;
            break;
        }
        if (frames[f].var_name.len == rl && sg_memcmp(frames[f].var_name.ptr, r, rl) == 0) {
            cur = frames[f].var_val;
            found = true;
            break;
        }
    }
    if (!found) {
        switch (p->root_hint) {
            case ROOT_SITE:   cur = (SgVal){ VK_OBJ_SITE, {0}, 0 }; break;
            case ROOT_TAGS:   cur = (SgVal){ VK_ARR_ALL_TAGS, .v.p = ctx->all_tags, ctx->all_tags_count }; break;
            case ROOT_RECENT: cur = (SgVal){ VK_ARR_CARDS, .v.p = ctx->recent, ctx->recent_count }; break;
            case ROOT_PAGE:   if (ctx->page) cur = (SgVal){ VK_OBJ_PAGE, .v.p = ctx->page, 0 }; break;
            case ROOT_POSTS:  if (ctx->has_posts) cur = (SgVal){ VK_ARR_CARDS, .v.p = ctx->posts, ctx->posts_count }; break;
            case ROOT_PAGER:  if (ctx->pager) cur = (SgVal){ VK_OBJ_PAGER, .v.p = ctx->pager, 0 }; break;
            case ROOT_TAG:    if (ctx->tag) cur = (SgVal){ VK_OBJ_FULL_TAG, .v.p = ctx->tag, 0 }; break;
            default: break;
        }
    }
    if (cur.kind == VK_UNDEF) return cur;
    for (uint8_t k = 1; k < p->plen; k++) {
        if (k - 1 >= 8) return (SgVal){ VK_UNDEF, {0}, 0 };
        cur = sg_prop_get(ctx, cur, p->fields[k - 1]);
        if (cur.kind == VK_UNDEF) return cur;
    }
    return cur;
}

SG_INLINE bool sg_val_truthy(SgVal val) {
    switch (val.kind) {
        case VK_UNDEF:
        case VK_NULL:
            return false;
        case VK_BOOL:
            return val.v.b;
        case VK_U32:
            return val.v.u != 0;
        case VK_STR:
        case VK_STR_JSON:
            return val.v.s.len > 0;
        case VK_ARR_ALL_TAGS:
        case VK_ARR_TAG_REFS:
        case VK_ARR_CARDS:
        case VK_ARR_TOC:
            return val.count > 0;
        default:
            return true; /* non-null objects are truthy */
    }
}

/* Output helpers that write ALREADY JSON-ESCAPED bytes into SgOutBuf */
static void sg_emit_val_to_ob(SgOutBuf *ob, SgVal val, bool escape_html) {
    switch (val.kind) {
        case VK_STR_JSON:
            /* Already JSON-escaped raw HTML; only used by {{{ page.content }}} */
            sg_ob_write(ob, val.v.s.ptr, val.v.s.len);
            break;
        case VK_STR:
            if (val.v.s.len == 0) break;
            if (escape_html) sg_ob_html_then_json_esc(ob, val.v.s.ptr, val.v.s.len);
            else sg_ob_json_esc(ob, val.v.s.ptr, val.v.s.len);
            break;
        case VK_U32:
            sg_ob_u32(ob, val.v.u);
            break;
        case VK_BOOL:
            sg_ob_write(ob, val.v.b ? "true" : "false", val.v.b ? 4 : 5);
            break;
        case VK_ARR_ALL_TAGS:
        case VK_ARR_TAG_REFS:
        case VK_ARR_CARDS:
        case VK_ARR_TOC:
            /* Object arrays yield "" joined by ", ", unless string/num/bool */
            for (uint32_t i = 0; i < val.count; i++) {
                if (i > 0) sg_ob_write(ob, ", ", 2);
            }
            break;
        default:
            break;
    }
}

static SgParsedTemplate *sg_find_parsed_tpl(const SgRenderCtx *ctx, SgStr name) {
    for (uint32_t i = 0; i < ctx->tpl_count; i++) {
        if (ctx->tpls[i].valid && ctx->tpls[i].name.len == name.len &&
            sg_memcmp(ctx->tpls[i].name.ptr, name.ptr, name.len) == 0) {
            return &ctx->tpls[i];
        }
    }
    return NULL;
}

static void sg_render_tpl_nodes(const SgRenderCtx *ctx, const SgTplNode *nodes, uint32_t count,
                                SgLoopFrame *frames, int nf, int depth, SgOutBuf *ob) {
    for (uint32_t i = 0; i < count; i++) {
        const SgTplNode *n = &nodes[i];
        switch (n->kind) {
            case TPL_NODE_TEXT:
                if (n->json_text.len) sg_ob_write(ob, n->json_text.ptr, n->json_text.len);
                break;
            case TPL_NODE_VAR:
                sg_emit_val_to_ob(ob, sg_lookup_path(ctx, frames, nf, &n->path), true);
                break;
            case TPL_NODE_RAW:
                sg_emit_val_to_ob(ob, sg_lookup_path(ctx, frames, nf, &n->path), false);
                break;
            case TPL_NODE_IF: {
                bool t = sg_val_truthy(sg_lookup_path(ctx, frames, nf, &n->path));
                bool take_then = (t != (n->if_not != 0));
                if (take_then) {
                    if (n->body_count) sg_render_tpl_nodes(ctx, n->body, n->body_count, frames, nf, depth, ob);
                } else {
                    if (n->else_count) sg_render_tpl_nodes(ctx, n->else_body, n->else_count, frames, nf, depth, ob);
                }
                break;
            }
            case TPL_NODE_FOR: {
                SgVal xs = sg_lookup_path(ctx, frames, nf, &n->path);
                uint32_t len = 0;
                uint8_t elem_kind = VK_UNDEF;
                if (xs.kind == VK_ARR_ALL_TAGS) { len = xs.count; elem_kind = VK_OBJ_ALL_TAG; }
                else if (xs.kind == VK_ARR_TAG_REFS) { len = xs.count; elem_kind = VK_OBJ_TAG_REF; }
                else if (xs.kind == VK_ARR_CARDS) { len = xs.count; elem_kind = VK_OBJ_CARD; }
                else if (xs.kind == VK_ARR_TOC) { len = xs.count; elem_kind = VK_OBJ_TOC; }
                else break;
                if (nf >= 16) break;
                for (uint32_t k = 0; k < len; k++) {
                    if (elem_kind == VK_OBJ_CARD && n->card_cache_slot >= 0 && n->card_cache_slot < 2) {
                        SgPostCard *card = ((SgPostCard **)xs.v.p)[k];
                        if (card->has_cached_card[n->card_cache_slot]) {
                            SgStr cs = card->cached_card_json[n->card_cache_slot];
                            if (cs.len) sg_ob_write(ob, cs.ptr, cs.len);
                            continue;
                        }
                    }
                    SgVal elem = { elem_kind, {0}, 0 };
                    if (elem_kind == VK_OBJ_ALL_TAG) elem.v.p = &((const SgTagObj *)xs.v.p)[k];
                    else if (elem_kind == VK_OBJ_TAG_REF) elem.v.p = &((const SgTagRef *)xs.v.p)[k];
                    else if (elem_kind == VK_OBJ_CARD) elem.v.p = ((SgPostCard **)xs.v.p)[k];
                    else elem.v.p = &((const SgTocItem *)xs.v.p)[k];

                    frames[nf].var_name = n->loop_var;
                    frames[nf].var_val = elem;
                    frames[nf].loop_val = (SgLoopObj){ k + 1, k == 0, k + 1 == len, len };

                    if (elem_kind == VK_OBJ_CARD && n->card_cache_slot >= 0 && n->card_cache_slot < 2) {
                        /* Record card rendering to cache! Wait: flush to card_buf if possible or render into temp ob?
                           Actually card caching is populated in a dedicated fast pre-pass for index/tag! */
                    }
                    sg_render_tpl_nodes(ctx, n->body, n->body_count, frames, nf + 1, depth, ob);
                }
                break;
            }
            case TPL_NODE_INCLUDE: {
                if (depth < SG_INCLUDE_DEPTH) {
                    SgParsedTemplate *inc = sg_find_parsed_tpl(ctx, n->text);
                    if (inc) sg_render_tpl_nodes(ctx, inc->nodes, inc->node_count, frames, nf, depth + 1, ob);
                }
                break;
            }
        }
    }
}

/* ---- Include Inlining, Coalescing TEXT nodes, Site-Invariant Caching & Card Detection ---- */

static uint32_t sg_count_inlined_nodes(SgParsedTemplate *tpls, uint32_t tpl_count, const SgTplNode *nodes, uint32_t count, int depth, uint32_t limit) {
    uint32_t tot = 0;
    for (uint32_t i = 0; i < count; i++) {
        const SgTplNode *n = &nodes[i];
        if (n->kind == TPL_NODE_INCLUDE && depth < SG_INCLUDE_DEPTH) {
            for (uint32_t j = 0; j < tpl_count; j++) {
                if (tpls[j].valid && tpls[j].name.len == n->text.len && sg_memcmp(tpls[j].name.ptr, n->text.ptr, n->text.len) == 0) {
                    tot += sg_count_inlined_nodes(tpls, tpl_count, tpls[j].nodes, tpls[j].node_count, depth + 1, limit);
                    break;
                }
            }
        } else {
            tot += 1;
            if (n->body_count) tot += sg_count_inlined_nodes(tpls, tpl_count, n->body, n->body_count, depth, limit);
            if (n->else_count) tot += sg_count_inlined_nodes(tpls, tpl_count, n->else_body, n->else_count, depth, limit);
        }
        if (tot > limit) return tot;
    }
    return tot;
}

static uint32_t sg_inline_copy(SgArena *a, SgParsedTemplate *tpls, uint32_t tpl_count,
                               const SgTplNode *nodes, uint32_t count, int depth, SgTplNode *dst) {
    uint32_t out_n = 0;
    for (uint32_t i = 0; i < count; i++) {
        const SgTplNode *n = &nodes[i];
        if (n->kind == TPL_NODE_INCLUDE && depth < SG_INCLUDE_DEPTH) {
            for (uint32_t j = 0; j < tpl_count; j++) {
                if (tpls[j].valid && tpls[j].name.len == n->text.len && sg_memcmp(tpls[j].name.ptr, n->text.ptr, n->text.len) == 0) {
                    out_n += sg_inline_copy(a, tpls, tpl_count, tpls[j].nodes, tpls[j].node_count, depth + 1, dst + out_n);
                    break;
                }
            }
            continue;
        }
        if (n->kind == TPL_NODE_INCLUDE) continue; /* past INCLUDE_DEPTH -> emits nothing */
        SgTplNode cp = *n;
        if (n->body_count > 0) {
            uint32_t est = sg_count_inlined_nodes(tpls, tpl_count, n->body, n->body_count, depth, 10000);
            cp.body = est ? (SgTplNode *)sg_arena_alloc(a, est * sizeof(SgTplNode), 8) : NULL;
            cp.body_count = est ? sg_inline_copy(a, tpls, tpl_count, n->body, n->body_count, depth, cp.body) : 0;
        }
        if (n->else_count > 0) {
            uint32_t est = sg_count_inlined_nodes(tpls, tpl_count, n->else_body, n->else_count, depth, 10000);
            cp.else_body = est ? (SgTplNode *)sg_arena_alloc(a, est * sizeof(SgTplNode), 8) : NULL;
            cp.else_count = est ? sg_inline_copy(a, tpls, tpl_count, n->else_body, n->else_count, depth, cp.else_body) : 0;
        }
        dst[out_n++] = cp;
    }
    return out_n;
}

/* Analyze if node tree only accesses (site, tags, recent) and loop variables declared inside it */
static bool sg_is_subtree_site_pure(const SgTplNode *nodes, uint32_t count, const SgStr *bound, int nb) {
    for (uint32_t i = 0; i < count; i++) {
        const SgTplNode *n = &nodes[i];
        if (n->kind == TPL_NODE_INCLUDE) return false;
        if (n->kind == TPL_NODE_VAR || n->kind == TPL_NODE_RAW || n->kind == TPL_NODE_IF || n->kind == TPL_NODE_FOR) {
            bool is_local = false;
            if (n->path.raw_first.len == 4 && sg_memcmp(n->path.raw_first.ptr, "loop", 4) == 0 && nb > 0) is_local = true;
            for (int b = 0; b < nb && !is_local; b++) {
                if (bound[b].len == n->path.raw_first.len &&
                    sg_memcmp(bound[b].ptr, n->path.raw_first.ptr, bound[b].len) == 0) is_local = true;
            }
            if (!is_local) {
                if (n->path.root_hint == ROOT_PAGE || n->path.root_hint == ROOT_POSTS ||
                    n->path.root_hint == ROOT_PAGER || n->path.root_hint == ROOT_TAG) return false;
                /* Note: unknown root variables outside loops evaluate to UNDEF everywhere, so they are site-pure! */
            }
        }
        if (n->kind == TPL_NODE_IF) {
            if (!sg_is_subtree_site_pure(n->body, n->body_count, bound, nb)) return false;
            if (!sg_is_subtree_site_pure(n->else_body, n->else_count, bound, nb)) return false;
        } else if (n->kind == TPL_NODE_FOR) {
            if (nb >= 16) return false;
            SgStr nb_arr[16];
            if (nb) sg_memcpy(nb_arr, bound, (size_t)nb * sizeof(SgStr));
            nb_arr[nb] = n->loop_var;
            if (!sg_is_subtree_site_pure(n->body, n->body_count, nb_arr, nb + 1)) return false;
        }
    }
    return true;
}

/* Analyze if a `for p in posts` / `for p in tag.posts` body ONLY depends on (p, site, tags, recent) + inner locals */
static bool sg_is_loop_card_pure(const SgTplNode *nodes, uint32_t count, const SgStr *bound, int nb) {
    for (uint32_t i = 0; i < count; i++) {
        const SgTplNode *n = &nodes[i];
        if (n->kind == TPL_NODE_INCLUDE) return false;
        if (n->kind == TPL_NODE_VAR || n->kind == TPL_NODE_RAW || n->kind == TPL_NODE_IF || n->kind == TPL_NODE_FOR) {
            /* Do NOT allow `loop.*` referencing outer card loop (nb==1 means outer card loop is bound[0], no inner loop!) */
            bool is_inner_loop = (n->path.raw_first.len == 4 && sg_memcmp(n->path.raw_first.ptr, "loop", 4) == 0 && nb > 1);
            bool is_bound = false;
            for (int b = 0; b < nb && !is_bound; b++) {
                if (bound[b].len == n->path.raw_first.len &&
                    sg_memcmp(bound[b].ptr, n->path.raw_first.ptr, bound[b].len) == 0) is_bound = true;
            }
            if (!is_inner_loop && !is_bound) {
                if (n->path.root_hint == ROOT_PAGE || n->path.root_hint == ROOT_POSTS ||
                    n->path.root_hint == ROOT_PAGER || n->path.root_hint == ROOT_TAG ||
                    (n->path.raw_first.len == 4 && sg_memcmp(n->path.raw_first.ptr, "loop", 4) == 0)) return false;
            }
        }
        if (n->kind == TPL_NODE_IF) {
            if (!sg_is_loop_card_pure(n->body, n->body_count, bound, nb)) return false;
            if (!sg_is_loop_card_pure(n->else_body, n->else_count, bound, nb)) return false;
        } else if (n->kind == TPL_NODE_FOR) {
            if (nb >= 16) return false;
            SgStr nb_arr[16];
            sg_memcpy(nb_arr, bound, (size_t)nb * sizeof(SgStr));
            nb_arr[nb] = n->loop_var;
            if (!sg_is_loop_card_pure(n->body, n->body_count, nb_arr, nb + 1)) return false;
        }
    }
    return true;
}

/* Fold site-pure runs into single TEXT nodes and coalesce adjacent TEXT nodes */
static void sg_buf_json_esc_s(SgBuf *b, const char *s, size_t len) {
    const uint8_t *p = (const uint8_t *)s;
    size_t i = 0;
    sg_buf_reserve(b, len * 6 + 64);
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
            sg_memcpy(b->data + b->len, p + i, 32);
            b->len += 32;
            i += 32;
            continue;
        }
        uint32_t k = (uint32_t)__builtin_ctz(mask);
        if (k > 0) { sg_memcpy(b->data + b->len, p + i, k); b->len += k; i += k; }
        b->len += sg_json_esc_need_char(p[i++], (char *)(b->data + b->len));
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
            sg_memcpy(b->data + b->len, p + i, 16);
            b->len += 16;
            i += 16;
            continue;
        }
        uint32_t k = (uint32_t)__builtin_ctz(mask);
        if (k > 0) { sg_memcpy(b->data + b->len, p + i, k); b->len += k; i += k; }
        b->len += sg_json_esc_need_char(p[i++], (char *)(b->data + b->len));
    }
#elif defined(SG_USE_NEON)
    const uint8x16_t v_q = vdupq_n_u8('"');
    const uint8x16_t v_bs = vdupq_n_u8('\\');
    const uint8x16_t v_20 = vdupq_n_u8(0x20);
    while (i + 16 <= len) {
        uint8x16_t v = vld1q_u8(p + i);
        uint8x16_t m = vorrq_u8(vorrq_u8(vceqq_u8(v, v_q), vceqq_u8(v, v_bs)), vcltq_u8(v, v_20));
        uint64_t mask = vget_lane_u64(vreinterpret_u64_u8(vshrn_n_u16(vreinterpretq_u16_u8(m), 4)), 0);
        if (SG_LIKELY(mask == 0)) {
            sg_memcpy(b->data + b->len, p + i, 16);
            b->len += 16;
            i += 16;
            continue;
        }
        uint32_t k = (uint32_t)(__builtin_ctzll(mask) >> 2);
        if (k > 0) { sg_memcpy(b->data + b->len, p + i, k); b->len += k; i += k; }
        b->len += sg_json_esc_need_char(p[i++], (char *)(b->data + b->len));
    }
#endif
    while (i < len) {
        uint8_t c = p[i++];
        if (c < 0x20 || c == '"' || c == '\\') {
            b->len += sg_json_esc_need_char(c, (char *)(b->data + b->len));
        } else {
            b->data[b->len++] = c;
        }
    }
}

static void sg_buf_html_then_json_s(SgBuf *b, const char *s, size_t len) {
    const uint8_t *p = (const uint8_t *)s;
    size_t i = 0;
    sg_buf_reserve(b, len * 6 + 64);
#if defined(SG_USE_AVX2)
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
            sg_memcpy(b->data + b->len, p + i, 32);
            b->len += 32;
            i += 32;
            continue;
        }
        uint32_t k = (uint32_t)__builtin_ctz(mask);
        if (k > 0) { sg_memcpy(b->data + b->len, p + i, k); b->len += k; i += k; }
        uint8_t c = p[i++];
        if (c == '&') { sg_memcpy(b->data + b->len, "&amp;", 5); b->len += 5; }
        else if (c == '<') { sg_memcpy(b->data + b->len, "&lt;", 4); b->len += 4; }
        else if (c == '>') { sg_memcpy(b->data + b->len, "&gt;", 4); b->len += 4; }
        else if (c == '"') { sg_memcpy(b->data + b->len, "&quot;", 6); b->len += 6; }
        else if (c == '\'') { sg_memcpy(b->data + b->len, "&#39;", 5); b->len += 5; }
        else b->len += sg_json_esc_need_char(c, (char *)(b->data + b->len));
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
            sg_memcpy(b->data + b->len, p + i, 16);
            b->len += 16;
            i += 16;
            continue;
        }
        uint32_t k = (uint32_t)__builtin_ctz(mask);
        if (k > 0) { sg_memcpy(b->data + b->len, p + i, k); b->len += k; i += k; }
        uint8_t c = p[i++];
        if (c == '&') { sg_memcpy(b->data + b->len, "&amp;", 5); b->len += 5; }
        else if (c == '<') { sg_memcpy(b->data + b->len, "&lt;", 4); b->len += 4; }
        else if (c == '>') { sg_memcpy(b->data + b->len, "&gt;", 4); b->len += 4; }
        else if (c == '"') { sg_memcpy(b->data + b->len, "&quot;", 6); b->len += 6; }
        else if (c == '\'') { sg_memcpy(b->data + b->len, "&#39;", 5); b->len += 5; }
        else b->len += sg_json_esc_need_char(c, (char *)(b->data + b->len));
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
            sg_memcpy(b->data + b->len, p + i, 16);
            b->len += 16;
            i += 16;
            continue;
        }
        uint32_t k = (uint32_t)(__builtin_ctzll(mask) >> 2);
        if (k > 0) { sg_memcpy(b->data + b->len, p + i, k); b->len += k; i += k; }
        uint8_t c = p[i++];
        if (c == '&') { sg_memcpy(b->data + b->len, "&amp;", 5); b->len += 5; }
        else if (c == '<') { sg_memcpy(b->data + b->len, "&lt;", 4); b->len += 4; }
        else if (c == '>') { sg_memcpy(b->data + b->len, "&gt;", 4); b->len += 4; }
        else if (c == '"') { sg_memcpy(b->data + b->len, "&quot;", 6); b->len += 6; }
        else if (c == '\'') { sg_memcpy(b->data + b->len, "&#39;", 5); b->len += 5; }
        else b->len += sg_json_esc_need_char(c, (char *)(b->data + b->len));
    }
#endif
    while (i < len) {
        uint8_t c = p[i++];
        if (c == '&') { sg_memcpy(b->data + b->len, "&amp;", 5); b->len += 5; }
        else if (c == '<') { sg_memcpy(b->data + b->len, "&lt;", 4); b->len += 4; }
        else if (c == '>') { sg_memcpy(b->data + b->len, "&gt;", 4); b->len += 4; }
        else if (c == '"') { sg_memcpy(b->data + b->len, "&quot;", 6); b->len += 6; }
        else if (c == '\'') { sg_memcpy(b->data + b->len, "&#39;", 5); b->len += 5; }
        else if (c < 0x20 || c == '\\') b->len += sg_json_esc_need_char(c, (char *)(b->data + b->len));
        else b->data[b->len++] = c;
    }
}

static void sg_emit_val_to_buf(SgBuf *b, SgVal val, bool escape_html) {
    switch (val.kind) {
        case VK_STR_JSON:
            sg_buf_write_s(b, val.v.s.ptr, val.v.s.len);
            break;
        case VK_STR:
            if (val.v.s.len == 0) break;
            if (escape_html) sg_buf_html_then_json_s(b, val.v.s.ptr, val.v.s.len);
            else sg_buf_json_esc_s(b, val.v.s.ptr, val.v.s.len);
            break;
        case VK_U32: {
            char tmp[16]; int idx = 16; uint32_t v = val.v.u;
            do { tmp[--idx] = (char)('0' + (v % 10u)); v /= 10u; } while (v);
            sg_buf_write_s(b, tmp + idx, (size_t)(16 - idx));
            break;
        }
        case VK_BOOL:
            sg_buf_write_s(b, val.v.b ? "true" : "false", val.v.b ? 4 : 5);
            break;
        case VK_ARR_ALL_TAGS:
        case VK_ARR_TAG_REFS:
        case VK_ARR_CARDS:
        case VK_ARR_TOC:
            for (uint32_t i = 0; i < val.count; i++) {
                if (i > 0) sg_buf_write_s(b, ", ", 2);
            }
            break;
        default: break;
    }
}

static void sg_render_nodes_to_buf(const SgRenderCtx *ctx, const SgTplNode *nodes, uint32_t count,
                                   SgLoopFrame *frames, int nf, SgBuf *buf) {
    for (uint32_t i = 0; i < count; i++) {
        const SgTplNode *n = &nodes[i];
        switch (n->kind) {
            case TPL_NODE_TEXT:
                if (n->json_text.len) sg_buf_write_s(buf, n->json_text.ptr, n->json_text.len);
                break;
            case TPL_NODE_VAR:
                sg_emit_val_to_buf(buf, sg_lookup_path(ctx, frames, nf, &n->path), true);
                break;
            case TPL_NODE_RAW:
                sg_emit_val_to_buf(buf, sg_lookup_path(ctx, frames, nf, &n->path), false);
                break;
            case TPL_NODE_IF: {
                bool t = sg_val_truthy(sg_lookup_path(ctx, frames, nf, &n->path));
                if (t != (n->if_not != 0)) {
                    if (n->body_count) sg_render_nodes_to_buf(ctx, n->body, n->body_count, frames, nf, buf);
                } else {
                    if (n->else_count) sg_render_nodes_to_buf(ctx, n->else_body, n->else_count, frames, nf, buf);
                }
                break;
            }
            case TPL_NODE_FOR: {
                SgVal xs = sg_lookup_path(ctx, frames, nf, &n->path);
                uint32_t len = 0; uint8_t elem_kind = VK_UNDEF;
                if (xs.kind == VK_ARR_ALL_TAGS) { len = xs.count; elem_kind = VK_OBJ_ALL_TAG; }
                else if (xs.kind == VK_ARR_TAG_REFS) { len = xs.count; elem_kind = VK_OBJ_TAG_REF; }
                else if (xs.kind == VK_ARR_CARDS) { len = xs.count; elem_kind = VK_OBJ_CARD; }
                else if (xs.kind == VK_ARR_TOC) { len = xs.count; elem_kind = VK_OBJ_TOC; }
                else break;
                if (nf >= 16) break;
                for (uint32_t k = 0; k < len; k++) {
                    SgVal elem = { elem_kind, {0}, 0 };
                    if (elem_kind == VK_OBJ_ALL_TAG) elem.v.p = &((const SgTagObj *)xs.v.p)[k];
                    else if (elem_kind == VK_OBJ_TAG_REF) elem.v.p = &((const SgTagRef *)xs.v.p)[k];
                    else if (elem_kind == VK_OBJ_CARD) elem.v.p = ((SgPostCard **)xs.v.p)[k];
                    else elem.v.p = &((const SgTocItem *)xs.v.p)[k];
                    frames[nf].var_name = n->loop_var;
                    frames[nf].var_val = elem;
                    frames[nf].loop_val = (SgLoopObj){ k + 1, k == 0, k + 1 == len, len };
                    sg_render_nodes_to_buf(ctx, n->body, n->body_count, frames, nf + 1, buf);
                }
                break;
            }
            default: break;
        }
    }
}

/* Fold site-pure runs inside a node array recursively, and detect card-cacheable FOR loops */
static uint32_t sg_optimize_nodes(SgArena *a, const SgRenderCtx *ctx, SgTplNode *nodes, uint32_t count,
                                  const SgStr *bound, int nb, SgBuf *scratch_buf,
                                  const SgTplNode **card_loop_bodies, int *card_slot_counter) {
    uint32_t w = 0;
    uint32_t i = 0;
    SgLoopFrame frames[16];
    while (i < count) {
        /* Try collecting maximal run of site-pure nodes starting at i (only when nb == 0 so outer loop vars aren't mistaken for unknown roots!) */
        if (nb == 0 && sg_is_subtree_site_pure(&nodes[i], 1, bound, nb)) {
            uint32_t j = i + 1;
            while (j < count && sg_is_subtree_site_pure(&nodes[j], 1, bound, nb)) j++;
            /* Fold i..j-1 into a single TPL_NODE_TEXT (if count > 1 or single non-text node) */
            if (j - i > 1 || nodes[i].kind != TPL_NODE_TEXT) {
                scratch_buf->len = 0;
                sg_render_nodes_to_buf(ctx, nodes + i, j - i, frames, 0, scratch_buf);
                SgTplNode txt_nd = {0};
                txt_nd.kind = TPL_NODE_TEXT;
                txt_nd.json_text.ptr = sg_arena_strdup(a, (const char *)scratch_buf->data, scratch_buf->len);
                txt_nd.json_text.len = (uint32_t)scratch_buf->len;
                nodes[w++] = txt_nd;
                i = j;
                continue;
            } else {
                nodes[w++] = nodes[i++];
                continue;
            }
        }
        SgTplNode nd = nodes[i++];
        if (nd.kind == TPL_NODE_IF) {
            if (nd.body_count)
                nd.body_count = sg_optimize_nodes(a, ctx, nd.body, nd.body_count, bound, nb, scratch_buf, card_loop_bodies, card_slot_counter);
            if (nd.else_count)
                nd.else_count = sg_optimize_nodes(a, ctx, nd.else_body, nd.else_count, bound, nb, scratch_buf, card_loop_bodies, card_slot_counter);
        } else if (nd.kind == TPL_NODE_FOR) {
            if (nb < 16) {
                SgStr nb_arr[16];
                if (nb) sg_memcpy(nb_arr, bound, (size_t)nb * sizeof(SgStr));
                nb_arr[nb] = nd.loop_var;
                if (nd.body_count)
                    nd.body_count = sg_optimize_nodes(a, ctx, nd.body, nd.body_count, nb_arr, nb + 1, scratch_buf, card_loop_bodies, card_slot_counter);
                /* Check if this is `for p in posts` or `for p in tag.posts` at top level (nb == 0) */
                bool is_posts_loop = (nb == 0) &&
                    ((nd.path.root_hint == ROOT_POSTS && nd.path.plen == 1) ||
                     (nd.path.root_hint == ROOT_TAG && nd.path.plen == 2 && nd.path.fields[0] == FLD_POSTS));
                if (is_posts_loop && card_loop_bodies && sg_is_loop_card_pure(nd.body, nd.body_count, &nd.loop_var, 1)) {
                    /* Match against existing card_loop_bodies if identical, or assign slot 0/1 */
                    int slot = -1;
                    for (int s = 0; s < *card_slot_counter; s++) {
                        const SgTplNode *prev = card_loop_bodies[s];
                        if (prev && prev->body_count == nd.body_count && prev->loop_var.len == nd.loop_var.len &&
                            sg_memcmp(prev->loop_var.ptr, nd.loop_var.ptr, nd.loop_var.len) == 0 &&
                            prev->body_count == 1 && nd.body_count == 1 && prev->body[0].kind == TPL_NODE_TEXT && nd.body[0].kind == TPL_NODE_TEXT) {
                            /* Keep separate slot if different; max 2 slots */
                        }
                    }
                    if (*card_slot_counter < 2) {
                        slot = (*card_slot_counter)++;
                        nd.card_cache_slot = slot;
                        nodes[w] = nd;
                        card_loop_bodies[slot] = &nodes[w];
                        w++;
                        continue;
                    }
                }
            }
        }
        nodes[w++] = nd;
    }
    return w;
}
