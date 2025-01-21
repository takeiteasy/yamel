#define SIMPLE_CHARS \
    X(PERIOD, '.') \
    X(LPAREN, '(') \
    X(RPAREN, ')') \
    X(SLPAREN, '[') \
    X(SRPAREN, ']') \
    X(CLPAREN, '{') \
    X(CRPAREN, '}')

#define PREFIX_CHARS \
    X(SQUOTE, '\'') \
    X(BQUOTE, '`') \
    X(COMMA, ',') \
    X(ARROBA, '@') \
    X(HASH, '#') \
    X(SYMBOL, ':')

#define PRIMITIVES \
    X(TRUE, "T") \
    X(NIL, "NIL") \
    X(QUOTE, "QUOTE") \
    X(UNQUOTE, "UNQUOTE") \
    X(SETQ, "SETQ") \
    X(PROGN, "PROGN") \
    X(IF, "IF") \
    X(COND, "COND") \
    X(LAMBDA, "LAMBDA") \
    X(MACRO, "MACRO") \
    X(ATOM, "ATOM") \
    X(EQ, "EQ") \
    X(CAR, "CAR") \
    X(CDR, "CDR") \
    X(CONS, "CONS") \
    X(PRINT, "PRINT") \
    X(AND, "AND") \
    X(OR, "OR") \
    X(LT_EQ, "<=") \
    X(GT_EQ, ">=")

#define BIN_OPS \
    X(ADD, '+') \
    X(SUB, '-') \
    X(MUL, '*') \
    X(DIV, '/') \
    X(LT,  '<') \
    X(GT,  '>')

typedef enum mel_token_type {
    MEL_TOKEN_ERROR = 0,
    MEL_TOKEN_EOF,
    MEL_TOKEN_NUMBER,
    MEL_TOKEN_STRING,
#define X(N, _) \
    MEL_TOKEN_##N,
    PRIMITIVES
#undef X
#define X(N, C) \
    MEL_TOKEN_##N = C,
    SIMPLE_CHARS
    PREFIX_CHARS
    BIN_OPS
#undef X
} mel_token_type;

typedef struct mel_token {
    mel_token_type type;
    const wchar_t *cursor;
    int length;
    int line;
    int position;
} mel_token_t;

typedef struct mel_lexer {
    const wchar_t *source;
    const wchar_t *cursor;
    int current_line;
    int line_position;
    mel_token_t current;
    mel_token_t previous;
    trie *primitives;
} mel_lexer_t;

static void lexer_init(mel_lexer_t *l, const wchar_t *str, int str_length) {
    memset(l, 0, sizeof(mel_lexer_t));
    l->source = str;
    l->cursor = str;
    l->primitives = trie_create();
#define X(N, S) \
    trie_insert(l->primitives, S);
    PRIMITIVES
#undef X
}

static void lexer_free(mel_lexer_t *l) {
    if (l->primitives)
        trie_destroy(l->primitives);
}

static wchar_t lexer_peek(mel_lexer_t *p) {
    return *p->cursor;
}

static wchar_t lexer_eof(mel_lexer_t *p) {
    return lexer_peek(p) == L'\0';
}

static inline void lexer_update(mel_lexer_t *p) {
    p->source = p->cursor;
}

static inline wchar_t lexer_next(mel_lexer_t *p) {
    return lexer_eof(p) ? L'\0' : *(p->cursor + 1);
}

static inline wchar_t lexer_advance(mel_lexer_t *p) {
    wchar_t current = *p->cursor;
    switch (current) {
        case '\r':
            if (lexer_next(p) == '\n') {
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

static inline wchar_t lexer_skip(mel_lexer_t *p) {
    wchar_t ret = lexer_advance(p);
    lexer_update(p);
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

static int lexer_peek_newline(mel_lexer_t *p) {
    switch (lexer_peek(p)) {
        case '\r':
            return lexer_next(p) == '\n' ? 2 : 0;
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

static int lexer_peek_terminators(mel_lexer_t *p) {
    switch (lexer_peek(p)) {
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

static int lexer_peek_digit(mel_lexer_t *p) {
    wchar_t c = lexer_peek(p);
    return c >= L'0' && c <= L'9';
}

static void skip_line(mel_lexer_t *p) {
    while (!lexer_peek_newline(p))
        lexer_advance(p);
}

static inline int lexer_peek_whitespace(mel_lexer_t *p) {
    return is_whitespace(lexer_peek(p));
}

static void skip_whitespace(mel_lexer_t *p) {
    while (!lexer_eof(p) && lexer_peek_whitespace(p))
        lexer_advance(p);
    lexer_update(p);
}

static mel_token_t read_number(mel_lexer_t *p) {
    while (!lexer_eof(p) && lexer_peek_digit(p))
        lexer_advance(p);
    return TOKEN(MEL_TOKEN_NUMBER);
}

static mel_token_t read_string(mel_lexer_t *p) {
    mel_token_t ret = TOKEN(MEL_TOKEN_ERROR);
    lexer_skip(p); // skip opening "
    for (;;) {
        if (lexer_eof(p))
            goto BAIL; // error, unterminated "
        if (lexer_peek(p) == L'"')
            break;
        else
            lexer_advance(p);
    }
    ret = TOKEN(MEL_TOKEN_STRING);
    lexer_skip(p); // skip terminating "
BAIL:
    return ret;
}

static mel_token_type identify(mel_lexer_t *l) {
    int len = (int)(l->cursor - l->source);
    char *buf = malloc(sizeof(char) * len + 1);
    for (int i = 0; i < len; i++)
        buf[i] = toupper(*(l->source + i));
    buf[len] = '\0';
    mel_token_type ret = MEL_TOKEN_ATOM;
    int found = trie_find(l->primitives, buf);
    if (!found)
        goto BAIL;
#define X(N, S) \
    if (!strncmp(buf, S, len)) { \
        ret = MEL_TOKEN_##N; \
        goto BAIL; \
    }
    PRIMITIVES
#undef X
BAIL:
    if (buf)
        free(buf);
    return ret;
}

static mel_token_t read_atom(mel_lexer_t *p) {
    while (!lexer_eof(p) && !lexer_peek_terminators(p))
        lexer_advance(p);
    return TOKEN(identify(p));
}

static inline mel_token_t single_token(mel_lexer_t *p, mel_token_type type) {
    lexer_advance(p);
    return TOKEN(type);
}

static mel_token_t next_token(mel_lexer_t *p) {
    lexer_update(p);
    skip_whitespace(p);
    if (lexer_eof(p))
        return TOKEN(MEL_TOKEN_EOF);
    wchar_t c = lexer_peek(p);
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
            if (is_whitespace(lexer_next(p)))
                return TOKEN(MEL_TOKEN_ERROR);
#define X(N, _) \
        case MEL_TOKEN_##N:
            SIMPLE_CHARS
            BIN_OPS
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
        case MEL_TOKEN_NUMBER:
            return "NUMBER";
        case MEL_TOKEN_STRING:
            return "STRING";
#define X(N, _) \
        case MEL_TOKEN_##N:\
            return #N;
            SIMPLE_CHARS
            PREFIX_CHARS
            PRIMITIVES
            BIN_OPS
#undef X
    }
}

static void print_token(mel_token_t *token) {
    wprintf(L"(MEL_TOKEN_%s, \"%.*ls\", %d:%d:%d)\n", token_str(token->type), token->length, token->cursor, token->line, token->position, token->length);
}

static void emit(mel_lexer_t *lexer, mel_chunk_t *chunk, uint8_t byte) {
    chunk_write(chunk, byte, lexer->previous.line);
}

static void emit_constant(mel_lexer_t *lexer, mel_chunk_t *chunk, mel_value_t value) {
    chunk_write_constant(chunk, value, lexer->previous.line);
}

static void emit_string(mel_lexer_t *lexer, mel_chunk_t *chunk) {
    emit_constant(lexer, chunk, mel_obj(mel_string_new(lexer->current.cursor, lexer->current.length)));
}

static void emit_number(mel_lexer_t *l, mel_chunk_t *chunk) {
    char buf[l->current.length+1];
    for (int i = 0; i < l->current.length; i++)
        buf[i] = (char)*(l->current.cursor + i);
    buf[l->current.length] = '\0';
    mel_value_t v = mel_number(strtod(buf, NULL));
    emit_constant(l, chunk, v);
}

static mel_token_t lexer_consume(mel_lexer_t *lexer) {
    lexer->previous = lexer->current;
    lexer->current = next_token(lexer);
    return lexer->current;
}
