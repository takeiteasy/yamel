#ifndef MEL_HEADER
#define MEL_HEADER
#ifdef __cplusplus
extern "C" {
#endif
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdio.h>
#include <wchar.h>
#include <locale.h>

typedef enum mel_return_status {
    MEL_OK,
    MEL_PARSER_ERROR
} mel_return_status_t;

typedef enum mel_token_type {
    MEL_TOKEN_ERROR,
    MEL_TOKEN_EOF,
    MEL_TOKEN_LPAREN,
    MEL_TOKEN_RPAREN,
    MEL_TOKEN_ATOM,
    MEL_TOKEN_NUMBER,
    MEL_TOKEN_STRING
} mel_token_type;

typedef struct mel_token {
    mel_token_type type;
    const wchar_t *cursor;
    int length;
} mel_token_t;

typedef struct mel_parser {
    const wchar_t *source;
    const wchar_t *cursor;
} mel_parser_t;

typedef struct mel {
    
} mel_t;

void mel_init(mel_t *mel);

mel_return_status_t mel_parse(mel_t *mel, const unsigned char *str, int str_length, mel_token_t **tokens);

#ifdef __cplusplus
}
#endif
#endif // MEL_HEADER

#ifdef MEL_IMPLEMENTATION

#define __garry_raw(a)           ((int*)(void*)(a)-2)
#define __garry_m(a)             __garry_raw(a)[0]
#define __garry_n(a)             __garry_raw(a)[1]
#define __garry_needgrow(a,n)    ((a)==0 || __garry_n(a)+(n) >= __garry_m(a))
#define __garry_maybegrow(a,n)   (__garry_needgrow(a,(n)) ? __garry_grow(a,n) : 0)
#define __garry_grow(a,n)        (*((void **)&(a)) = __garry_growf((a), (n), sizeof(*(a))))
#define __garry_needshrink(a)    (__garry_m(a) > 4 && __garry_n(a) <= __garry_m(a) / 4)
#define __garry_maybeshrink(a)   (__garry_needshrink(a) ? __garry_shrink(a) : 0)
#define __garry_shrink(a)        (*((void **)&(a)) = __garry_shrinkf((a), sizeof(*(a))))
#define garry_free(a)           ((a) ? free(__garry_raw(a)),((a)=NULL) : 0)
#define garry_append(a,v)       (__garry_maybegrow(a,1), (a)[__garry_n(a)++] = (v))
#define garry_count(a)          ((a) ? __garry_n(a) : 0)

static void *__garry_growf(void *arr, int increment, int itemsize) {
    int dbl_cur = arr ? 2 * __garry_m(arr) : 0;
    int min_needed = garry_count(arr) + increment;
    int m = dbl_cur > min_needed ? dbl_cur : min_needed;
    int *p = realloc(arr ? __garry_raw(arr) : 0, itemsize * m + sizeof(int) * 2);
    if (p) {
        if (!arr)
            p[1] = 0;
        p[0] = m;
        return p + 2;
    }
    return NULL;
}

static void *__garry_shrinkf(void *arr, int itemsize) {
    int new_capacity = __garry_m(arr) / 2;
    int *p = realloc(arr ? __garry_raw(arr) : 0, itemsize * new_capacity + sizeof(int) * 2);
    if (p) {
        p[0] = new_capacity;
        return p + 2;
    }
    return NULL;
}

#ifndef _WIN32
static int _vscprintf(const char *format, va_list args) {
    va_list copy;
    va_copy(copy, args);
    int retval = vsnprintf(NULL, 0, format, copy);
    va_end(copy);
    return retval;
}
#endif

static char* format(const char *fmt, size_t *size, ...) {
    va_list args;
    va_start(args, size);
    size_t _size = _vscprintf(fmt, args);
    char *result = malloc(_size + 1 * sizeof(char));
    if (!result)
        return NULL;
    vsnprintf(result, _size, fmt, args);
    result[_size] = '\0';
    va_end(args);
    if (size)
        *size = _size;
    return result;
}

static int read_wide(const unsigned char* str, wchar_t* char_out) {
    wchar_t u = *str, l = 1;
    if ((u & 0xC0) == 0xC0) {
        int a = (u & 0x20) ? ((u & 0x10) ? ((u & 0x08) ? ((u & 0x04) ? 6 : 5) : 4) : 3) : 2;
        if (a < 6 || !(u & 0x02)) {
            u = ((u << (a + 1)) & 0xFF) >> (a + 1);
            for (int b = 1; b < a; ++b)
                u = (u << 6) | (str[l++] & 0x3F);
        }
    }
    if (char_out)
        *char_out = u;
    return l;
}

static int wide_length(const unsigned char *str, int str_length) {
    if (!str)
        return -1;
    if (!str_length)
        str_length = (int)strlen((const char*)str);
    const unsigned char *cursor = str;
    int length = 0, counter = 0;
    while (cursor[0] != L'\0' && counter++ < str_length) {
        int cl = read_wide(cursor, NULL);
        length += cl;
        cursor += cl;
    }
    return length;
}

static wchar_t *to_wide(const unsigned char *str, int str_length) {
    wchar_t *ret = malloc(sizeof(wchar_t) * wide_length(str, str_length));
    const unsigned char *cursor = str;
    int length = 0, counter = 0;
    while (cursor[0] != L'\0' && counter < str_length) {
        int cl = read_wide(cursor, &ret[counter++]);
        length += cl;
        cursor += cl;
    }
    return ret;
}

void mel_init(mel_t *mel) {
#ifdef __APPLE__
    // XCode won't print wprintf otherwise...
    setlocale(LC_ALL, "en_US.UTF-8");
#else
    setlocale(LC_ALL, "");
#endif
}

static wchar_t parser_peek(mel_parser_t *p) {
    return p->cursor[0];
}

static wchar_t parser_eof(mel_parser_t *p) {
    return parser_peek(p) == '\0';
}

static inline void parser_update(mel_parser_t *p) {
    p->source = p->cursor;
}

static inline wchar_t parser_advance(mel_parser_t *p) {
    wchar_t current = *p->cursor;
    p->cursor++;
    return current;
}

static inline wchar_t parser_skip(mel_parser_t *p) {
    wchar_t ret = parser_advance(p);
    parser_update(p);
    return ret;
}

static inline wchar_t parser_next(mel_parser_t *p) {
    return parser_eof(p) ? L'\0' : *(p->cursor + 1);
}

static inline mel_token_t make_token(mel_token_type type, const wchar_t *cursor, int length) {
    return (mel_token_t) {
        .type = type,
        .cursor = cursor,
        .length = length
    };
}

#define TOKEN(T) (make_token((T), p->source, (int)(p->cursor - p->source)))

static int parser_peek_nl(mel_parser_t *p) {
    switch (parser_peek(p)) {
        case '\r':
            return parser_next(p) == '\n';
        case '\n':
            return 1;
    }
    return 0;
}

static int parser_peek_whitespace(mel_parser_t *p) {
    switch (parser_peek(p)) {
        case ' ':
        case '\t':
        case '\v':
        case '\r':
        case '\n':
        case '\f':
            return 1;
        default:
            return 0;
    }
}

static int parser_peek_terminators(mel_parser_t *p) {
    switch (parser_peek(p)) {
        case ' ':
        case '\t':
        case '\v':
        case '\r':
        case '\n':
        case '\f':
        case '(':
        case ')':
        case ';':
        case '"':
            return 1;
        default:
            return 0;
    }
}

static int parser_peek_digit(mel_parser_t *p) {
    wchar_t c = parser_peek(p);
    return c >= L'0' && c <= L'9';
}

static void skip_line(mel_parser_t *p) {
    while (!parser_peek_nl(p))
        parser_advance(p);
}

static void skip_whitespace(mel_parser_t *p) {
    for (;;) {
        if (parser_eof(p))
            return;
        switch (parser_peek(p)) {
            case ' ':
            case '\t':
            case '\v':
            case '\r':
            case '\n':
            case '\f':
                parser_advance(p);
                break;
            default:
                return;
        }
    }
}

static mel_token_t read_number(mel_parser_t *p) {
    while (!parser_eof(p) && parser_peek_digit(p))
        parser_advance(p);
    return TOKEN(MEL_TOKEN_NUMBER);
}

static mel_token_t read_string(mel_parser_t *p) {
    mel_token_t ret = TOKEN(MEL_TOKEN_ERROR);
    parser_skip(p); // skip opening "
    for (;;) {
        if (parser_eof(p))
            goto BAIL; // error, unterminated "
        if (parser_peek(p) == L'"')
            break;
        else
            parser_advance(p);
    }
    ret = TOKEN(MEL_TOKEN_STRING);
    parser_skip(p); // skip terminating "
BAIL:
    return ret;
}

static mel_token_t read_atom(mel_parser_t *p) {
    while (!parser_eof(p) && !parser_peek_terminators(p))
        parser_advance(p);
    return TOKEN(MEL_TOKEN_ATOM);
}

static inline mel_token_t single_token(mel_parser_t *p, mel_token_type type) {
    parser_advance(p);
    return TOKEN(type);
}

static mel_token_t next_token(mel_parser_t *p) {
    skip_whitespace(p);
    if (parser_eof(p))
        return make_token(MEL_TOKEN_EOF, p->source, (int)(p->source - p->cursor));
    parser_update(p);
    switch (parser_peek(p)) {
        case ';':
            skip_line(p);
            return next_token(p);
        case '"':
            return read_string(p);
        case '0' ... '9':
            return read_number(p);
        case '(':
            return single_token(p, MEL_TOKEN_LPAREN);
        case ')':
            return single_token(p, MEL_TOKEN_RPAREN);
        default:
            return read_atom(p);
    }
    return TOKEN(MEL_TOKEN_ERROR);
}

static const char *token_str(mel_token_type type) {
    switch (type) {
        case MEL_TOKEN_ERROR:
            return "ERROR";
        case MEL_TOKEN_EOF:
            return "EOF";
        case MEL_TOKEN_LPAREN:
            return "LPAREN";
        case MEL_TOKEN_RPAREN:
            return "RPAREN";
        case MEL_TOKEN_ATOM:
            return "ATOM";
        case MEL_TOKEN_NUMBER:
            return "NUMBER";
        case MEL_TOKEN_STRING:
            return "STRING";
    }
}

static void print_token(mel_token_t *token) {
    wprintf(L"[MEL_TOKEN_%s] '%.*ls'\n", token_str(token->type), token->length, token->cursor);
}

mel_return_status_t mel_parse(mel_t *mel, const unsigned char *str, int str_length, mel_token_t **tokens) {
    const wchar_t *wide_str = to_wide(str, str_length);
    
    mel_parser_t p = {
        .source = wide_str,
        .cursor = wide_str
    };
    for (;;) {
        mel_token_t token = next_token(&p);
        garry_append(*tokens, token);
        switch (token.type) {
            case MEL_TOKEN_ERROR:
            case MEL_TOKEN_EOF:
                goto BAIL;
            case MEL_TOKEN_LPAREN:
            case MEL_TOKEN_RPAREN:
            case MEL_TOKEN_ATOM:
            case MEL_TOKEN_NUMBER:
            case MEL_TOKEN_STRING:
                print_token(&token);
                break;
        }
    }
BAIL:
    return MEL_OK;
}
#endif
