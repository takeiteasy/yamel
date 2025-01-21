typedef struct mel_local {
    
} mel_local_t;

typedef struct mel_compiler {
    
} mel_compiler_t;

static mel_result mel_compile(mel_lexer_t *lexer, mel_chunk_t *chunk) {
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
