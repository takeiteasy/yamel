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
    MEL_PARSER_ERROR,
    MEL_LEXER_ERROR
} mel_return_status_t;

#define SIMPLE_CHARS \
    X(PERIOD, '.') \
    X(LPAREN, '(') \
    X(RPAREN, ')') \
    X(SLPAREN, '[') \
    X(SRPAREN, ']') \
    X(CLPAREN, '{') \
    X(CRPAREN, '}')

#define PREFIX_CHARS \
    X(SINGLE_QUOTE, '\'') \
    X(BACK_QUOTE, '`') \
    X(COMMA, ',') \
    X(ARROBA, '@') \
    X(HASH, '#') \
    X(SYMBOL, ':')

typedef enum mel_token_type {
    MEL_TOKEN_ERROR = 0,
    MEL_TOKEN_EOF,
    
    MEL_TOKEN_ATOM,
    MEL_TOKEN_NUMBER,
    MEL_TOKEN_STRING,
    
#define X(N, C) \
    MEL_TOKEN_##N = C,
    SIMPLE_CHARS
    PREFIX_CHARS
#undef X
} mel_token_type;

typedef struct mel_token {
    mel_token_type type;
    const wchar_t *cursor;
    int length;
    int line;
    int position;
} mel_token_t;

typedef struct mel_parser {
    const wchar_t *source;
    const wchar_t *cursor;
    mel_token_t *tokens;
    int current_line;
    int line_position;
} mel_parser_t;

typedef enum mel_op {
    MEL_OP_RETURN
} mel_op;

typedef struct mel_ast {
    mel_token_t *token;
    struct mel_ast *left, *right;
} mel_ast_t;

typedef struct mel_lexer {
    mel_token_t *tokens;
    int token_count;
    int cursor;
} mel_lexer_t;

mel_return_status_t mel_parse(mel_parser_t *p, const unsigned char *str, int str_length);
mel_return_status_t mel_lexer(mel_parser_t *p, mel_ast_t **ast);

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

static wchar_t parser_peek(mel_parser_t *p) {
    return *p->cursor;
}

static wchar_t parser_eof(mel_parser_t *p) {
    return parser_peek(p) == '\0';
}

static inline void parser_update(mel_parser_t *p) {
    p->source = p->cursor;
}

static inline wchar_t parser_next(mel_parser_t *p) {
    return parser_eof(p) ? L'\0' : *(p->cursor + 1);
}

static inline wchar_t parser_advance(mel_parser_t *p) {
    wchar_t current = *p->cursor;
    switch (current) {
        case '\r':
            if (parser_next(p) == '\n') {
                p->cursor += 2;
                p->current_line++;
                p->line_position = 0;
            } else {
                p->cursor++;
                p->line_position++;
            }
            break;
        case '\n':
            p->cursor++;
            p->current_line++;
            p->line_position = 0;
            break;
        default:
            p->cursor++;
            p->line_position++;
            break;
    }
    return current;
}

static inline wchar_t parser_skip(mel_parser_t *p) {
    wchar_t ret = parser_advance(p);
    parser_update(p);
    return ret;
}

static inline mel_token_t make_token(mel_token_type type, const wchar_t *cursor, int length, int line, int line_position) {
    return (mel_token_t) {
        .type = type,
        .cursor = cursor,
        .length = length,
        .line = line,
        .position = line_position - length
    };
}

#define TOKEN(T) (make_token((T), p->source, (int)(p->cursor - p->source), p->current_line, p->line_position))

static int parser_peek_newline(mel_parser_t *p) {
    switch (parser_peek(p)) {
        case '\r':
            return parser_next(p) == '\n' ? 2 : 0;
        case '\n':
            return 1;
    }
    return 0;
}

#define WHITESPACE \
    X(' ') \
    X('\t') \
    X('\v') \
    X('\r') \
    X('\n') \
    X('\f')

static int is_whitespace(wchar_t c) {
    switch (c) {
#define X(C) case C:
            WHITESPACE
#undef X
            return 1;
        default:
            return 0;
    }
}

static int parser_peek_terminators(mel_parser_t *p) {
    switch (parser_peek(p)) {
#define X(C) case C:
            WHITESPACE
#undef X
#define X(_, C) \
        case C:
            SIMPLE_CHARS
#undef X
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
    while (!parser_peek_newline(p))
        parser_advance(p);
}

static inline int parser_peek_whitespace(mel_parser_t *p) {
    return is_whitespace(parser_peek(p));
}

static void skip_whitespace(mel_parser_t *p) {
    while (!parser_eof(p) && parser_peek_whitespace(p))
        parser_advance(p);
    parser_update(p);
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
    parser_update(p);
    skip_whitespace(p);
    if (parser_eof(p))
        return TOKEN(MEL_TOKEN_EOF);
    wchar_t c = parser_peek(p);
    switch (c) {
        case ';':
            skip_line(p);
            return next_token(p);
        case '"':
            return read_string(p);
        case '0' ... '9':
            return read_number(p);
#define X(N, _) \
        case MEL_TOKEN_##N:
            PREFIX_CHARS
#undef X
            if (is_whitespace(parser_next(p)))
                return TOKEN(MEL_TOKEN_ERROR);
#define X(N, _) \
        case MEL_TOKEN_##N:
            SIMPLE_CHARS
#undef X
            return single_token(p, (mel_token_type)c);
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
        case MEL_TOKEN_ATOM:
            return "ATOM";
        case MEL_TOKEN_NUMBER:
            return "NUMBER";
        case MEL_TOKEN_STRING:
            return "STRING";
#define X(N, _) \
        case MEL_TOKEN_##N:\
            return #N;
            SIMPLE_CHARS
            PREFIX_CHARS
#undef X
    }
}

static void print_token(mel_token_t *token) {
    wprintf(L"(MEL_TOKEN_%s, \"%.*ls\", %d:%d)\n", token_str(token->type), token->length, token->cursor, token->line, token->position);
}

mel_return_status_t mel_parse(mel_parser_t *p, const unsigned char *str, int str_length) {
    const wchar_t *wide_str = to_wide(str, str_length);
    p->source = wide_str;
    p->cursor = wide_str;
    for (;;) {
        mel_token_t token = next_token(p);
        garry_append(p->tokens, token);
        switch (token.type) {
            case MEL_TOKEN_ERROR:
                return MEL_PARSER_ERROR;
            case MEL_TOKEN_EOF:
                return MEL_OK;
            default:
                print_token(&token);
                break;
        }
    }
}

static inline int lexer_eol(mel_lexer_t *l) {
    return l->cursor >= l->token_count;
}

static mel_token_t* lexer_peek(mel_lexer_t *l) {
    return &l->tokens[l->cursor];
}

static mel_token_t* lexer_next(mel_lexer_t *l) {
    return l->cursor + 1 >= l->token_count ? NULL : &l->tokens[l->cursor];
}

mel_return_status_t mel_lexer(mel_parser_t *p, mel_ast_t **ast) {
    mel_lexer_t lexer = {
        .tokens = p->tokens,
        .token_count = garry_count(p->tokens),
        .cursor = 0
    };
    
    while (!lexer_eol(&lexer)) {
        switch (lexer_peek(&lexer)->type) {
            case MEL_TOKEN_ERROR:
                abort(); // should never happen
            case MEL_TOKEN_EOF:
                return MEL_OK;
            case MEL_TOKEN_ATOM:
                break;
            case MEL_TOKEN_NUMBER:
                break;
            case MEL_TOKEN_STRING:
                break;
            case MEL_TOKEN_LPAREN:
                break;
            case MEL_TOKEN_RPAREN:
                break;
            case MEL_TOKEN_SLPAREN:
                break;
            case MEL_TOKEN_SRPAREN:
                break;
            case MEL_TOKEN_CLPAREN:
                break;
            case MEL_TOKEN_CRPAREN:
                break;
            case MEL_TOKEN_SINGLE_QUOTE:
                break;
            case MEL_TOKEN_BACK_QUOTE:
                break;
            case MEL_TOKEN_COMMA:
                break;
            case MEL_TOKEN_ARROBA:
                break;
            case MEL_TOKEN_HASH:
                break;
            case MEL_TOKEN_PERIOD:
                break;
            case MEL_TOKEN_SYMBOL:
                break;
        }
    }
    return MEL_OK;
}
#endif
