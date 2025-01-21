typedef struct mel_local {
    mel_token_t *token;
    int depth;
} mel_local_t;

typedef struct mel_compiler {
    mel_lexer_t *lexer;
    mel_chunk_t *chunk;
    struct {
        mel_local_t *locals;
        int depth;
    } scope;
} mel_compiler_t;

static inline mel_token_t* compiler_peek(mel_compiler_t *cmp) {
    return &cmp->lexer->current;
}

static inline int compiler_check(mel_compiler_t *cmp, mel_token_type type) {
    mel_token_t *token = compiler_peek(cmp);
    return token ? token->type == type : 0;
}

static inline void scope_begin(mel_compiler_t *cmp) {
    cmp->scope.depth++;
}

static inline void scope_end(mel_compiler_t *cmp) {
    cmp->scope.depth--;
}

static mel_result block(mel_compiler_t *cmp) {
    mel_token_t first = lexer_consume(cmp->lexer);
    switch (first.type) {
        case MEL_TOKEN_QUOTE:
            break;
        case MEL_TOKEN_UNQUOTE:
            break;
        case MEL_TOKEN_SETQ:
            break;
        case MEL_TOKEN_PROGN:
            break;
        case MEL_TOKEN_IF:
            break;
        case MEL_TOKEN_COND:
            break;
        case MEL_TOKEN_LAMBDA:
            break;
        case MEL_TOKEN_MACRO:
            break;
        case MEL_TOKEN_EQ:
            break;
        case MEL_TOKEN_CAR:
            break;
        case MEL_TOKEN_CDR:
            break;
        case MEL_TOKEN_CONS:
            break;
        case MEL_TOKEN_PRINT:
            break;
        case MEL_TOKEN_AND:
            break;
        case MEL_TOKEN_OR:
            break;
        case MEL_TOKEN_LT_EQ:
            break;
        case MEL_TOKEN_GT_EQ:
            break;
        case MEL_TOKEN_PERIOD:
            break;
        case MEL_TOKEN_ADD:
            break;
        case MEL_TOKEN_SUB:
            break;
        case MEL_TOKEN_MUL:
            break;
        case MEL_TOKEN_DIV:
            break;
        case MEL_TOKEN_LT:
            break;
        case MEL_TOKEN_GT:
            break;
        case MEL_TOKEN_ATOM:
            break;
        default:
            return MEL_COMPILE_ERROR; // unexpected token
    }
    while (compiler_peek(cmp) &&
           !compiler_check(cmp, MEL_TOKEN_EOF) &&
           !compiler_check(cmp, MEL_TOKEN_RPAREN))
        lexer_consume(cmp->lexer);
    if (!compiler_check(cmp, MEL_TOKEN_RPAREN))
        return MEL_COMPILE_ERROR; // expected closing )
    lexer_consume(cmp->lexer);
    return MEL_OK;
}

static mel_result mel_compile(mel_lexer_t *lexer, mel_chunk_t *chunk) {
    mel_compiler_t compiler = {
        .lexer = lexer,
        .chunk = chunk,
        .scope = {0}
    };
    
    for (;;) {
        lexer->current = next_token(lexer);
        print_token(&lexer->current);
        switch (lexer->current.type) {
            case MEL_TOKEN_ERROR:
                return MEL_COMPILE_ERROR;
            case MEL_TOKEN_EOF:
                emit(lexer, chunk, MEL_OP_EXIT);
                return MEL_OK;
            case MEL_TOKEN_NUMBER:
                emit_number(lexer, chunk);
                emit(lexer, chunk, MEL_OP_RETURN);
                break;
            case MEL_TOKEN_STRING:
                emit_string(lexer, chunk);
                emit(lexer, chunk, MEL_OP_RETURN);
                break;
            case MEL_TOKEN_LPAREN:
                lexer_consume(lexer);
                block(&compiler);
                break;
            case MEL_TOKEN_SLPAREN:
                break;
            case MEL_TOKEN_CLPAREN:
                break;
#define X(N, S) \
            case MEL_TOKEN_##N: { \
                int wlen; \
                emit_constant(lexer, chunk, \
                    mel_obj(mel_string_new(to_wide((const unsigned char*)(":KEYWORD-" S), strlen((":KEYWORD-" S)), &wlen), wlen))); \
                emit(lexer, chunk, MEL_OP_RETURN); \
                break; \
            }
                PRIMITIVES
#undef X
#define X(N, C) \
            case MEL_TOKEN_##N: { \
                char b[11] = ":KEYWORD-"; \
                b[9] = C; \
                b[10] = '\0'; \
                int wlen; \
                emit_constant(lexer, chunk, \
                mel_obj(mel_string_new(to_wide((const unsigned char*)b, 11, &wlen), wlen))); \
                emit(lexer, chunk, MEL_OP_RETURN); \
            }
                BIN_OPS
#undef X
            default:
                return MEL_COMPILE_ERROR; // unexpected token
        }
        lexer->previous = lexer->current;
    }
    return MEL_COMPILE_ERROR;
}

static void vm_push(mel_vm_t *vm, mel_value_t value) {
    garry_append(vm->stack, value);
}

static mel_value_t vm_pop(mel_vm_t *vm) {
    if (!vm->stack)
        return mel_nil();
    mel_value_t ret = *(mel_value_t*)garry_last(vm->stack);
    garry_pop(vm->stack);
    return ret;
}

static mel_result mel_exec(mel_vm_t *vm, mel_chunk_t *chunk) {
    if (!(vm->pc = chunk->data))
        return MEL_RUNTIME_ERROR;
    uint8_t inst;
    while ((inst = *vm->pc++) != MEL_OP_EXIT)
        switch (inst) {
            case MEL_OP_EXIT:
                return MEL_OK;
            case MEL_OP_RETURN:
                mel_print(vm_pop(vm));
                garry_free(vm->stack);
                break;
            case MEL_OP_CONSTANT:
            case MEL_OP_CONSTANT_LONG:
                vm_push(vm, chunk->constants[*vm->pc++]);
                break;
        }
    return MEL_OK;
}
