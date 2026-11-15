// The TON interpreter of the Atome kernel: a small tree-walking interpreter for the core of the
// TON language, written for a machine with no operating system underneath (no libc, no floats).
//
// It runs the same syntax as the desktop interpreter for: ton.let / ton.int / ton.string / ... ,
// ton.function (and `function`), if / else if / else, while, for ... in, break, continue, return,
// strings (with \n \t \" \\, `raw` strings and f"..{expr}.."), integers, true/false/nil, arrays,
// dicts, indexing, calls, d.key, method-call syntax (s.upper()), and or !, ?:.
// Also slices (a[1:3]). Not yet: decimal numbers, classes, closures and arrow functions, imports (`atome build` inlines them).
#include "kernel.h"
#ifdef ATOME_HOSTED
#include <string.h>
#endif

// ------------------------------------------------------------------ values
enum { V_NIL, V_BOOL, V_INT, V_STR, V_ARR, V_DICT, V_FUNC };
typedef struct Str { u32 len; char d[4]; } Str;
struct Arr; struct Dict; struct Node;
typedef struct Value {
    u8 t;
    union { i32 i; Str *s; struct Arr *a; struct Dict *m; struct Node *f; } u;
} Value;
typedef struct Arr { u32 len, cap; Value *v; } Arr;
typedef struct Dict { u32 len, cap; Str **k; Value *v; } Dict;

static const Value NIL = { V_NIL, { 0 } };
static Value mk_int(i32 i) { Value v; v.t = V_INT; v.u.i = i; return v; }
static Value mk_bool(int b) { Value v; v.t = V_BOOL; v.u.i = b != 0; return v; }

// ------------------------------------------------------------------ errors
static void *err_jmp[5];
static int vm_failed;
static void fail(int line, const char *a, const char *b) {
    console_color(12, 0);
    console_puts("Error at line "); console_print_int(line); console_puts(": ");
    console_puts(a); if (b) console_puts(b);
    console_putc('\n'); console_color(7, 0);
    vm_failed = 1;
    __builtin_longjmp(err_jmp, 1);
}

// ------------------------------------------------------------------ strings and containers
static Str *str_new(const char *s, u32 n) {
    Str *r = kalloc(sizeof(Str) + n);
    if (!r) fail(0, "out of memory", NULL);
    r->len = n;
    memcpy(r->d, s, n);
    r->d[n] = 0;
    return r;
}
static Value mk_str(const char *s, u32 n) { Value v; v.t = V_STR; v.u.s = str_new(s, n); return v; }
static Value mk_arr(u32 cap) {
    Value v; v.t = V_ARR;
    v.u.a = kalloc(sizeof(Arr));
    if (!v.u.a) fail(0, "out of memory", NULL);
    v.u.a->len = 0; v.u.a->cap = cap ? cap : 4;
    v.u.a->v = kalloc(sizeof(Value) * v.u.a->cap);
    if (!v.u.a->v) fail(0, "out of memory", NULL);
    return v;
}
static void arr_push(Arr *a, Value x) {
    if (a->len == a->cap) {
        u32 nc = a->cap * 2;
        Value *nv = kalloc(sizeof(Value) * nc);
        if (!nv) fail(0, "out of memory", NULL);
        memcpy(nv, a->v, sizeof(Value) * a->len);
        a->v = nv; a->cap = nc;
    }
    a->v[a->len++] = x;
}
static Value mk_dict(void) {
    Value v; v.t = V_DICT;
    v.u.m = kalloc(sizeof(Dict));
    if (!v.u.m) fail(0, "out of memory", NULL);
    v.u.m->len = 0; v.u.m->cap = 0; v.u.m->k = NULL; v.u.m->v = NULL;
    return v;
}
static int str_eq(const Str *a, const Str *b) { return a->len == b->len && memcmp(a->d, b->d, a->len) == 0; }
static int dict_find(Dict *m, const Str *k) {
    for (u32 i = 0; i < m->len; i++) if (str_eq(m->k[i], k)) return (int)i;
    return -1;
}
static void dict_set(Dict *m, Str *k, Value x) {
    int i = dict_find(m, k);
    if (i >= 0) { m->v[i] = x; return; }
    if (m->len == m->cap) {
        u32 nc = m->cap ? m->cap * 2 : 4;
        Str **nk = kalloc(sizeof(Str *) * nc);
        Value *nv = kalloc(sizeof(Value) * nc);
        if (!nk || !nv) fail(0, "out of memory", NULL);
        if (m->len) { memcpy(nk, m->k, sizeof(Str *) * m->len); memcpy(nv, m->v, sizeof(Value) * m->len); }
        m->k = nk; m->v = nv; m->cap = nc;
    }
    m->k[m->len] = k; m->v[m->len] = x; m->len++;
}
static void dict_remove(Dict *m, const Str *k) {
    int i = dict_find(m, k);
    if (i < 0) return;
    for (u32 j = (u32)i; j + 1 < m->len; j++) { m->k[j] = m->k[j + 1]; m->v[j] = m->v[j + 1]; }
    m->len--;
}

static int truthy(Value v) {
    switch (v.t) {
        case V_NIL: return 0;
        case V_BOOL: case V_INT: return v.u.i != 0;
        case V_STR: return v.u.s->len != 0;
        case V_ARR: return v.u.a->len != 0;
        case V_DICT: return v.u.m->len != 0;
        default: return 1;
    }
}
static int val_eq(Value a, Value b) {
    if (a.t != b.t) return 0;
    switch (a.t) {
        case V_NIL: return 1;
        case V_BOOL: case V_INT: return a.u.i == b.u.i;
        case V_STR: return str_eq(a.u.s, b.u.s);
        case V_ARR: return a.u.a == b.u.a;
        case V_DICT: return a.u.m == b.u.m;
        default: return a.u.f == b.u.f;
    }
}
static const char *type_name(Value v) {
    static const char *n[] = { "nil", "bool", "int", "string", "array", "dict", "function" };
    return n[v.t];
}

// text of a value, appended to a growing buffer
typedef struct Buf { char *d; u32 len, cap; } Buf;
static void buf_add(Buf *b, const char *s, u32 n) {
    if (b->len + n + 1 > b->cap) {
        u32 nc = b->cap ? b->cap : 64;
        while (nc < b->len + n + 1) nc *= 2;
        char *nd = kalloc(nc);
        if (!nd) fail(0, "out of memory", NULL);
        if (b->len) memcpy(nd, b->d, b->len);
        b->d = nd; b->cap = nc;
    }
    memcpy(b->d + b->len, s, n);
    b->len += n;
    b->d[b->len] = 0;
}
static void val_text(Buf *b, Value v, int quote) {
    char tmp[16];
    switch (v.t) {
        case V_NIL: buf_add(b, "nil", 3); break;
        case V_BOOL: if (v.u.i) buf_add(b, "true", 4); else buf_add(b, "false", 5); break;
        case V_INT: buf_add(b, tmp, (u32)kitoa(v.u.i, tmp)); break;
        case V_STR:
            if (quote) buf_add(b, "\"", 1);
            buf_add(b, v.u.s->d, v.u.s->len);
            if (quote) buf_add(b, "\"", 1);
            break;
        case V_ARR:
            buf_add(b, "[", 1);
            for (u32 i = 0; i < v.u.a->len; i++) { if (i) buf_add(b, ", ", 2); val_text(b, v.u.a->v[i], 1); }
            buf_add(b, "]", 1);
            break;
        case V_DICT:
            buf_add(b, "{", 1);
            for (u32 i = 0; i < v.u.m->len; i++) {
                if (i) buf_add(b, ", ", 2);
                buf_add(b, v.u.m->k[i]->d, v.u.m->k[i]->len); buf_add(b, ": ", 2);
                val_text(b, v.u.m->v[i], 1);
            }
            buf_add(b, "}", 1);
            break;
        default: buf_add(b, "<function>", 10);
    }
}

// ------------------------------------------------------------------ lexer
enum { K_EOF, K_NUM, K_STR, K_FSTR, K_ID, K_OP };
typedef struct Token { u8 k; u16 op; i32 num; const char *s; u32 n; int line; } Token;

static const char **names; static u32 name_n, name_cap;
static const char *intern(const char *s, u32 n) {
    for (u32 i = 0; i < name_n; i++) if (strlen(names[i]) == n && memcmp(names[i], s, n) == 0) return names[i];
    if (name_n == name_cap) {
        u32 nc = name_cap ? name_cap * 2 : 64;
        const char **nn = kalloc(sizeof(char *) * nc);
        if (!nn) fail(0, "out of memory", NULL);
        if (name_n) memcpy(nn, names, sizeof(char *) * name_n);
        names = nn; name_cap = nc;
    }
    char *c = kalloc(n + 1);
    if (!c) fail(0, "out of memory", NULL);
    memcpy(c, s, n); c[n] = 0;
    return names[name_n++] = c;
}

static Token *toks; static u32 tok_n, tok_cap;
static void tok_add(Token t) {
    if (tok_n == tok_cap) {
        u32 nc = tok_cap ? tok_cap * 2 : 256;
        Token *nt = kalloc(sizeof(Token) * nc);
        if (!nt) fail(t.line, "out of memory", NULL);
        if (tok_n) memcpy(nt, toks, sizeof(Token) * tok_n);
        toks = nt; tok_cap = nc;
    }
    toks[tok_n++] = t;
}
static int is_alpha(int c) { return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_'; }
static int is_digit(int c) { return c >= '0' && c <= '9'; }

#define OP2(a, b) ((u16)(((a) << 8) | (b)))
// Lexes src[0..len) into the global token array. Newlines are only whitespace: the grammar is unambiguous without them.
static void lex(const char *src, u32 len) {
    u32 i = 0; int line = 1;
    while (i < len) {
        char c = src[i];
        if (c == '\n') { line++; i++; continue; }
        if (c == ' ' || c == '\t' || c == '\r' || c == ';') { i++; continue; }
        if (c == '/' && i + 1 < len && src[i + 1] == '/') { while (i < len && src[i] != '\n') i++; continue; }
        if (c == '/' && i + 1 < len && src[i + 1] == '*') {
            i += 2;
            while (i + 1 < len && !(src[i] == '*' && src[i + 1] == '/')) { if (src[i] == '\n') line++; i++; }
            i += 2; continue;
        }
        if (c == 'I' && len - i > 9 && memcmp(src + i, "IMPORT://", 9) == 0) { while (i < len && src[i] != '\n') i++; continue; }
        Token t; memset(&t, 0, sizeof t); t.line = line;
        if (is_digit(c)) {
            i32 v = 0;
            if (c == '0' && i + 1 < len && (src[i + 1] == 'x' || src[i + 1] == 'X')) {
                i += 2;
                while (i < len && ((src[i] >= '0' && src[i] <= '9') || (src[i] >= 'a' && src[i] <= 'f') || (src[i] >= 'A' && src[i] <= 'F') || src[i] == '_')) {
                    if (src[i] != '_') { int d = is_digit(src[i]) ? src[i] - '0' : ((src[i] | 32) - 'a' + 10); v = v * 16 + d; }
                    i++;
                }
            } else {
                while (i < len && (is_digit(src[i]) || src[i] == '_')) { if (src[i] != '_') v = v * 10 + (src[i] - '0'); i++; }
                if (i + 1 < len && src[i] == '.' && is_digit(src[i + 1])) fail(line, "decimal numbers are not supported by the kernel's TON yet", NULL);
            }
            t.k = K_NUM; t.num = v; tok_add(t); continue;
        }
        if (c == '"' || c == '`' || (c == 'f' && i + 1 < len && src[i + 1] == '"')) {
            int fstr = c == 'f';
            if (fstr) i++;
            char q = src[i++];
            Buf b = { 0, 0, 0 };
            buf_add(&b, "", 0);
            int depth = 0;
            while (i < len && (src[i] != q || depth > 0)) {
                if (q == '"' && src[i] == '\\' && i + 1 < len) {
                    char e = src[i + 1]; i += 2;
                    if (fstr) { buf_add(&b, "\\", 1); buf_add(&b, &e, 1); continue; }   // f-strings keep escapes for the second pass
                    char r = e == 'n' ? '\n' : e == 't' ? '\t' : e == 'r' ? '\r' : e;
                    buf_add(&b, &r, 1); continue;
                }
                if (fstr && src[i] == '{') { if (i + 1 < len && src[i + 1] == '{' && depth == 0) { buf_add(&b, "{{", 2); i += 2; continue; } depth++; }
                else if (fstr && src[i] == '}' && depth > 0) depth--;
                if (src[i] == '\n') line++;
                buf_add(&b, &src[i], 1); i++;
            }
            if (i >= len) fail(t.line, "unterminated string", NULL);
            i++;
            t.k = fstr ? K_FSTR : K_STR; t.s = b.d; t.n = b.len; tok_add(t); continue;
        }
        if (is_alpha(c)) {
            u32 s = i;
            while (i < len && (is_alpha(src[i]) || is_digit(src[i]))) i++;
            // "ton.let", "ton.function", "ton.int" ... are keywords; "ton.name" elsewhere is just name
            if (i - s == 3 && memcmp(src + s, "ton", 3) == 0 && i + 1 < len && src[i] == '.' && is_alpha(src[i + 1])) {
                u32 d = ++i;
                while (i < len && (is_alpha(src[i]) || is_digit(src[i]))) i++;
                t.k = K_ID; t.s = intern(src + d, i - d); t.n = i - d;
                const char *w = t.s;
                if (!(strcmp(w, "let") == 0 || strcmp(w, "int") == 0 || strcmp(w, "string") == 0 || strcmp(w, "bool") == 0 ||
                      strcmp(w, "array") == 0 || strcmp(w, "dict") == 0 || strcmp(w, "float") == 0 || strcmp(w, "function") == 0 || strcmp(w, "const") == 0))
                    { /* a plain name after the prefix */ }
                tok_add(t); continue;
            }
            t.k = K_ID; t.s = intern(src + s, i - s); t.n = i - s; tok_add(t); continue;
        }
        // operators
        if (i + 1 < len) {
            u16 two = OP2(c, src[i + 1]);
            if (two == OP2('=', '=') || two == OP2('!', '=') || two == OP2('<', '=') || two == OP2('>', '=') ||
                two == OP2('+', '=') || two == OP2('-', '=') || two == OP2('*', '=') || two == OP2('/', '=') ||
                two == OP2('&', '&') || two == OP2('|', '|') || two == OP2('=', '>')) {
                t.k = K_OP; t.op = two; tok_add(t); i += 2; continue;
            }
        }
        if (c == '(' || c == ')' || c == '[' || c == ']' || c == '{' || c == '}' || c == ',' || c == ':' || c == '.' ||
            c == '+' || c == '-' || c == '*' || c == '/' || c == '%' || c == '<' || c == '>' || c == '=' || c == '!' || c == '?') {
            t.k = K_OP; t.op = (u16)c; tok_add(t); i++; continue;
        }
        { char m[2] = { c, 0 }; fail(line, "unexpected character ", m); }
    }
    Token e; memset(&e, 0, sizeof e); e.k = K_EOF; e.line = line; tok_add(e);
}

// ------------------------------------------------------------------ syntax tree
enum {
    N_NUM, N_STR, N_FSTR, N_BOOL, N_NIL, N_VAR, N_ARR, N_DICT, N_IDX, N_SLICE, N_FIELD, N_CALL, N_NOT, N_NEG, N_BIN, N_AND, N_OR, N_TERN,
    N_LET, N_ASSIGN, N_EXPR, N_IF, N_WHILE, N_FOR, N_BLOCK, N_RET, N_BRK, N_CONT, N_FUNC
};
typedef struct Node {
    u8 kind; u16 op; int line; i32 num;
    const char *name; Str *str;
    struct Node *a, *b, *c;
    struct Node **list; u32 n;
    const char **params; u32 np;
} Node;

static Node *node(int kind, int line) {
    Node *n = kalloc(sizeof(Node));
    if (!n) fail(line, "out of memory", NULL);
    memset(n, 0, sizeof *n);
    n->kind = (u8)kind; n->line = line;
    return n;
}
static void list_add(Node *n, Node *x) {
    Node **nl = kalloc(sizeof(Node *) * (n->n + 1));
    if (!nl) fail(n->line, "out of memory", NULL);
    if (n->n) memcpy(nl, n->list, sizeof(Node *) * n->n);
    nl[n->n++] = x;
    n->list = nl;
}

// ------------------------------------------------------------------ parser
typedef struct Parser { Token *t; u32 pos; } Parser;
static Token *cur(Parser *p) { return &p->t[p->pos]; }
static int is_op(Parser *p, u16 op) { return cur(p)->k == K_OP && cur(p)->op == op; }
static int is_word(Parser *p, const char *w) { return cur(p)->k == K_ID && strcmp(cur(p)->s, w) == 0; }
static int accept_op(Parser *p, u16 op) { if (is_op(p, op)) { p->pos++; return 1; } return 0; }
static int accept_word(Parser *p, const char *w) { if (is_word(p, w)) { p->pos++; return 1; } return 0; }
static void expect_op(Parser *p, u16 op, const char *what) {
    if (!accept_op(p, op)) fail(cur(p)->line, "expected ", what);
}
static Node *parse_expr(Parser *p);
static Node *parse_block(Parser *p);
static Node *parse_stmt(Parser *p);

static Node *parse_fstring(Token *t) {
    Node *n = node(N_FSTR, t->line);
    const char *s = t->s; u32 len = t->n, i = 0;
    Buf lit = { 0, 0, 0 }; buf_add(&lit, "", 0);
    while (i < len) {
        if (s[i] == '\\' && i + 1 < len) {
            char e = s[i + 1]; char r = e == 'n' ? '\n' : e == 't' ? '\t' : e == 'r' ? '\r' : e;
            buf_add(&lit, &r, 1); i += 2; continue;
        }
        if (s[i] == '{' && i + 1 < len && s[i + 1] == '{') { buf_add(&lit, "{", 1); i += 2; continue; }
        if (s[i] == '}' && i + 1 < len && s[i + 1] == '}') { buf_add(&lit, "}", 1); i += 2; continue; }
        if (s[i] == '{') {
            if (lit.len) { Node *l = node(N_STR, t->line); l->str = str_new(lit.d, lit.len); list_add(n, l); lit.len = 0; lit.d[0] = 0; }
            u32 j = i + 1; int depth = 1;
            while (j < len && depth) { if (s[j] == '{') depth++; else if (s[j] == '}') depth--; j++; }
            if (depth) fail(t->line, "missing } in an f-string", NULL);
            // lex the expression between the braces with a nested lexer state
            Token *save = toks; u32 sn = tok_n, sc = tok_cap;
            toks = NULL; tok_n = tok_cap = 0;
            lex(s + i + 1, j - i - 2);
            for (u32 k = 0; k < tok_n; k++) toks[k].line = t->line;
            Parser sub = { toks, 0 };
            Node *e = parse_expr(&sub);
            if (cur(&sub)->k != K_EOF) fail(t->line, "unexpected text in an f-string expression", NULL);
            toks = save; tok_n = sn; tok_cap = sc;
            list_add(n, e);
            i = j; continue;
        }
        buf_add(&lit, &s[i], 1); i++;
    }
    if (lit.len) { Node *l = node(N_STR, t->line); l->str = str_new(lit.d, lit.len); list_add(n, l); }
    return n;
}

static Node *parse_primary(Parser *p) {
    Token *t = cur(p); int line = t->line;
    if (t->k == K_NUM) { p->pos++; Node *n = node(N_NUM, line); n->num = t->num; return n; }
    if (t->k == K_STR) { p->pos++; Node *n = node(N_STR, line); n->str = str_new(t->s, t->n); return n; }
    if (t->k == K_FSTR) { p->pos++; return parse_fstring(t); }
    if (t->k == K_ID) {
        if (accept_word(p, "true")) { Node *n = node(N_BOOL, line); n->num = 1; return n; }
        if (accept_word(p, "false")) { Node *n = node(N_BOOL, line); n->num = 0; return n; }
        if (accept_word(p, "nil")) return node(N_NIL, line);
        p->pos++; Node *n = node(N_VAR, line); n->name = t->s; return n;
    }
    if (accept_op(p, '(')) { Node *e = parse_expr(p); expect_op(p, ')', "')'"); return e; }
    if (accept_op(p, '[')) {
        Node *n = node(N_ARR, line);
        while (!is_op(p, ']')) { list_add(n, parse_expr(p)); if (!accept_op(p, ',')) break; }
        expect_op(p, ']', "']'");
        return n;
    }
    if (accept_op(p, '{')) {
        Node *n = node(N_DICT, line);
        while (!is_op(p, '}')) {
            Node *k;
            if (cur(p)->k == K_ID && p->t[p->pos + 1].k == K_OP && p->t[p->pos + 1].op == ':') {   // {key: value}
                k = node(N_STR, cur(p)->line); k->str = str_new(cur(p)->s, cur(p)->n); p->pos++;
            } else k = parse_expr(p);
            expect_op(p, ':', "':' after a dict key");
            list_add(n, k); list_add(n, parse_expr(p));
            if (!accept_op(p, ',')) break;
        }
        expect_op(p, '}', "'}'");
        return n;
    }
    fail(line, "expected an expression", NULL);
    return NULL;
}
static Node *parse_postfix(Parser *p) {
    Node *e = parse_primary(p);
    for (;;) {
        int line = cur(p)->line;
        if (accept_op(p, '(')) {
            Node *c = node(N_CALL, line); c->a = e;
            while (!is_op(p, ')')) { list_add(c, parse_expr(p)); if (!accept_op(p, ',')) break; }
            expect_op(p, ')', "')'");
            e = c;
        } else if (accept_op(p, '[')) {
            Node *i = node(N_IDX, line); i->a = e;
            if (accept_op(p, ':')) {                       // x[:end]
                i->kind = N_SLICE;
                if (!is_op(p, ']')) i->c = parse_expr(p);
            } else {
                i->b = parse_expr(p);
                if (accept_op(p, ':')) {                   // x[start:end] or x[start:]
                    i->kind = N_SLICE;
                    if (!is_op(p, ']')) i->c = parse_expr(p);
                }
            }
            expect_op(p, ']', "']'");
            e = i;
        } else if (is_op(p, '.') && p->t[p->pos + 1].k == K_ID) {
            p->pos++;
            Node *f = node(N_FIELD, line); f->a = e; f->name = cur(p)->s; p->pos++;
            e = f;
        } else return e;
    }
}
static Node *parse_unary(Parser *p) {
    int line = cur(p)->line;
    if (accept_op(p, '!')) { Node *n = node(N_NOT, line); n->a = parse_unary(p); return n; }
    if (accept_op(p, '-')) { Node *n = node(N_NEG, line); n->a = parse_unary(p); return n; }
    return parse_postfix(p);
}
static Node *bin(Parser *p, Node *l, u16 op, Node *r, int line) {
    Node *n = node(N_BIN, line); n->op = op; n->a = l; n->b = r; (void)p; return n;
}
static Node *parse_mul(Parser *p) {
    Node *l = parse_unary(p);
    for (;;) {
        int line = cur(p)->line;
        if (is_op(p, '*') || is_op(p, '/') || is_op(p, '%')) { u16 op = cur(p)->op; p->pos++; l = bin(p, l, op, parse_unary(p), line); }
        else return l;
    }
}
static Node *parse_add(Parser *p) {
    Node *l = parse_mul(p);
    for (;;) {
        int line = cur(p)->line;
        if (is_op(p, '+') || is_op(p, '-')) { u16 op = cur(p)->op; p->pos++; l = bin(p, l, op, parse_mul(p), line); }
        else return l;
    }
}
static Node *parse_cmp(Parser *p) {
    Node *l = parse_add(p);
    for (;;) {
        int line = cur(p)->line;
        if (is_op(p, '<') || is_op(p, '>') || is_op(p, OP2('<', '=')) || is_op(p, OP2('>', '='))) { u16 op = cur(p)->op; p->pos++; l = bin(p, l, op, parse_add(p), line); }
        else return l;
    }
}
static Node *parse_eq(Parser *p) {
    Node *l = parse_cmp(p);
    for (;;) {
        int line = cur(p)->line;
        if (is_op(p, OP2('=', '=')) || is_op(p, OP2('!', '='))) { u16 op = cur(p)->op; p->pos++; l = bin(p, l, op, parse_cmp(p), line); }
        else return l;
    }
}
static Node *parse_and(Parser *p) {
    Node *l = parse_eq(p);
    while (is_word(p, "and") || is_op(p, OP2('&', '&'))) {
        int line = cur(p)->line; p->pos++;
        Node *n = node(N_AND, line); n->a = l; n->b = parse_eq(p); l = n;
    }
    return l;
}
static Node *parse_or(Parser *p) {
    Node *l = parse_and(p);
    while (is_word(p, "or") || is_op(p, OP2('|', '|'))) {
        int line = cur(p)->line; p->pos++;
        Node *n = node(N_OR, line); n->a = l; n->b = parse_and(p); l = n;
    }
    return l;
}
static Node *parse_expr(Parser *p) {
    Node *c = parse_or(p);
    if (is_op(p, '?')) {
        int line = cur(p)->line; p->pos++;
        Node *n = node(N_TERN, line); n->a = c; n->b = parse_expr(p);
        expect_op(p, ':', "':' in a ?: expression");
        n->c = parse_expr(p);
        return n;
    }
    return c;
}

static Node *parse_block(Parser *p) {
    Node *b = node(N_BLOCK, cur(p)->line);
    expect_op(p, '{', "'{'");
    while (!is_op(p, '}')) {
        if (cur(p)->k == K_EOF) fail(cur(p)->line, "missing '}'", NULL);
        list_add(b, parse_stmt(p));
    }
    p->pos++;
    return b;
}
static Node *parse_func(Parser *p, int line) {
    Node *f = node(N_FUNC, line);
    if (cur(p)->k != K_ID) fail(line, "expected a function name", NULL);
    f->name = cur(p)->s; p->pos++;
    expect_op(p, '(', "'(' after the function name");
    while (!is_op(p, ')')) {
        if (cur(p)->k != K_ID) fail(cur(p)->line, "expected a parameter name", NULL);
        const char **np = kalloc(sizeof(char *) * (f->np + 1));
        if (!np) fail(line, "out of memory", NULL);
        if (f->np) memcpy(np, f->params, sizeof(char *) * f->np);
        np[f->np++] = cur(p)->s; f->params = np; p->pos++;
        if (!accept_op(p, ',')) break;
    }
    expect_op(p, ')', "')'");
    f->a = parse_block(p);
    return f;
}
static Node *parse_stmt(Parser *p) {
    Token *t = cur(p); int line = t->line;
    if (t->k == K_ID) {
        const char *w = t->s;
        if (strcmp(w, "let") == 0 || strcmp(w, "int") == 0 || strcmp(w, "string") == 0 || strcmp(w, "bool") == 0 ||
            strcmp(w, "array") == 0 || strcmp(w, "dict") == 0 || strcmp(w, "float") == 0 || strcmp(w, "const") == 0) {
            if (p->t[p->pos + 1].k == K_ID) {
                p->pos++;
                Node *n = node(N_LET, line); n->name = cur(p)->s; p->pos++;
                expect_op(p, '=', "'=' in a declaration");
                n->a = parse_expr(p);
                return n;
            }
        }
        if (strcmp(w, "function") == 0 && p->t[p->pos + 1].k == K_ID) { p->pos++; return parse_func(p, line); }
        if (accept_word(p, "if")) {
            Node *n = node(N_IF, line);
            n->a = parse_expr(p); n->b = parse_block(p);
            if (accept_word(p, "else")) {
                if (is_word(p, "if")) { Node *e = node(N_BLOCK, line); list_add(e, parse_stmt(p)); n->c = e; }
                else n->c = parse_block(p);
            }
            return n;
        }
        if (accept_word(p, "while")) { Node *n = node(N_WHILE, line); n->a = parse_expr(p); n->b = parse_block(p); return n; }
        if (accept_word(p, "for")) {
            Node *n = node(N_FOR, line);
            if (cur(p)->k != K_ID) fail(line, "expected a loop variable", NULL);
            n->name = cur(p)->s; p->pos++;
            if (!accept_word(p, "in")) fail(line, "expected 'in' in a for loop", NULL);
            n->a = parse_expr(p); n->b = parse_block(p);
            return n;
        }
        if (accept_word(p, "return")) {
            Node *n = node(N_RET, line);
            if (!is_op(p, '}') && cur(p)->k != K_EOF) n->a = parse_expr(p);
            return n;
        }
        if (accept_word(p, "break")) return node(N_BRK, line);
        if (accept_word(p, "continue")) return node(N_CONT, line);
    }
    Node *e = parse_expr(p);
    u16 op = 0;
    if (is_op(p, '=')) op = '=';
    else if (is_op(p, OP2('+', '='))) op = '+';
    else if (is_op(p, OP2('-', '='))) op = '-';
    else if (is_op(p, OP2('*', '='))) op = '*';
    else if (is_op(p, OP2('/', '='))) op = '/';
    if (op) {
        if (e->kind != N_VAR && e->kind != N_IDX && e->kind != N_FIELD) fail(line, "can't assign to this", NULL);
        p->pos++;
        Node *n = node(N_ASSIGN, line); n->op = op; n->a = e; n->b = parse_expr(p);
        return n;
    }
    Node *n = node(N_EXPR, line); n->a = e;
    return n;
}

// ------------------------------------------------------------------ evaluation
typedef struct Frame { u32 n, cap; const char **name; Value *val; } Frame;
static Frame globals_f;
static Frame *locals;      // the running function's variables, or NULL at the top level
static int call_depth;
static Value ret_val;
enum { SIG_NONE, SIG_BREAK, SIG_CONT, SIG_RET };

static int frame_find(Frame *f, const char *name) {
    for (int i = (int)f->n - 1; i >= 0; i--) if (f->name[i] == name) return i;
    return -1;
}
static void frame_set(Frame *f, const char *name, Value v) {
    int i = frame_find(f, name);
    if (i >= 0) { f->val[i] = v; return; }
    if (f->n == f->cap) {
        u32 nc = f->cap ? f->cap * 2 : 8;
        const char **nn = kalloc(sizeof(char *) * nc);
        Value *nv = kalloc(sizeof(Value) * nc);
        if (!nn || !nv) fail(0, "out of memory", NULL);
        if (f->n) { memcpy(nn, f->name, sizeof(char *) * f->n); memcpy(nv, f->val, sizeof(Value) * f->n); }
        f->name = nn; f->val = nv; f->cap = nc;
    }
    f->name[f->n] = name; f->val[f->n] = v; f->n++;
}
static Value *var_ref(const char *name) {
    if (locals) { int i = frame_find(locals, name); if (i >= 0) return &locals->val[i]; }
    int i = frame_find(&globals_f, name);
    return i >= 0 ? &globals_f.val[i] : NULL;
}

static Value eval(Node *n);
static int exec(Node *n);
static Value call_native(const char *name, Value *args, int argc, int line, int *found);

static i32 need_int(Value v, int line, const char *what) {
    if (v.t != V_INT) fail(line, what, " must be an integer");
    return v.u.i;
}
static Value concat(Value a, Value b) {
    Buf buf = { 0, 0, 0 }; buf_add(&buf, "", 0);
    val_text(&buf, a, 0); val_text(&buf, b, 0);
    return mk_str(buf.d, buf.len);
}

static Value call_func(Node *f, Value *args, int argc, int line) {
    if (call_depth > 40) fail(line, "too many nested calls", NULL);
    if ((u32)argc != f->np) fail(line, "wrong number of arguments for ", f->name);
    Frame fr = { 0, 0, NULL, NULL };
    for (int i = 0; i < argc; i++) frame_set(&fr, f->params[i], args[i]);
    Frame *saved = locals; locals = &fr; call_depth++;
    ret_val = NIL;
    exec(f->a);
    call_depth--; locals = saved;
    Value r = ret_val; ret_val = NIL;
    return r;
}

static Value eval_call(Node *n) {
    Value args[12]; int argc = 0;
    Node *callee = n->a;
    Value self = NIL; int has_self = 0;
    Value fv = NIL; int have_fv = 0;
    if (callee->kind == N_FIELD) {
        self = eval(callee->a); has_self = 1;
        if (self.t == V_DICT) {
            Str *k = str_new(callee->name, (u32)strlen(callee->name));
            int i = dict_find(self.u.m, k);
            if (i >= 0 && self.u.m->v[i].t == V_FUNC) { fv = self.u.m->v[i]; have_fv = 1; has_self = 0; }
        }
    } else if (callee->kind == N_VAR) {
        Value *r = var_ref(callee->name);
        if (r && r->t == V_FUNC) { fv = *r; have_fv = 1; }
    } else { fv = eval(callee); have_fv = fv.t == V_FUNC; if (!have_fv) fail(n->line, "this can't be called", NULL); }
    if (has_self) args[argc++] = self;
    for (u32 i = 0; i < n->n; i++) {
        if (argc >= 12) fail(n->line, "too many arguments", NULL);
        args[argc++] = eval(n->list[i]);
    }
    if (have_fv) return call_func(fv.u.f, args, argc, n->line);
    const char *name = callee->kind == N_FIELD ? callee->name : callee->name;
    int found = 0;
    Value r = call_native(name, args, argc, n->line, &found);
    if (!found) fail(n->line, "unknown function ", name);
    return r;
}

static Value eval(Node *n) {
    switch (n->kind) {
        case N_NUM: return mk_int(n->num);
        case N_STR: { Value v; v.t = V_STR; v.u.s = n->str; return v; }
        case N_BOOL: return mk_bool((int)n->num);
        case N_NIL: return NIL;
        case N_FSTR: {
            Buf b = { 0, 0, 0 }; buf_add(&b, "", 0);
            for (u32 i = 0; i < n->n; i++) {
                if (n->list[i]->kind == N_STR) buf_add(&b, n->list[i]->str->d, n->list[i]->str->len);
                else val_text(&b, eval(n->list[i]), 0);
            }
            return mk_str(b.d, b.len);
        }
        case N_VAR: {
            Value *r = var_ref(n->name);
            if (r) return *r;
            if (strcmp(n->name, "pi") == 0) return mk_int(3);
            fail(n->line, "undefined variable or function ", n->name);
            return NIL;
        }
        case N_ARR: {
            Value a = mk_arr(n->n);
            for (u32 i = 0; i < n->n; i++) arr_push(a.u.a, eval(n->list[i]));
            return a;
        }
        case N_DICT: {
            Value d = mk_dict();
            for (u32 i = 0; i + 1 < n->n; i += 2) {
                Value k = eval(n->list[i]);
                if (k.t != V_STR) fail(n->line, "dict keys must be strings", NULL);
                dict_set(d.u.m, k.u.s, eval(n->list[i + 1]));
            }
            return d;
        }
        case N_IDX: {
            Value o = eval(n->a), k = eval(n->b);
            if (o.t == V_ARR) {
                i32 i = need_int(k, n->line, "an array index");
                if (i < 0) i += (i32)o.u.a->len;
                if (i < 0 || (u32)i >= o.u.a->len) fail(n->line, "array index out of range", NULL);
                return o.u.a->v[i];
            }
            if (o.t == V_STR) {
                i32 i = need_int(k, n->line, "a string index");
                if (i < 0) i += (i32)o.u.s->len;
                if (i < 0 || (u32)i >= o.u.s->len) fail(n->line, "string index out of range", NULL);
                return mk_str(o.u.s->d + i, 1);
            }
            if (o.t == V_DICT) {
                if (k.t != V_STR) fail(n->line, "dict keys must be strings", NULL);
                int i = dict_find(o.u.m, k.u.s);
                return i >= 0 ? o.u.m->v[i] : NIL;
            }
            fail(n->line, "can't index a ", type_name(o));
            return NIL;
        }
        case N_SLICE: {
            Value o = eval(n->a);
            if (o.t != V_ARR && o.t != V_STR) fail(n->line, "can't slice a ", type_name(o));
            i32 size = o.t == V_ARR ? (i32)o.u.a->len : (i32)o.u.s->len;
            i32 a = n->b ? need_int(eval(n->b), n->line, "a slice start") : 0;
            i32 b = n->c ? need_int(eval(n->c), n->line, "a slice end") : size;
            if (a < 0) a += size;
            if (b < 0) b += size;
            if (a < 0) a = 0;
            if (b > size) b = size;
            if (b < a) b = a;
            if (o.t == V_STR) return mk_str(o.u.s->d + a, (u32)(b - a));
            Value r = mk_arr((u32)(b - a));
            for (i32 i = a; i < b; i++) arr_push(r.u.a, o.u.a->v[i]);
            return r;
        }
        case N_FIELD: {
            Value o = eval(n->a);
            if (o.t == V_DICT) {
                Str *k = str_new(n->name, (u32)strlen(n->name));
                int i = dict_find(o.u.m, k);
                return i >= 0 ? o.u.m->v[i] : NIL;
            }
            if (strcmp(n->name, "length") == 0 || strcmp(n->name, "len") == 0) {
                if (o.t == V_ARR) return mk_int((i32)o.u.a->len);
                if (o.t == V_STR) return mk_int((i32)o.u.s->len);
            }
            fail(n->line, "no field ", n->name);
            return NIL;
        }
        case N_CALL: return eval_call(n);
        case N_NOT: return mk_bool(!truthy(eval(n->a)));
        case N_NEG: return mk_int(-need_int(eval(n->a), n->line, "the operand of -"));
        case N_AND: { Value l = eval(n->a); return truthy(l) ? eval(n->b) : l; }
        case N_OR: { Value l = eval(n->a); return truthy(l) ? l : eval(n->b); }
        case N_TERN: return truthy(eval(n->a)) ? eval(n->b) : eval(n->c);
        case N_BIN: {
            Value l = eval(n->a), r = eval(n->b);
            switch (n->op) {
                case OP2('=', '='): return mk_bool(val_eq(l, r));
                case OP2('!', '='): return mk_bool(!val_eq(l, r));
                case '+':
                    if (l.t == V_INT && r.t == V_INT) return mk_int(l.u.i + r.u.i);
                    if (l.t == V_STR || r.t == V_STR) return concat(l, r);
                    if (l.t == V_ARR && r.t == V_ARR) { Value a = mk_arr(l.u.a->len + r.u.a->len); for (u32 i = 0; i < l.u.a->len; i++) arr_push(a.u.a, l.u.a->v[i]); for (u32 i = 0; i < r.u.a->len; i++) arr_push(a.u.a, r.u.a->v[i]); return a; }
                    fail(n->line, "can't add a ", type_name(l)); break;
                case '*':
                    if (l.t == V_STR && r.t == V_INT) { Buf b = { 0, 0, 0 }; buf_add(&b, "", 0); for (i32 i = 0; i < r.u.i; i++) buf_add(&b, l.u.s->d, l.u.s->len); return mk_str(b.d, b.len); }
                    break;
                case '<': case '>': case OP2('<', '='): case OP2('>', '='): {
                    if (l.t == V_STR && r.t == V_STR) {
                        int c = strcmp(l.u.s->d, r.u.s->d);
                        return mk_bool(n->op == '<' ? c < 0 : n->op == '>' ? c > 0 : n->op == OP2('<', '=') ? c <= 0 : c >= 0);
                    }
                    i32 a = need_int(l, n->line, "a compared value"), b = need_int(r, n->line, "a compared value");
                    return mk_bool(n->op == '<' ? a < b : n->op == '>' ? a > b : n->op == OP2('<', '=') ? a <= b : a >= b);
                }
                default: break;
            }
            i32 a = need_int(l, n->line, "an arithmetic operand"), b = need_int(r, n->line, "an arithmetic operand");
            switch (n->op) {
                case '-': return mk_int(a - b);
                case '*': return mk_int(a * b);
                case '/': if (b == 0) fail(n->line, "division by zero", NULL); return mk_int(a / b);
                case '%': if (b == 0) fail(n->line, "modulo by zero", NULL); return mk_int(a % b);
                default: return NIL;
            }
        }
        default: fail(n->line, "can't evaluate this", NULL); return NIL;
    }
}

static void assign(Node *target, Value v, int line) {
    if (target->kind == N_VAR) {
        Value *r = var_ref(target->name);
        if (!r) fail(line, "can't assign to '", target->name), (void)0;
        *r = v;
    } else if (target->kind == N_IDX) {
        Value o = eval(target->a), k = eval(target->b);
        if (o.t == V_ARR) {
            i32 i = need_int(k, line, "an array index");
            if (i < 0) i += (i32)o.u.a->len;
            if (i < 0 || (u32)i >= o.u.a->len) fail(line, "array index out of range", NULL);
            o.u.a->v[i] = v;
        } else if (o.t == V_DICT && k.t == V_STR) dict_set(o.u.m, k.u.s, v);
        else fail(line, "can't assign into a ", type_name(o));
    } else {
        Value o = eval(target->a);
        if (o.t != V_DICT) fail(line, "can't set a field on a ", type_name(o));
        dict_set(o.u.m, str_new(target->name, (u32)strlen(target->name)), v);
    }
}

static int exec(Node *n) {
    switch (n->kind) {
        case N_BLOCK:
            for (u32 i = 0; i < n->n; i++) { int s = exec(n->list[i]); if (s) return s; }
            return SIG_NONE;
        case N_LET: { Value v = eval(n->a); frame_set(locals ? locals : &globals_f, n->name, v); return SIG_NONE; }
        case N_FUNC: { Value f; f.t = V_FUNC; f.u.f = n; frame_set(locals ? locals : &globals_f, n->name, f); return SIG_NONE; }
        case N_ASSIGN: {
            Value v = eval(n->b);
            if (n->op != '=') {
                Value cur_v = eval(n->a);
                Node tmp; memset(&tmp, 0, sizeof tmp);
                Node l = *n->a; (void)l;
                if (cur_v.t == V_STR || v.t == V_STR) v = concat(cur_v, v);
                else {
                    i32 a = need_int(cur_v, n->line, "the left side"), b = need_int(v, n->line, "the right side");
                    v = mk_int(n->op == '+' ? a + b : n->op == '-' ? a - b : n->op == '*' ? a * b : (b ? a / b : 0));
                }
            }
            assign(n->a, v, n->line);
            return SIG_NONE;
        }
        case N_EXPR: eval(n->a); return SIG_NONE;
        case N_IF:
            if (truthy(eval(n->a))) return exec(n->b);
            return n->c ? exec(n->c) : SIG_NONE;
        case N_WHILE:
            while (truthy(eval(n->a))) {
                int s = exec(n->b);
                if (s == SIG_BREAK) break;
                if (s == SIG_RET) return s;
            }
            return SIG_NONE;
        case N_FOR: {
            Value it = eval(n->a);
            u32 count = it.t == V_ARR ? it.u.a->len : it.t == V_STR ? it.u.s->len : it.t == V_DICT ? it.u.m->len : 0;
            if (it.t != V_ARR && it.t != V_STR && it.t != V_DICT) fail(n->line, "can't loop over a ", type_name(it));
            for (u32 i = 0; i < count; i++) {
                Value x = it.t == V_ARR ? it.u.a->v[i] : it.t == V_STR ? mk_str(it.u.s->d + i, 1) : (Value){ V_STR, { .s = it.u.m->k[i] } };
                frame_set(locals ? locals : &globals_f, n->name, x);
                int s = exec(n->b);
                if (s == SIG_BREAK) break;
                if (s == SIG_RET) return s;
            }
            return SIG_NONE;
        }
        case N_RET: ret_val = n->a ? eval(n->a) : NIL; return SIG_RET;
        case N_BRK: return SIG_BREAK;
        case N_CONT: return SIG_CONT;
        default: fail(n->line, "can't run this", NULL); return SIG_NONE;
    }
}

// ------------------------------------------------------------------ the functions every program has
static u32 rng = 2463534242u;
static i32 next_rand(void) { rng ^= rng << 13; rng ^= rng >> 17; rng ^= rng << 5; return (i32)(rng & 0x7FFFFFFF); }

static const Str *need_str(Value v, int line, const char *what) {
    if (v.t != V_STR) fail(line, what, " must be a string");
    return v.u.s;
}
#define ARGC(n) do { if (argc < (n)) fail(line, name, " needs more arguments"); } while (0)

static Value call_native(const char *name, Value *args, int argc, int line, int *found) {
    *found = 1;
    if (strcmp(name, "print") == 0) {
        Buf b = { 0, 0, 0 }; buf_add(&b, "", 0);
        for (int i = 0; i < argc; i++) { if (i) buf_add(&b, " ", 1); val_text(&b, args[i], 0); }
        console_puts(b.d); console_putc('\n');
        return NIL;
    }
    if (strcmp(name, "len") == 0) {
        ARGC(1);
        if (args[0].t == V_STR) return mk_int((i32)args[0].u.s->len);
        if (args[0].t == V_ARR) return mk_int((i32)args[0].u.a->len);
        if (args[0].t == V_DICT) return mk_int((i32)args[0].u.m->len);
        fail(line, "len() needs a string, array or dict", NULL);
    }
    if (strcmp(name, "str") == 0) { ARGC(1); Buf b = { 0, 0, 0 }; buf_add(&b, "", 0); val_text(&b, args[0], 0); return mk_str(b.d, b.len); }
    if (strcmp(name, "type") == 0) { ARGC(1); const char *t = type_name(args[0]); return mk_str(t, (u32)strlen(t)); }
    if (strcmp(name, "num") == 0) {
        ARGC(1);
        if (args[0].t == V_INT) return args[0];
        const Str *s = need_str(args[0], line, "num()'s argument");
        i32 v = 0; u32 i = 0; int neg = 0;
        while (i < s->len && s->d[i] == ' ') i++;
        if (i < s->len && s->d[i] == '-') { neg = 1; i++; }
        while (i < s->len && is_digit(s->d[i])) v = v * 10 + (s->d[i++] - '0');
        return mk_int(neg ? -v : v);
    }
    if (strcmp(name, "push") == 0) { ARGC(2); if (args[0].t != V_ARR) fail(line, "push() needs an array", NULL); arr_push(args[0].u.a, args[1]); return args[0]; }
    if (strcmp(name, "pop") == 0) { ARGC(1); if (args[0].t != V_ARR || !args[0].u.a->len) return NIL; return args[0].u.a->v[--args[0].u.a->len]; }
    if (strcmp(name, "keys") == 0) {
        ARGC(1); if (args[0].t != V_DICT) fail(line, "keys() needs a dict", NULL);
        Value a = mk_arr(args[0].u.m->len);
        for (u32 i = 0; i < args[0].u.m->len; i++) { Value k; k.t = V_STR; k.u.s = args[0].u.m->k[i]; arr_push(a.u.a, k); }
        return a;
    }
    if (strcmp(name, "has") == 0) {
        ARGC(2);
        if (args[0].t == V_DICT && args[1].t == V_STR) return mk_bool(dict_find(args[0].u.m, args[1].u.s) >= 0);
        if (args[0].t == V_ARR) { for (u32 i = 0; i < args[0].u.a->len; i++) if (val_eq(args[0].u.a->v[i], args[1])) return mk_bool(1); return mk_bool(0); }
        return mk_bool(0);
    }
    if (strcmp(name, "remove") == 0) {
        ARGC(2);
        if (args[0].t == V_DICT && args[1].t == V_STR) { dict_remove(args[0].u.m, args[1].u.s); return args[0]; }
        if (args[0].t == V_ARR) {
            Arr *a = args[0].u.a;
            for (u32 i = 0; i < a->len; i++) if (val_eq(a->v[i], args[1])) { for (u32 j = i; j + 1 < a->len; j++) a->v[j] = a->v[j + 1]; a->len--; break; }
            return args[0];
        }
        fail(line, "remove() needs an array or a dict", NULL);
    }
    if (strcmp(name, "range") == 0) {
        ARGC(1);
        i32 a = 0, b = need_int(args[0], line, "range()'s limit");
        if (argc >= 2) { a = b; b = need_int(args[1], line, "range()'s limit"); }
        Value r = mk_arr(b > a ? (u32)(b - a) : 1);
        for (i32 i = a; i < b; i++) arr_push(r.u.a, mk_int(i));
        return r;
    }
    if (strcmp(name, "upper") == 0 || strcmp(name, "lower") == 0) {
        ARGC(1); const Str *s = need_str(args[0], line, "the argument");
        Value r = mk_str(s->d, s->len);
        for (u32 i = 0; i < s->len; i++) {
            char c = r.u.s->d[i];
            if (name[0] == 'u' && c >= 'a' && c <= 'z') r.u.s->d[i] = (char)(c - 32);
            if (name[0] == 'l' && c >= 'A' && c <= 'Z') r.u.s->d[i] = (char)(c + 32);
        }
        return r;
    }
    if (strcmp(name, "substring") == 0) {
        ARGC(2); const Str *s = need_str(args[0], line, "the first argument");
        i32 a = need_int(args[1], line, "the start"), n = argc >= 3 ? need_int(args[2], line, "the length") : (i32)s->len - a;
        if (a < 0) a = 0;
        if ((u32)a > s->len) a = (i32)s->len;
        if (n < 0 || (u32)(a + n) > s->len) n = (i32)s->len - a;
        return mk_str(s->d + a, (u32)n);
    }
    if (strcmp(name, "contains") == 0) {
        ARGC(2);
        if (args[0].t == V_STR && args[1].t == V_STR) {
            const Str *h = args[0].u.s, *nd = args[1].u.s;
            for (u32 i = 0; i + nd->len <= h->len; i++) if (memcmp(h->d + i, nd->d, nd->len) == 0) return mk_bool(1);
            return mk_bool(0);
        }
        int f2; return call_native("has", args, argc, line, &f2);
    }
    if (strcmp(name, "join") == 0) {
        ARGC(2);
        if (args[0].t != V_ARR) fail(line, "join() needs an array", NULL);
        Buf b = { 0, 0, 0 }; buf_add(&b, "", 0);
        for (u32 i = 0; i < args[0].u.a->len; i++) { if (i) val_text(&b, args[1], 0); val_text(&b, args[0].u.a->v[i], 0); }
        return mk_str(b.d, b.len);
    }
    if (strcmp(name, "split") == 0) {
        ARGC(2);
        const Str *s = need_str(args[0], line, "the first argument"), *sep = need_str(args[1], line, "the separator");
        Value r = mk_arr(4); u32 start = 0;
        if (sep->len == 0) { for (u32 i = 0; i < s->len; i++) arr_push(r.u.a, mk_str(s->d + i, 1)); return r; }
        for (u32 i = 0; i + sep->len <= s->len; ) {
            if (memcmp(s->d + i, sep->d, sep->len) == 0) { arr_push(r.u.a, mk_str(s->d + start, i - start)); i += sep->len; start = i; }
            else i++;
        }
        arr_push(r.u.a, mk_str(s->d + start, s->len - start));
        return r;
    }
    if (strcmp(name, "replace") == 0) {
        ARGC(3);
        const Str *s = need_str(args[0], line, "the first argument"), *a = need_str(args[1], line, "the text to find"), *b = need_str(args[2], line, "the replacement");
        Buf o = { 0, 0, 0 }; buf_add(&o, "", 0);
        for (u32 i = 0; i < s->len; ) {
            if (a->len && i + a->len <= s->len && memcmp(s->d + i, a->d, a->len) == 0) { buf_add(&o, b->d, b->len); i += a->len; }
            else { buf_add(&o, s->d + i, 1); i++; }
        }
        return mk_str(o.d, o.len);
    }
    if (strcmp(name, "trim") == 0) {
        ARGC(1); const Str *s = need_str(args[0], line, "the argument");
        u32 a = 0, b = s->len;
        while (a < b && (s->d[a] == ' ' || s->d[a] == '\n' || s->d[a] == '\t')) a++;
        while (b > a && (s->d[b - 1] == ' ' || s->d[b - 1] == '\n' || s->d[b - 1] == '\t')) b--;
        return mk_str(s->d + a, b - a);
    }
    if (strcmp(name, "abs") == 0) { ARGC(1); i32 v = need_int(args[0], line, "abs()'s argument"); return mk_int(v < 0 ? -v : v); }
    if (strcmp(name, "min") == 0 || strcmp(name, "max") == 0) {
        ARGC(2); i32 a = need_int(args[0], line, "the first argument"), b = need_int(args[1], line, "the second argument");
        return mk_int(name[1] == 'i' ? (a < b ? a : b) : (a > b ? a : b));
    }
    if (strcmp(name, "rand") == 0) { ARGC(1); i32 m = need_int(args[0], line, "rand()'s limit"); return mk_int(m > 0 ? next_rand() % m : 0); }
    if (strcmp(name, "char") == 0) { ARGC(1); char c = (char)need_int(args[0], line, "char()'s argument"); return mk_str(&c, 1); }
    if (strcmp(name, "ord") == 0) { ARGC(1); const Str *s = need_str(args[0], line, "ord()'s argument"); return mk_int(s->len ? (u8)s->d[0] : 0); }
    // ---- the machine
    if (strcmp(name, "sleep") == 0) { ARGC(1); sleep_ms((u32)need_int(args[0], line, "the delay")); return NIL; }
    if (strcmp(name, "platform") == 0) return mk_str("kernel", 6);
    if (strcmp(name, "uptime") == 0) return mk_int((i32)uptime_ms());
    if (strcmp(name, "clear") == 0) { console_clear(); return NIL; }
    if (strcmp(name, "width") == 0) return mk_int(VGA_W);
    if (strcmp(name, "height") == 0) return mk_int(VGA_H);
    if (strcmp(name, "color") == 0) { ARGC(2); return mk_int((need_int(args[1], line, "the background") & 15) * 16 + (need_int(args[0], line, "the foreground") & 15)); }
    if (strcmp(name, "put") == 0) {
        ARGC(3);
        Buf b = { 0, 0, 0 }; buf_add(&b, "", 0); val_text(&b, args[2], 0);
        console_put_at(need_int(args[0], line, "x"), need_int(args[1], line, "y"), b.d, argc >= 4 ? (u8)need_int(args[3], line, "the color") : 0x07);
        return NIL;
    }
    if (strcmp(name, "fill") == 0) {
        ARGC(5);
        const Str *c = need_str(args[4], line, "the fill character");
        console_fill(need_int(args[0], line, "x"), need_int(args[1], line, "y"), need_int(args[2], line, "the width"), need_int(args[3], line, "the height"),
                     c->len ? c->d[0] : ' ', argc >= 6 ? (u8)need_int(args[5], line, "the color") : 0x07);
        return NIL;
    }
    if (strcmp(name, "cursor") == 0) { ARGC(2); console_cursor(need_int(args[0], line, "x"), need_int(args[1], line, "y")); return NIL; }
    if (strcmp(name, "key") == 0) {     // waits for a key; arrows come back as "up", "down", "left", "right"
        int c = input_wait();
        if (c == KEY_UP) return mk_str("up", 2);
        if (c == KEY_DOWN) return mk_str("down", 4);
        if (c == KEY_LEFT) return mk_str("left", 4);
        if (c == KEY_RIGHT) return mk_str("right", 5);
        if (c == '\n') return mk_str("enter", 5);
        if (c == 8) return mk_str("backspace", 9);
        if (c == KEY_ESC) return mk_str("esc", 3);
        if (c == '\t') return mk_str("tab", 3);
        char ch = (char)c; return mk_str(&ch, 1);
    }
    if (strcmp(name, "poll") == 0) {    // like key(), but returns "" at once when nothing was pressed
        if (input_poll() < 0) return mk_str("", 0);
        return mk_str("", 0);
    }
    if (strcmp(name, "input") == 0) {
        if (argc >= 1) { Buf b = { 0, 0, 0 }; buf_add(&b, "", 0); val_text(&b, args[0], 0); console_puts(b.d); }
        char line_buf[200]; int n = input_readline(line_buf, sizeof line_buf);
        return mk_str(line_buf, (u32)n);
    }
    if (strcmp(name, "reboot") == 0) { reboot(); return NIL; }
    if (strcmp(name, "poweroff") == 0) { power_off(); return NIL; }
    *found = 0;
    return NIL;
}

// ------------------------------------------------------------------ run a program
int ton_run(const char *source, size_t len) {
    size_t mark = kheap_mark();
    names = NULL; name_n = name_cap = 0;
    toks = NULL; tok_n = tok_cap = 0;
    globals_f.n = globals_f.cap = 0; globals_f.name = NULL; globals_f.val = NULL;
    locals = NULL; call_depth = 0; vm_failed = 0; ret_val = NIL;
    if (__builtin_setjmp(err_jmp) == 0) {
        lex(source, (u32)len);
        Parser p = { toks, 0 };
        Node *prog = node(N_BLOCK, 1);
        while (cur(&p)->k != K_EOF) list_add(prog, parse_stmt(&p));
        // functions can be called before their definition: define them first
        for (u32 i = 0; i < prog->n; i++) if (prog->list[i]->kind == N_FUNC) exec(prog->list[i]);
        exec(prog);
    }
    int failed = vm_failed;
    kheap_reset(mark);
    return failed;
}
