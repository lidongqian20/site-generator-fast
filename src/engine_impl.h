#pragma once
#include "frontmatter.h"
#include "json_in.h"
#include "template.h"

/* Complete makefun-site 1.0.0 single-case and batch engine */

typedef struct SgSource {
    SgStr path;
    SgStr text;
    SgStr dir;
    SgFrontMatter fm;
    bool is_post;
    SgStr layout;
    SgStr slug;
    SgStr out_dir;
    SgStr url;
    uint32_t reading_minutes;
    /* Set when markdown rendered */
    SgTagRef *tag_refs;
    uint32_t tag_ref_count;
    SgPostCard card;
    SgNeighbour nb;
    /* Neighbours in collections order */
    uint32_t post_order_idx;
    struct SgSource *newer;
    struct SgSource *older;
} SgSource;

/* Simple insertion/merge sort for small arrays using by_code_units comparator */
static void sg_sort_raw_templates(SgRawTemplate *arr, uint32_t n) {
    for (uint32_t i = 1; i < n; i++) {
        SgRawTemplate key = arr[i];
        int32_t j = (int32_t)i - 1;
        while (j >= 0 && sg_by_code_units(arr[j].name.ptr, arr[j].name.len, key.name.ptr, key.name.len) > 0) {
            arr[j + 1] = arr[j];
            j--;
        }
        arr[j + 1] = key;
    }
}

static void sg_sort_raw_files(SgRawFile *arr, uint32_t n, SgRawFile *tmp) {
    if (n <= 32) {
        for (uint32_t i = 1; i < n; i++) {
            SgRawFile key = arr[i];
            int32_t j = (int32_t)i - 1;
            while (j >= 0 && sg_by_code_units(arr[j].path.ptr, arr[j].path.len, key.path.ptr, key.path.len) > 0) {
                arr[j + 1] = arr[j];
                j--;
            }
            arr[j + 1] = key;
        }
        return;
    }
    uint32_t mid = n >> 1;
    sg_sort_raw_files(arr, mid, tmp);
    sg_sort_raw_files(arr + mid, n - mid, tmp);
    uint32_t i = 0, j = mid, k = 0;
    while (i < mid && j < n) {
        if (sg_by_code_units(arr[i].path.ptr, arr[i].path.len, arr[j].path.ptr, arr[j].path.len) <= 0)
            tmp[k++] = arr[i++];
        else
            tmp[k++] = arr[j++];
    }
    while (i < mid) tmp[k++] = arr[i++];
    while (j < n) tmp[k++] = arr[j++];
    sg_memcpy(arr, tmp, n * sizeof(SgRawFile));
}

static void sg_sort_str_arr(SgStr *arr, uint32_t n, SgStr *tmp) {
    if (n <= 32) {
        for (uint32_t i = 1; i < n; i++) {
            SgStr key = arr[i];
            int32_t j = (int32_t)i - 1;
            while (j >= 0 && sg_by_code_units(arr[j].ptr, arr[j].len, key.ptr, key.len) > 0) {
                arr[j + 1] = arr[j];
                j--;
            }
            arr[j + 1] = key;
        }
        return;
    }
    uint32_t mid = n >> 1;
    sg_sort_str_arr(arr, mid, tmp);
    sg_sort_str_arr(arr + mid, n - mid, tmp);
    uint32_t i = 0, j = mid, k = 0;
    while (i < mid && j < n) {
        if (sg_by_code_units(arr[i].ptr, arr[i].len, arr[j].ptr, arr[j].len) <= 0)
            tmp[k++] = arr[i++];
        else
            tmp[k++] = arr[j++];
    }
    while (i < mid) tmp[k++] = arr[i++];
    while (j < n) tmp[k++] = arr[j++];
    sg_memcpy(arr, tmp, n * sizeof(SgStr));
}

static void sg_sort_posts(uint32_t *order, const SgSource *srcs, uint32_t n, uint32_t *tmp) {
    if (n <= 32) {
        for (uint32_t i = 1; i < n; i++) {
            uint32_t key = order[i];
            int32_t j = (int32_t)i - 1;
            while (j >= 0) {
                const SgSource *a = &srcs[order[j]];
                const SgSource *b = &srcs[key];
                int cmp = sg_by_code_units(b->fm.date.ptr, b->fm.date.len, a->fm.date.ptr, a->fm.date.len);
                if (cmp == 0) cmp = sg_by_code_units(a->slug.ptr, a->slug.len, b->slug.ptr, b->slug.len);
                if (cmp <= 0) break;
                order[j + 1] = order[j];
                j--;
            }
            order[j + 1] = key;
        }
        return;
    }
    uint32_t mid = n >> 1;
    sg_sort_posts(order, srcs, mid, tmp);
    sg_sort_posts(order + mid, srcs, n - mid, tmp);
    uint32_t i = 0, j = mid, k = 0;
    while (i < mid && j < n) {
        const SgSource *a = &srcs[order[i]];
        const SgSource *b = &srcs[order[j]];
        int cmp = sg_by_code_units(b->fm.date.ptr, b->fm.date.len, a->fm.date.ptr, a->fm.date.len);
        if (cmp == 0) cmp = sg_by_code_units(a->slug.ptr, a->slug.len, b->slug.ptr, b->slug.len);
        if (cmp <= 0) tmp[k++] = order[i++];
        else tmp[k++] = order[j++];
    }
    while (i < mid) tmp[k++] = order[i++];
    while (j < n) tmp[k++] = order[j++];
    sg_memcpy(order, tmp, n * sizeof(uint32_t));
}

static void sg_sort_tags(SgTagObj *tags, uint32_t n, SgTagObj *tmp) {
    if (n <= 32) {
        for (uint32_t i = 1; i < n; i++) {
            SgTagObj key = tags[i];
            int32_t j = (int32_t)i - 1;
            while (j >= 0) {
                int cmp = (tags[j].count < key.count) ? 1 : (tags[j].count > key.count ? -1 :
                          sg_by_code_units(tags[j].slug.ptr, tags[j].slug.len, key.slug.ptr, key.slug.len));
                if (cmp <= 0) break;
                tags[j + 1] = tags[j];
                j--;
            }
            tags[j + 1] = key;
        }
        return;
    }
    uint32_t mid = n >> 1;
    sg_sort_tags(tags, mid, tmp);
    sg_sort_tags(tags + mid, n - mid, tmp);
    uint32_t i = 0, j = mid, k = 0;
    while (i < mid && j < n) {
        int cmp = (tags[i].count < tags[j].count) ? 1 : (tags[i].count > tags[j].count ? -1 :
                  sg_by_code_units(tags[i].slug.ptr, tags[i].slug.len, tags[j].slug.ptr, tags[j].slug.len));
        if (cmp <= 0) tmp[k++] = tags[i++];
        else tmp[k++] = tags[j++];
    }
    while (i < mid) tmp[k++] = tags[i++];
    while (j < n) tmp[k++] = tags[j++];
    sg_memcpy(tags, tmp, n * sizeof(SgTagObj));
}

SG_INLINE SgStr sg_dir_of(SgStr p) {
    for (int32_t i = (int32_t)p.len - 1; i >= 0; i--) {
        if (p.ptr[i] == '/') return (SgStr){ p.ptr, (uint32_t)i };
    }
    return (SgStr){ "", 0 };
}

static const char *SG_MONTHS[12] = {
    "January", "February", "March", "April", "May", "June",
    "July", "August", "September", "October", "November", "December"
};

static SgStr sg_long_date(SgArena *a, SgStr date) {
    /* YYYY-MM-DD */
    int y = (date.ptr[0]-'0')*1000 + (date.ptr[1]-'0')*100 + (date.ptr[2]-'0')*10 + (date.ptr[3]-'0');
    int m = (date.ptr[5]-'0')*10 + (date.ptr[6]-'0');
    int d = (date.ptr[8]-'0')*10 + (date.ptr[9]-'0');
    char *b = (char *)sg_arena_alloc(a, 32, 1);
    size_t p = 0;
    if (d >= 10) b[p++] = (char)('0' + d / 10);
    b[p++] = (char)('0' + d % 10);
    b[p++] = ' ';
    const char *mon = SG_MONTHS[m - 1];
    size_t ml = sg_strlen(mon);
    sg_memcpy(b + p, mon, ml); p += ml;
    b[p++] = ' ';
    b[p++] = (char)('0' + (y / 1000) % 10);
    b[p++] = (char)('0' + (y / 100) % 10);
    b[p++] = (char)('0' + (y / 10) % 10);
    b[p++] = (char)('0' + y % 10);
    b[p] = '\0';
    return (SgStr){ b, (uint32_t)p };
}

/* Count words matching JS /\S+/g */
static uint32_t sg_words_of(const char *s, size_t len) {
    uint32_t words = 0;
    bool in_word = false;
    const uint8_t *p = (const uint8_t *)s;
    size_t i = 0;
    while (i < len) {
        uint8_t c = p[i];
        bool ws;
        if (SG_LIKELY(c < 0x80)) {
            ws = (c == ' ' || (uint8_t)(c - '\t') <= ('\r' - '\t'));
            i++;
        } else {
            size_t step = 1;
            ws = sg_is_js_space_utf8(p + i, len - i, &step);
            i += step;
        }
        if (!ws) {
            if (!in_word) { words++; in_word = true; }
        } else {
            in_word = false;
        }
    }
    return words;
}

typedef struct {
    SgStrSet keys;
} SgDirTaken;

typedef struct {
    SgStr *dir_keys;
    SgDirTaken *dir_vals;
    uint32_t cap;
    uint32_t count;
} SgDirMap;

static SgDirTaken *sg_get_dir_taken(SgArena *a, SgDirMap *dm, SgStr dir) {
    if (dm->cap == 0 || (dm->count + 1) * 2 >= dm->cap) {
        uint32_t ncap = dm->cap ? dm->cap * 2 : 64;
        SgStr *nk = (SgStr *)sg_arena_alloc(a, ncap * sizeof(SgStr), 8);
        SgDirTaken *nv = (SgDirTaken *)sg_arena_alloc(a, ncap * sizeof(SgDirTaken), 8);
        sg_memset(nk, 0, ncap * sizeof(SgStr));
        sg_memset(nv, 0, ncap * sizeof(SgDirTaken));
        for (uint32_t i = 0; i < dm->cap; i++) {
            if (dm->dir_keys[i].ptr != NULL) {
                uint64_t h = sg_fnv1a(dm->dir_keys[i].ptr, dm->dir_keys[i].len);
                uint32_t idx = (uint32_t)h & (ncap - 1);
                while (nk[idx].ptr != NULL) idx = (idx + 1) & (ncap - 1);
                nk[idx] = dm->dir_keys[i];
                nv[idx] = dm->dir_vals[i];
            }
        }
        dm->dir_keys = nk;
        dm->dir_vals = nv;
        dm->cap = ncap;
    }
    uint64_t h = sg_fnv1a(dir.ptr, dir.len);
    uint32_t idx = (uint32_t)h & (dm->cap - 1);
    while (dm->dir_keys[idx].ptr != NULL) {
        if (dm->dir_keys[idx].len == dir.len && sg_memcmp(dm->dir_keys[idx].ptr, dir.ptr, dir.len) == 0)
            return &dm->dir_vals[idx];
        idx = (idx + 1) & (dm->cap - 1);
    }
    dm->count++;
    dm->dir_keys[idx] = (SgStr){ dir.len ? dir.ptr : "", dir.len };
    SgDirTaken *dt = &dm->dir_vals[idx];
    dt->keys.cap = 0; dt->keys.count = 0; dt->keys.keys = NULL;
    if (dir.len == 0) {
        sg_strset_insert(a, &dt->keys, "posts", 5);
        sg_strset_insert(a, &dt->keys, "tags", 4);
        sg_strset_insert(a, &dt->keys, "page", 4);
    }
    return dt;
}

static SgStr sg_fmt_idx_url(SgArena *a, SgStr base, uint32_t pnum) {
    if (pnum == 1) {
        char *dst = (char *)sg_arena_alloc(a, base.len + 2, 1);
        sg_memcpy(dst, base.ptr, base.len);
        dst[base.len] = '/';
        dst[base.len + 1] = '\0';
        return (SgStr){ dst, base.len + 1 };
    }
    char nb[16]; int ni = 16; uint32_t x = pnum;
    do { nb[--ni] = (char)('0' + (x % 10u)); x /= 10u; } while (x);
    size_t nl = (size_t)(16 - ni);
    size_t tot = base.len + 6 + nl + 1;
    char *dst = (char *)sg_arena_alloc(a, tot + 1, 1);
    sg_memcpy(dst, base.ptr, base.len);
    sg_memcpy(dst + base.len, "/page/", 6);
    sg_memcpy(dst + base.len + 6, nb + ni, nl);
    dst[tot - 1] = '/';
    dst[tot] = '\0';
    return (SgStr){ dst, (uint32_t)tot };
}

static SgStr sg_unique_slug(SgArena *a, const char *base, size_t blen, SgStrSet *taken) {
    if (sg_strset_insert(a, taken, base, blen)) {
        return (SgStr){ sg_arena_strdup(a, base, blen), (uint32_t)blen };
    }
    char buf[88];
    sg_memcpy(buf, base, blen);
    buf[blen] = '-';
    for (uint32_t n = 2; ; n++) {
        char nb[12]; int ni = 12; uint32_t x = n;
        do { nb[--ni] = (char)('0' + (x % 10u)); x /= 10u; } while (x);
        size_t nl = (size_t)(12 - ni);
        sg_memcpy(buf + blen + 1, nb + ni, nl);
        size_t ulen = blen + 1 + nl;
        if (sg_strset_insert(a, taken, buf, ulen)) {
            return (SgStr){ sg_arena_strdup(a, buf, ulen), (uint32_t)ulen };
        }
    }
}

typedef struct {
    SgStr slug;
    uint32_t tag_idx;
} SgTagSlot;

static int32_t sg_tagmap_find(const SgTagSlot *map, uint32_t cap, const char *s, size_t len) {
    if (cap == 0) return -1;
    uint64_t h = sg_fnv1a(s, len);
    uint32_t idx = (uint32_t)h & (cap - 1);
    while (map[idx].slug.ptr != NULL) {
        if (map[idx].slug.len == len && sg_memcmp(map[idx].slug.ptr, s, len) == 0) return (int32_t)map[idx].tag_idx;
        idx = (idx + 1) & (cap - 1);
    }
    return -1;
}

static void sg_tagmap_put(SgArena *a, SgTagSlot **map_p, uint32_t *cap_p, uint32_t *cnt_p, SgStr slug, uint32_t tag_idx) {
    if ((*cnt_p + 1) * 2 >= *cap_p) {
        uint32_t ncap = *cap_p ? (*cap_p * 2) : 128;
        SgTagSlot *nm = (SgTagSlot *)sg_arena_alloc(a, ncap * sizeof(SgTagSlot), 8);
        sg_memset(nm, 0, ncap * sizeof(SgTagSlot));
        for (uint32_t i = 0; i < *cap_p; i++) {
            if ((*map_p)[i].slug.ptr != NULL) {
                uint64_t h = sg_fnv1a((*map_p)[i].slug.ptr, (*map_p)[i].slug.len);
                uint32_t idx = (uint32_t)h & (ncap - 1);
                while (nm[idx].slug.ptr != NULL) idx = (idx + 1) & (ncap - 1);
                nm[idx] = (*map_p)[i];
            }
        }
        *map_p = nm; *cap_p = ncap;
    }
    uint64_t h = sg_fnv1a(slug.ptr, slug.len);
    uint32_t idx = (uint32_t)h & (*cap_p - 1);
    while ((*map_p)[idx].slug.ptr != NULL) idx = (idx + 1) & (*cap_p - 1);
    (*map_p)[idx].slug = slug;
    (*map_p)[idx].tag_idx = tag_idx;
    (*cnt_p)++;
}

static void sg_emit_json_key(SgOutBuf *ob, bool *first, const char *k, size_t klen) {
    if (!*first) sg_ob_putc(ob, ',');
    *first = false;
    sg_ob_putc(ob, '"');
    sg_ob_json_esc(ob, k, klen);
    sg_ob_write(ob, "\":", 2);
}

static void sg_run_single_case(SgArena *a, SgMdScratch *sc, const SgCaseInput *inp, SgOutBuf *ob) {
    /* Persistent small buffers inside case via arena */
    SgBuf fold_buf;
    uint8_t fold_stack[8192];
    sg_buf_init_static(&fold_buf, fold_stack, sizeof(fold_stack));

    /* Report lines collector */
    uint32_t rep_cap = 256, rep_cnt = 0;
    SgStr *report_lines = (SgStr *)sg_arena_alloc(a, rep_cap * sizeof(SgStr), 8);
    #define PUSH_REPORT(str_ptr, str_len) do { \
        if (rep_cnt == rep_cap) { \
            uint32_t nc = rep_cap * 2; \
            SgStr *nr = (SgStr *)sg_arena_alloc(a, nc * sizeof(SgStr), 8); \
            sg_memcpy(nr, report_lines, rep_cap * sizeof(SgStr)); \
            report_lines = nr; rep_cap = nc; \
        } \
        report_lines[rep_cnt++] = (SgStr){ (str_ptr), (uint32_t)(str_len) }; \
    } while (0)

    #define PUSH_REPORT_LIT(s1, p2, s3) do { \
        size_t l1 = sg_strlen(s1), l3 = (s3) ? sg_strlen(s3) : 0; \
        size_t tot = l1 + (p2).len + l3; \
        char *rb = (char *)sg_arena_alloc(a, tot + 1, 1); \
        sg_memcpy(rb, s1, l1); \
        if ((p2).len) sg_memcpy(rb + l1, (p2).ptr, (p2).len); \
        if (l3) sg_memcpy(rb + l1 + (p2).len, s3, l3); \
        rb[tot] = '\0'; \
        PUSH_REPORT(rb, tot); \
    } while (0)

    /* 0. Sort and parse templates */
    sg_sort_raw_templates(inp->templates, inp->tpl_count);
    SgParsedTemplate *parsed_tpls = (SgParsedTemplate *)sg_arena_alloc(a, (inp->tpl_count ? inp->tpl_count : 1) * sizeof(SgParsedTemplate), 8);
    bool any_page_content_escaped = false;
    for (uint32_t i = 0; i < inp->tpl_count; i++) {
        SgStr err = {0};
        if (!sg_parse_template(a, inp->templates[i].name, inp->templates[i].src, &parsed_tpls[i], &err)) {
            PUSH_REPORT(err.ptr, err.len);
        } else if (parsed_tpls[i].uses_page_content_escaped) {
            any_page_content_escaped = true;
        }
    }

    /* Trim trailing slashes from site.base_url */
    SgStr base = inp->site_base_url;
    while (base.len > 0 && base.ptr[base.len - 1] == '/') base.len--;

    /* 1. Sort files and split .md vs static */
    SgRawFile *file_tmp = (SgRawFile *)sg_arena_alloc(a, (inp->file_count ? inp->file_count : 1) * sizeof(SgRawFile), 8);
    sg_sort_raw_files(inp->files, inp->file_count, file_tmp);

    uint32_t md_cap = 0, st_cap = 0;
    for (uint32_t i = 0; i < inp->file_count; i++) {
        if (inp->files[i].is_md) md_cap++; else st_cap++;
    }
    SgSource *sources = (SgSource *)sg_arena_alloc(a, (md_cap ? md_cap : 1) * sizeof(SgSource), 8);
    uint32_t src_cnt = 0;
    SgRawFile *statics = (SgRawFile *)sg_arena_alloc(a, (st_cap ? st_cap : 1) * sizeof(SgRawFile), 8);
    uint32_t static_cnt = 0;
    SgStrSet static_paths = {0};

    /* 2. frontMatter */
    for (uint32_t i = 0; i < inp->file_count; i++) {
        SgRawFile *rf = &inp->files[i];
        if (!rf->is_md) {
            statics[static_cnt++] = *rf;
            sg_strset_insert(a, &static_paths, rf->path.ptr, rf->path.len);
            continue;
        }
        SgFrontMatter fm;
        sg_parse_frontmatter(a, rf->md_text.ptr, rf->md_text.len, &fm);
        if (fm.has_error) {
            size_t tot = 8 + rf->path.len + 2 + fm.error_len;
            char *rb = (char *)sg_arena_alloc(a, tot + 1, 1);
            sg_memcpy(rb, "refused ", 8);
            sg_memcpy(rb + 8, rf->path.ptr, rf->path.len);
            sg_memcpy(rb + 8 + rf->path.len, ": ", 2);
            sg_memcpy(rb + 10 + rf->path.len, fm.error, fm.error_len);
            rb[tot] = '\0';
            PUSH_REPORT(rb, tot);
            continue;
        }
        bool is_post = (rf->path.len >= 6 && sg_memcmp(rf->path.ptr, "posts/", 6) == 0);
        const char *own = NULL;
        if (rf->path.len >= 5 && sg_memcmp(rf->path.ptr, "tags/", 5) == 0) own = "tags";
        else if (rf->path.len >= 5 && sg_memcmp(rf->path.ptr, "page/", 5) == 0) own = "page";
        if (own != NULL) {
            size_t ol = sg_strlen(own);
            size_t tot = 8 + rf->path.len + 2 + ol + 24;
            char *rb = (char *)sg_arena_alloc(a, tot + 1, 1);
            size_t p = 0;
            sg_memcpy(rb + p, "refused ", 8); p += 8;
            sg_memcpy(rb + p, rf->path.ptr, rf->path.len); p += rf->path.len;
            sg_memcpy(rb + p, ": ", 2); p += 2;
            sg_memcpy(rb + p, own, ol); p += ol;
            sg_memcpy(rb + p, "/ is the generator's own", 24); p += 24;
            rb[p] = '\0';
            PUSH_REPORT(rb, p);
            continue;
        }
        if (is_post && !fm.has_date) {
            PUSH_REPORT_LIT("refused ", rf->path, ": a post needs a date");
            continue;
        }
        SgStr layout = fm.has_layout ? fm.layout : (is_post ? (SgStr){ "post", 4 } : (SgStr){ "page", 4 });
        bool tpl_exists = false;
        for (uint32_t t = 0; t < inp->tpl_count; t++) {
            if (parsed_tpls[t].valid && parsed_tpls[t].name.len == layout.len &&
                sg_memcmp(parsed_tpls[t].name.ptr, layout.ptr, layout.len) == 0) {
                tpl_exists = true; break;
            }
        }
        if (!tpl_exists) {
            size_t tot = 8 + rf->path.len + 10 + layout.len + 28;
            char *rb = (char *)sg_arena_alloc(a, tot + 1, 1);
            size_t p = 0;
            sg_memcpy(rb + p, "refused ", 8); p += 8;
            sg_memcpy(rb + p, rf->path.ptr, rf->path.len); p += rf->path.len;
            sg_memcpy(rb + p, ": layout \"", 10); p += 10;
            sg_memcpy(rb + p, layout.ptr, layout.len); p += layout.len;
            sg_memcpy(rb + p, "\" is no template that parses", 28); p += 28;
            rb[p] = '\0';
            PUSH_REPORT(rb, p);
            continue;
        }
        if (fm.has_draft && fm.draft) {
            PUSH_REPORT_LIT("draft ", rf->path, NULL);
            continue;
        }
        SgSource *s = &sources[src_cnt++];
        sg_memset(s, 0, sizeof(*s));
        s->path = rf->path;
        s->text = rf->md_text;
        s->dir = sg_dir_of(rf->path);
        s->fm = fm;
        s->is_post = is_post;
        s->layout = layout;
    }

    /* 3. permalinks */
    SgDirMap dir_map = {0};
    uint32_t smap_cap = 64;
    while (smap_cap < src_cnt * 2 + 16) smap_cap *= 2;
    SgSourceMap by_source = {
        (SgStr *)sg_arena_alloc(a, smap_cap * sizeof(SgStr), 8),
        (uint32_t *)sg_arena_alloc(a, smap_cap * sizeof(uint32_t), 4),
        smap_cap
    };
    sg_memset(by_source.keys, 0, smap_cap * sizeof(SgStr));
    SgStr *src_urls = (SgStr *)sg_arena_alloc(a, (src_cnt ? src_cnt : 1) * sizeof(SgStr), 8);

    for (uint32_t i = 0; i < src_cnt; i++) {
        SgSource *s = &sources[i];
        SgStr pdir = s->is_post ? (SgStr){ "posts", 5 } : s->dir;
        SgDirTaken *dt = sg_get_dir_taken(a, &dir_map, pdir);
        char base_slug_buf[80];
        const char *bs_ptr;
        size_t bs_len;
        if (s->fm.has_slug) {
            bs_ptr = s->fm.slug.ptr;
            bs_len = s->fm.slug.len;
        } else {
            bs_len = sg_slugify(s->fm.title.ptr, s->fm.title.len, base_slug_buf);
            bs_ptr = base_slug_buf;
        }
        s->slug = sg_unique_slug(a, bs_ptr, bs_len, &dt->keys);
        if (pdir.len == 0) {
            s->out_dir = s->slug;
        } else {
            size_t odlen = pdir.len + 1 + s->slug.len;
            char *od = (char *)sg_arena_alloc(a, odlen + 1, 1);
            sg_memcpy(od, pdir.ptr, pdir.len);
            od[pdir.len] = '/';
            sg_memcpy(od + pdir.len + 1, s->slug.ptr, s->slug.len);
            od[odlen] = '\0';
            s->out_dir = (SgStr){ od, (uint32_t)odlen };
        }
        size_t ulen = base.len + 1 + s->out_dir.len + 1;
        char *ub = (char *)sg_arena_alloc(a, ulen + 1, 1);
        sg_memcpy(ub, base.ptr, base.len);
        ub[base.len] = '/';
        sg_memcpy(ub + base.len + 1, s->out_dir.ptr, s->out_dir.len);
        ub[base.len + 1 + s->out_dir.len] = '/';
        ub[ulen] = '\0';
        s->url = (SgStr){ ub, (uint32_t)ulen };
        src_urls[i] = s->url;

        uint64_t h = sg_fnv1a(s->path.ptr, s->path.len);
        uint32_t idx = (uint32_t)h & (smap_cap - 1);
        while (by_source.keys[idx].ptr != NULL) idx = (idx + 1) & (smap_cap - 1);
        by_source.keys[idx] = s->path;
        by_source.vals[idx] = i + 1;
    }

    /* 4. collections (before markdown so we know post_order, though markdown iterates src order!) */
    uint32_t post_cnt = 0;
    for (uint32_t i = 0; i < src_cnt; i++) if (sources[i].is_post) post_cnt++;
    uint32_t *post_order = (uint32_t *)sg_arena_alloc(a, (post_cnt ? post_cnt : 1) * sizeof(uint32_t), 4);
    uint32_t *post_tmp   = (uint32_t *)sg_arena_alloc(a, (post_cnt ? post_cnt : 1) * sizeof(uint32_t), 4);
    {
        uint32_t pc = 0;
        for (uint32_t i = 0; i < src_cnt; i++) if (sources[i].is_post) post_order[pc++] = i;
        if (post_cnt > 1) sg_sort_posts(post_order, sources, post_cnt, post_tmp);
        for (uint32_t k = 0; k < post_cnt; k++) {
            SgSource *p = &sources[post_order[k]];
            p->post_order_idx = k;
            p->newer = (k == 0) ? NULL : &sources[post_order[k - 1]];
            p->older = (k + 1 == post_cnt) ? NULL : &sources[post_order[k + 1]];
        }
    }

    /* Initialize basic card & neighbour fields for all sources */
    for (uint32_t i = 0; i < src_cnt; i++) {
        SgSource *s = &sources[i];
        SgStr date_s = s->fm.has_date ? s->fm.date : (SgStr){ "", 0 };
        SgStr dlong_s = s->fm.has_date ? sg_long_date(a, s->fm.date) : (SgStr){ "", 0 };
        s->nb = (SgNeighbour){ s->fm.title, s->url, date_s, dlong_s };
        uint32_t wc = sg_words_of(s->fm.body.ptr, s->fm.body.len);
        uint32_t rmin = (wc + 199u) / 200u;
        if (rmin < 1) rmin = 1;
        s->reading_minutes = rmin;
        s->card.title = s->fm.title;
        s->card.url = s->url;
        s->card.date = date_s;
        s->card.date_long = dlong_s;
        s->card.summary = s->fm.has_summary ? s->fm.summary : (SgStr){ "", 0 };
        s->card.excerpt = (SgStr){ "", 0 };
        s->card.reading_minutes = rmin;
        s->card.tags = NULL;
        s->card.tag_count = 0;
        s->card.has_cached_card[0] = false;
        s->card.has_cached_card[1] = false;
    }

    /* 5. tags */
    uint32_t max_tags_est = 32;
    for (uint32_t k = 0; k < post_cnt; k++) max_tags_est += sources[post_order[k]].fm.tag_count;
    SgTagObj *raw_tags = (SgTagObj *)sg_arena_alloc(a, max_tags_est * sizeof(SgTagObj), 8);
    uint32_t *tag_post_caps = (uint32_t *)sg_arena_alloc(a, max_tags_est * sizeof(uint32_t), 4);
    uint32_t tag_cnt = 0;
    SgTagSlot *tag_map = NULL;
    uint32_t tag_map_cap = 0, tag_map_cnt = 0;

    /* Helper per-source ownTags deduplication */
    for (uint32_t k = 0; k < post_cnt; k++) {
        SgSource *p = &sources[post_order[k]];
        uint32_t ftc = p->fm.tag_count;
        if (ftc == 0) continue;
        SgTagRef *refs = (SgTagRef *)sg_arena_alloc(a, ftc * sizeof(SgTagRef), 8);
        uint32_t rc = 0;
        for (uint32_t ti = 0; ti < ftc; ti++) {
            SgStr tname = p->fm.tags[ti];
            char ts_buf[80];
            size_t ts_len = sg_slugify(tname.ptr, tname.len, ts_buf);
            /* Check if already in ownTags of p */
            bool dup = false;
            for (uint32_t r = 0; r < rc; r++) {
                if (refs[r].slug.len == ts_len && sg_memcmp(refs[r].slug.ptr, ts_buf, ts_len) == 0) { dup = true; break; }
            }
            if (dup) continue;
            int32_t tidx = sg_tagmap_find(tag_map, tag_map_cap, ts_buf, ts_len);
            if (tidx < 0) {
                tidx = (int32_t)tag_cnt++;
                SgStr slug_s = { sg_arena_strdup(a, ts_buf, ts_len), (uint32_t)ts_len };
                size_t ulen = base.len + 6 + ts_len + 1; /* /tags/<slug>/ */
                char *ub = (char *)sg_arena_alloc(a, ulen + 1, 1);
                sg_memcpy(ub, base.ptr, base.len);
                sg_memcpy(ub + base.len, "/tags/", 6);
                sg_memcpy(ub + base.len + 6, ts_buf, ts_len);
                ub[ulen - 1] = '/'; ub[ulen] = '\0';
                raw_tags[tidx].name = tname;
                raw_tags[tidx].slug = slug_s;
                raw_tags[tidx].url = (SgStr){ ub, (uint32_t)ulen };
                raw_tags[tidx].count = 0;
                uint32_t cap0 = 16;
                raw_tags[tidx].posts = (SgPostCard **)sg_arena_alloc(a, cap0 * sizeof(SgPostCard *), 8);
                tag_post_caps[tidx] = cap0;
                sg_tagmap_put(a, &tag_map, &tag_map_cap, &tag_map_cnt, slug_s, (uint32_t)tidx);
            }
            SgTagObj *tobj = &raw_tags[tidx];
            if (tobj->count == tag_post_caps[tidx]) {
                uint32_t ncap = tag_post_caps[tidx] * 2;
                SgPostCard **np = (SgPostCard **)sg_arena_alloc(a, ncap * sizeof(SgPostCard *), 8);
                sg_memcpy(np, tobj->posts, tobj->count * sizeof(SgPostCard *));
                tobj->posts = np; tag_post_caps[tidx] = ncap;
            }
            tobj->posts[tobj->count++] = &p->card;
            refs[rc++] = (SgTagRef){ tobj->name, tobj->slug, tobj->url };
        }
        p->card.tags = refs;
        p->card.tag_count = rc;
    }

    /* Non-post page tags */
    for (uint32_t i = 0; i < src_cnt; i++) {
        SgSource *s = &sources[i];
        if (s->is_post || s->fm.tag_count == 0) continue;
        uint32_t ftc = s->fm.tag_count;
        SgTagRef *refs = (SgTagRef *)sg_arena_alloc(a, ftc * sizeof(SgTagRef), 8);
        uint32_t rc = 0;
        for (uint32_t ti = 0; ti < ftc; ti++) {
            SgStr tname = s->fm.tags[ti];
            char ts_buf[80];
            size_t ts_len = sg_slugify(tname.ptr, tname.len, ts_buf);
            bool dup = false;
            for (uint32_t r = 0; r < rc; r++) {
                if (refs[r].slug.len == ts_len && sg_memcmp(refs[r].slug.ptr, ts_buf, ts_len) == 0) { dup = true; break; }
            }
            if (dup) continue;
            int32_t tidx = sg_tagmap_find(tag_map, tag_map_cap, ts_buf, ts_len);
            if (tidx >= 0) {
                refs[rc++] = (SgTagRef){ raw_tags[tidx].name, raw_tags[tidx].slug, raw_tags[tidx].url };
            } else {
                refs[rc++] = (SgTagRef){ tname, { sg_arena_strdup(a, ts_buf, ts_len), (uint32_t)ts_len }, { "", 0 } };
            }
        }
        s->card.tags = refs;
        s->card.tag_count = rc;
    }

    if (tag_cnt > 1) {
        SgTagObj *tag_tmp = (SgTagObj *)sg_arena_alloc(a, tag_cnt * sizeof(SgTagObj), 8);
        sg_sort_tags(raw_tags, tag_cnt, tag_tmp);
    }

    /* Build recent array */
    uint32_t recent_cnt = inp->recent < post_cnt ? inp->recent : post_cnt;
    SgPostCard **recent_cards = (SgPostCard **)sg_arena_alloc(a, (recent_cnt ? recent_cnt : 1) * sizeof(SgPostCard *), 8);
    for (uint32_t k = 0; k < recent_cnt; k++) recent_cards[k] = &sources[post_order[k]].card;

    /* 6a. Pre-render Markdown ONLY for `recent` posts FIRST if we want site-pure folding!
       Wait: `recent` posts only need `excerpt` before site-pure folding! And running Markdown in b.sources order vs recent order:
       Does rendering recent posts in advance change `b.broken` set?
       `b.broken` is a Set whose strings are sorted at the end in `report()`, so insertion order doesn't change anything!
       Wait: if we don't store `s.html` for recent posts, we'd render them twice (3-6 posts out of 400 is <1%), OR store `s.html` in arena ONLY for those 3-6 recent posts! Storing 3-6 HTML strings (~20 KB total) avoids rendering them twice! */
    SgStrSet broken_set = {0};
    SgMdEnv env = {
        .src_path = {0}, .src_dir = {0}, .base = base,
        .by_source = &by_source, .src_urls = src_urls,
        .static_paths = &static_paths, .broken = &broken_set, .arena = a
    };

    typedef struct {
        SgStr html;
        SgTocItem *toc;
        uint32_t toc_count;
    } SgSavedMd;
    bool *md_done = (bool *)sg_arena_alloc(a, (src_cnt ? src_cnt : 1) * sizeof(bool), 1);
    SgSavedMd *saved_recent_md = (SgSavedMd *)sg_arena_alloc(a, (recent_cnt ? recent_cnt : 1) * sizeof(SgSavedMd), 8);
    sg_memset(md_done, 0, src_cnt ? src_cnt : 1);

    for (uint32_t k = 0; k < recent_cnt; k++) {
        uint32_t sidx = post_order[k];
        SgSource *s = &sources[sidx];
        env.src_path = s->path;
        env.src_dir = s->dir;
        SgMdResult mres;
        sg_render_markdown(sc, &env, s->fm.body.ptr, s->fm.body.len, &mres);
        s->card.excerpt = mres.excerpt;
        /* Save JSON-escaped (or raw if any_page_content_escaped) HTML for this recent post */
        SgStr saved_html;
        if (!any_page_content_escaped) {
            fold_buf.len = 0;
            sg_buf_json_esc_s(&fold_buf, mres.html.ptr, mres.html.len);
            saved_html.ptr = sg_arena_strdup(a, (const char *)fold_buf.data, fold_buf.len);
            saved_html.len = (uint32_t)fold_buf.len;
        } else {
            saved_html.ptr = sg_arena_strdup(a, mres.html.ptr, mres.html.len);
            saved_html.len = mres.html.len;
        }
        saved_recent_md[k] = (SgSavedMd){ saved_html, mres.toc, mres.toc_count };
        md_done[sidx] = true;
    }

    /* Map src_idx -> recent slot k if md_done */
    int32_t *recent_slot_of = (int32_t *)sg_arena_alloc(a, (src_cnt ? src_cnt : 1) * sizeof(int32_t), 4);
    for (uint32_t i = 0; i < src_cnt; i++) recent_slot_of[i] = -1;
    for (uint32_t k = 0; k < recent_cnt; k++) recent_slot_of[post_order[k]] = (int32_t)k;

    /* 6b. Now site, all_tags, and recent are complete! Inline includes & fold site-pure fragments in ALL valid templates! */
    SgRenderCtx rctx = {
        .site_title = inp->site_title, .site_base = base,
        .all_tags = raw_tags, .all_tags_count = tag_cnt,
        .recent = recent_cards, .recent_count = recent_cnt,
        .page = NULL, .posts = NULL, .posts_count = 0, .has_posts = false,
        .pager = NULL, .tag = NULL,
        .tpls = parsed_tpls, .tpl_count = inp->tpl_count
    };

    /* Create copies with inlined includes (skip if count > 10000 to avoid include-bomb edge cases) */
    SgTplNode **inlined_nodes = (SgTplNode **)sg_arena_alloc(a, (inp->tpl_count ? inp->tpl_count : 1) * sizeof(SgTplNode *), 8);
    uint32_t *inlined_counts = (uint32_t *)sg_arena_alloc(a, (inp->tpl_count ? inp->tpl_count : 1) * sizeof(uint32_t), 4);
    for (uint32_t t = 0; t < inp->tpl_count; t++) {
        if (!parsed_tpls[t].valid) { inlined_nodes[t] = NULL; inlined_counts[t] = 0; continue; }
        uint32_t est = sg_count_inlined_nodes(parsed_tpls, inp->tpl_count, parsed_tpls[t].nodes, parsed_tpls[t].node_count, 0, 10000);
        if (est <= 10000 && est > 0) {
            inlined_nodes[t] = (SgTplNode *)sg_arena_alloc(a, est * sizeof(SgTplNode), 8);
            inlined_counts[t] = sg_inline_copy(a, parsed_tpls, inp->tpl_count, parsed_tpls[t].nodes, parsed_tpls[t].node_count, 0, inlined_nodes[t]);
        } else {
            inlined_nodes[t] = parsed_tpls[t].nodes;
            inlined_counts[t] = parsed_tpls[t].node_count;
        }
    }
    const SgTplNode *card_loop_bodies[2] = { NULL, NULL };
    int card_slot_counter = 0;
    for (uint32_t t = 0; t < inp->tpl_count; t++) {
        if (!parsed_tpls[t].valid) continue;
        parsed_tpls[t].nodes = inlined_nodes[t];
        parsed_tpls[t].node_count = inlined_counts[t];
        if (parsed_tpls[t].node_count > 0) {
            /* Only enable card cache slots for `index` and `tag` templates */
            bool is_index_or_tag = (parsed_tpls[t].name.len == 5 && sg_memcmp(parsed_tpls[t].name.ptr, "index", 5) == 0) ||
                                   (parsed_tpls[t].name.len == 3 && sg_memcmp(parsed_tpls[t].name.ptr, "tag", 3) == 0);
            parsed_tpls[t].node_count = sg_optimize_nodes(
                a, &rctx, parsed_tpls[t].nodes, parsed_tpls[t].node_count,
                NULL, 0, &fold_buf, is_index_or_tag ? card_loop_bodies : NULL, &card_slot_counter);
        }
    }

    /* 7. Stream output JSON object directly into OutBuf! */
    sg_ob_putc(ob, '{');
    bool first_out = true;
    SgLoopFrame frames[16];
    SgStrSet built_paths = {0};

    /* Helper macro to record built page path only when needed for static shadow check */
    #define RECORD_BUILT(pptr, plen) do { \
        if (static_cnt > 0) sg_strset_insert(a, &built_paths, (pptr), (plen)); \
    } while (0)

    /* 7a. Render every source page immediately (Markdown -> OutBuf JSON value) */
    for (uint32_t i = 0; i < src_cnt; i++) {
        SgSource *s = &sources[i];
        SgStr html_val;
        bool is_json_esc = !any_page_content_escaped;
        SgTocItem *toc_ptr;
        uint32_t toc_cnt;

        if (md_done[i]) {
            int32_t rs = recent_slot_of[i];
            html_val = saved_recent_md[rs].html;
            toc_ptr = saved_recent_md[rs].toc;
            toc_cnt = saved_recent_md[rs].toc_count;
        } else {
            env.src_path = s->path;
            env.src_dir = s->dir;
            SgMdResult mres;
            sg_render_markdown(sc, &env, s->fm.body.ptr, s->fm.body.len, &mres);
            s->card.excerpt = mres.excerpt;
            toc_ptr = mres.toc;
            toc_cnt = mres.toc_count;
            if (!any_page_content_escaped) {
                /* JSON-escape mres.html into fold_buf right now so {{{ page.content }}} blasts it directly! */
                fold_buf.len = 0;
                sg_buf_json_esc_s(&fold_buf, mres.html.ptr, mres.html.len);
                html_val.ptr = (const char *)fold_buf.data;
                html_val.len = (uint32_t)fold_buf.len;
            } else {
                html_val = mres.html;
            }
        }

        SgPageObj page_obj = {
            .card = &s->card,
            .slug = s->slug,
            .content = html_val,
            .content_is_json_esc = is_json_esc,
            .toc = toc_ptr,
            .toc_count = toc_cnt,
            .newer = s->newer ? &s->newer->nb : NULL,
            .older = s->older ? &s->older->nb : NULL,
            .is_post = s->is_post
        };
        rctx.page = &page_obj;
        rctx.posts = NULL; rctx.posts_count = 0; rctx.has_posts = false;
        rctx.pager = NULL; rctx.tag = NULL;

        SgParsedTemplate *tpl = sg_find_parsed_tpl(&rctx, s->layout);
        if (tpl) {
            if (!first_out) sg_ob_putc(ob, ',');
            first_out = false;
            sg_ob_putc(ob, '"');
            sg_ob_json_esc(ob, s->out_dir.ptr, s->out_dir.len);
            sg_ob_write(ob, "/index.html\":\"", 14);
            sg_render_tpl_nodes(&rctx, tpl->nodes, tpl->node_count, frames, 0, 0, ob);
            sg_ob_putc(ob, '"');

            if (static_cnt > 0) {
                size_t kl = s->out_dir.len + 11;
                char *kb = (char *)sg_arena_alloc(a, kl + 1, 1);
                sg_memcpy(kb, s->out_dir.ptr, s->out_dir.len);
                sg_memcpy(kb + s->out_dir.len, "/index.html", 11);
                sg_strset_insert(a, &built_paths, kb, kl);
            }
        }
    }

    /* Pre-render cached card HTML for posts! Now EVERY post has its excerpt computed! */
    if (card_slot_counter > 0 && post_cnt > 0) {
        rctx.page = NULL; rctx.posts = NULL; rctx.posts_count = 0; rctx.has_posts = false;
        rctx.pager = NULL; rctx.tag = NULL;
        for (int slot = 0; slot < card_slot_counter; slot++) {
            const SgTplNode *lnode = card_loop_bodies[slot];
            if (!lnode) continue;
            for (uint32_t k = 0; k < post_cnt; k++) {
                SgPostCard *c = &sources[post_order[k]].card;
                frames[0].var_name = lnode->loop_var;
                frames[0].var_val = (SgVal){ VK_OBJ_CARD, .v.p = c, 0 };
                frames[0].loop_val = (SgLoopObj){ 1, true, true, 1 };
                fold_buf.len = 0;
                sg_render_nodes_to_buf(&rctx, lnode->body, lnode->body_count, frames, 1, &fold_buf);
                c->cached_card_json[slot].ptr = sg_arena_strdup(a, (const char *)fold_buf.data, fold_buf.len);
                c->cached_card_json[slot].len = (uint32_t)fold_buf.len;
                c->has_cached_card[slot] = true;
            }
        }
    }

    /* 7b. Index / paginated pages */
    SgParsedTemplate *idx_tpl = sg_find_parsed_tpl(&rctx, (SgStr){ "index", 5 });
    if (idx_tpl) {
        uint32_t per = inp->per_page ? inp->per_page : 1;
        uint32_t pages = post_cnt == 0 ? 1 : (post_cnt + per - 1) / per;
        SgPostCard **all_post_cards = (SgPostCard **)sg_arena_alloc(a, (post_cnt ? post_cnt : 1) * sizeof(SgPostCard *), 8);
        for (uint32_t k = 0; k < post_cnt; k++) all_post_cards[k] = &sources[post_order[k]].card;

        for (uint32_t k = 1; k <= pages; k++) {
            uint32_t start = (k - 1) * per;
            uint32_t end = start + per;
            if (end > post_cnt) end = post_cnt;
            SgStr ucur = sg_fmt_idx_url(a, base, k);
            SgStr unewer = (k > 1) ? sg_fmt_idx_url(a, base, k - 1) : (SgStr){ "", 0 };
            SgStr uolder = (k < pages) ? sg_fmt_idx_url(a, base, k + 1) : (SgStr){ "", 0 };

            SgPagerObj pager = { k, pages, ucur, unewer, uolder };
            rctx.page = NULL;
            rctx.posts = all_post_cards + start;
            rctx.posts_count = end - start;
            rctx.has_posts = true;
            rctx.pager = &pager;
            rctx.tag = NULL;

            if (!first_out) sg_ob_putc(ob, ',');
            first_out = false;
            if (k == 1) {
                sg_ob_write(ob, "\"index.html\":\"", 14);
                RECORD_BUILT("index.html", 10);
            } else {
                char keyb[32];
                sg_memcpy(keyb, "page/", 5);
                size_t kp = 5;
                char nb[16]; int ni = 16; uint32_t x = k;
                do { nb[--ni] = (char)('0' + (x % 10u)); x /= 10u; } while (x);
                size_t nl = (size_t)(16 - ni);
                sg_memcpy(keyb + kp, nb + ni, nl); kp += nl;
                sg_memcpy(keyb + kp, "/index.html", 11); kp += 11;
                sg_ob_putc(ob, '"');
                sg_ob_write(ob, keyb, kp);
                sg_ob_write(ob, "\":\"", 3);
                RECORD_BUILT(sg_arena_strdup(a, keyb, kp), kp);
            }
            sg_render_tpl_nodes(&rctx, idx_tpl->nodes, idx_tpl->node_count, frames, 0, 0, ob);
            sg_ob_putc(ob, '"');
        }
    }

    /* 7c. Tag pages */
    SgParsedTemplate *tag_tpl = sg_find_parsed_tpl(&rctx, (SgStr){ "tag", 3 });
    if (tag_tpl) {
        for (uint32_t ti = 0; ti < tag_cnt; ti++) {
            rctx.page = NULL; rctx.posts = NULL; rctx.posts_count = 0; rctx.has_posts = false;
            rctx.pager = NULL; rctx.tag = &raw_tags[ti];
            if (!first_out) sg_ob_putc(ob, ',');
            first_out = false;
            sg_ob_write(ob, "\"tags/", 6);
            sg_ob_json_esc(ob, raw_tags[ti].slug.ptr, raw_tags[ti].slug.len);
            sg_ob_write(ob, "/index.html\":\"", 14);
            sg_render_tpl_nodes(&rctx, tag_tpl->nodes, tag_tpl->node_count, frames, 0, 0, ob);
            sg_ob_putc(ob, '"');

            if (static_cnt > 0) {
                size_t kl = 5 + raw_tags[ti].slug.len + 11;
                char *kb = (char *)sg_arena_alloc(a, kl + 1, 1);
                sg_memcpy(kb, "tags/", 5);
                sg_memcpy(kb + 5, raw_tags[ti].slug.ptr, raw_tags[ti].slug.len);
                sg_memcpy(kb + 5 + raw_tags[ti].slug.len, "/index.html", 11);
                sg_strset_insert(a, &built_paths, kb, kl);
            }
        }
    }

    /* 7d. Tags index page */
    SgParsedTemplate *tags_tpl = sg_find_parsed_tpl(&rctx, (SgStr){ "tags", 4 });
    if (tags_tpl) {
        rctx.page = NULL; rctx.posts = NULL; rctx.posts_count = 0; rctx.has_posts = false;
        rctx.pager = NULL; rctx.tag = NULL;
        if (!first_out) sg_ob_putc(ob, ',');
        first_out = false;
        sg_ob_write(ob, "\"tags/index.html\":\"", 19);
        sg_render_tpl_nodes(&rctx, tags_tpl->nodes, tags_tpl->node_count, frames, 0, 0, ob);
        sg_ob_putc(ob, '"');
        RECORD_BUILT("tags/index.html", 15);
    }

    /* 7e. Static files (raw zero-copy JSON passthrough!) */
    for (uint32_t i = 0; i < static_cnt; i++) {
        SgRawFile *sf = &statics[i];
        if (sg_strset_has(&built_paths, sf->path.ptr, sf->path.len)) {
            PUSH_REPORT_LIT("shadowed ", sf->path, ": a built page has its path");
        } else {
            sg_emit_json_key(ob, &first_out, sf->path.ptr, sf->path.len);
            sg_ob_putc(ob, '"');
            sg_ob_write(ob, sf->raw_ptr, sf->raw_len);
            sg_ob_putc(ob, '"');
        }
    }

    /* 8. build-report.txt */
    uint32_t total_rep = rep_cnt + broken_set.count;
    SgStr *all_rep = (SgStr *)sg_arena_alloc(a, (total_rep + 2) * sizeof(SgStr), 8);
    SgStr *rep_tmp = (SgStr *)sg_arena_alloc(a, (total_rep + 2) * sizeof(SgStr), 8);
    for (uint32_t i = 0; i < rep_cnt; i++) all_rep[i] = report_lines[i];
    uint32_t rp = rep_cnt;
    for (uint32_t i = 0; i < broken_set.cap; i++) {
        if (broken_set.keys[i].ptr != NULL) all_rep[rp++] = broken_set.keys[i];
    }
    if (rp > 1) sg_sort_str_arr(all_rep, rp, rep_tmp);

    uint32_t page_cnt = src_cnt - post_cnt;
    if (!first_out) sg_ob_putc(ob, ',');
    sg_ob_write(ob, "\"build-report.txt\":\"", 20);
    for (uint32_t i = 0; i < rp; i++) {
        sg_ob_json_esc(ob, all_rep[i].ptr, all_rep[i].len);
        sg_ob_write(ob, "\\n", 2);
    }
    sg_ob_write(ob, "built ", 6);
    sg_ob_u32(ob, page_cnt);
    sg_ob_write(ob, " pages, ", 8);
    sg_ob_u32(ob, post_cnt);
    sg_ob_write(ob, " posts, ", 8);
    sg_ob_u32(ob, tag_cnt);
    sg_ob_write(ob, " tags and ", 10);
    sg_ob_u32(ob, static_cnt);
    sg_ob_write(ob, " static files\\n\"}", 17);

    if (fold_buf.owns_heap) sg_buf_free_raw(fold_buf.data, fold_buf.cap);
}

#if defined(SG_USE_AVX2)
void sg_solve_all_avx2(uint8_t *in_buf, size_t in_len, SgOutBuf *ob)
#elif defined(SG_USE_SSE2)
void sg_solve_all_sse2(uint8_t *in_buf, size_t in_len, SgOutBuf *ob)
#elif defined(SG_USE_NEON)
void sg_solve_all_neon(uint8_t *in_buf, size_t in_len, SgOutBuf *ob)
#else
void sg_solve_all_scalar(uint8_t *in_buf, size_t in_len, SgOutBuf *ob)
#endif
{
    static SgArena arena;
    sg_arena_init(&arena);

    static uint8_t md_html_backing[256u * 1024u] __attribute__((aligned(64)));
    static uint8_t md_join_backing[64u * 1024u] __attribute__((aligned(64)));
    SgMdScratch sc = {0};
    sg_buf_init_static(&sc.html_buf, md_html_backing, sizeof(md_html_backing));
    sg_buf_init_static(&sc.join_buf, md_join_backing, sizeof(md_join_backing));

    const uint8_t *p = in_buf;
    const uint8_t *end = in_buf + in_len;

    p = sg_skip_ws(p, end);
    if (p >= end || *p != '[') return;
    p++;

    sg_ob_putc(ob, '[');
    bool first_case = true;

    while (p < end) {
        p = sg_skip_ws(p, end);
        if (p >= end) break;
        if (*p == ']') break;
        if (*p == ',') { p++; continue; }
        if (*p != '{') break;
        p++;

        SgCaseInput cinp;
        sg_memset(&cinp, 0, sizeof(cinp));
        bool have_inp = false;
        while (p < end) {
            p = sg_skip_ws(p, end);
            if (*p == '}') { p++; break; }
            if (*p == ',') { p++; continue; }
            SgStr ck = {0};
            p = sg_parse_json_str_to_arena(&arena, p, end, false, &ck);
            p = sg_skip_ws(p, end);
            if (*p == ':') p++;
            p = sg_skip_ws(p, end);
            if (ck.len == 5 && sg_memcmp(ck.ptr, "input", 5) == 0) {
                p = sg_parse_case_input(&arena, p, end, &cinp);
                have_inp = true;
            } else {
                p = sg_skip_json_value(p, end);
            }
        }
        if (have_inp) {
            if (!first_case) sg_ob_putc(ob, ',');
            first_case = false;
            sg_run_single_case(&arena, &sc, &cinp, ob);
            sg_arena_reset(&arena);
        }
    }

    sg_ob_putc(ob, ']');
    sg_ob_flush(ob);
    if (sc.html_buf.owns_heap) sg_buf_free_raw(sc.html_buf.data, sc.html_buf.cap);
    if (sc.join_buf.owns_heap) sg_buf_free_raw(sc.join_buf.data, sc.join_buf.cap);
}
