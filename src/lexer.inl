typedef enum mel_token_type {
    MEL_TOKEN_ERROR = 0,
    MEL_TOKEN_EOF,
    MEL_TOKEN_ATOM,
    MEL_TOKEN_NUMBER,
    MEL_TOKEN_STRING,
    MEL_TOKEN_LPAREN = '(',
    MEL_TOKEN_RPAREN = ')',
    MEL_TOKEN_SQR_LPAREN = '[',
    MEL_TOKEN_SQR_RPAREN = ']',
    MEL_TOKEN_CRL_LPAREN = '{',
    MEL_TOKEN_CRL_RPAREN = '}',
    MEL_TOKEN_SINGLE_QUOTE = '\'',
    MEL_TOKEN_BACK_QUOTE = '`',
    MEL_TOKEN_AT = '@',
    MEL_TOKEN_HASH = '#',
    MEL_TOKEN_COLON = ','
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
} mel_lexer_t;

static void lexer_init(mel_lexer_t *l, const wchar_t *str, int str_length) {
    memset(l, 0, sizeof(mel_lexer_t));
    l->source = str;
    l->cursor = str;
}

static void lexer_free(mel_lexer_t *l) {
    
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

#define TERMINATORS \
    X('.') \
    X('(') \
    X(')') \
    X('[') \
    X(']') \
    X('{') \
    X('}') \
    X('\'') \
    X('`') \
    X(',') \
    X('@') \
    X('#') \
    X(':') \
    X('"')

static int lexer_peek_terminators(mel_lexer_t *p) {
    switch (lexer_peek(p)) {
#define X(C) \
        case C:
            WHITESPACE
            TERMINATORS
#undef X
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
        case '(':
        case ')':
        case '[':
        case ']':
        case '{':
        case '}':
        case '\'':
        case '`':
        case '@':
        case '#':
        case ',':
            return TOKEN((mel_token_type)lexer_advance(p));
        default:
            return read_atom(p);
    }
    return TOKEN(MEL_TOKEN_ERROR);
}

static const char *token_type_str(mel_token_type type) {
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
        case MEL_TOKEN_LPAREN:
            return "LEFT_PAREN";
        case MEL_TOKEN_RPAREN:
            return "RIGHT_PAREN";
        case MEL_TOKEN_SQR_LPAREN:
            return "LEFT_SQUARE_PAREN";
        case MEL_TOKEN_SQR_RPAREN:
            return "RIGHT_SQUARE_PAREN";
        case MEL_TOKEN_CRL_LPAREN:
            return "LEFT_CURLY_PAREN";
        case MEL_TOKEN_CRL_RPAREN:
            return "RIGHT_CURLY_PAREN";
        case MEL_TOKEN_SINGLE_QUOTE:
            return "SINGLE_QUOTE";
        case MEL_TOKEN_BACK_QUOTE:
            return "BACK_QUOTE";
        case MEL_TOKEN_AT:
            return "AT";
        case MEL_TOKEN_HASH:
            return "HASH";
        case MEL_TOKEN_COLON:
            return "COLON";
    }
}

static void print_token(mel_token_t *token) {
    wprintf(L"(MEL_TOKEN_%s, \"%.*ls\", %d:%d:%d)\n", token_type_str(token->type), token->length, token->cursor, token->line, token->position, token->length);
}

static mel_token_t lexer_consume(mel_lexer_t *lexer) {
    lexer->previous = lexer->current;
    lexer->current = next_token(lexer);
    return lexer->previous;
}
