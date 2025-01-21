#ifndef MEL_HEADER
#define MEL_HEADER
#ifdef __cplusplus
extern "C" {
#endif
#include <wchar.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef double mel_float;

#define TYPES \
    X(BOOLEAN, boolean, bool) \
    X(NUMBER, number, mel_float) \
    X(OBJECT, obj, void*)

typedef enum mel_value_type {
    MEL_VALUE_NIL,
#define X(T, _, __) MEL_VALUE_##T,
    TYPES
#undef X
} mel_value_type;

typedef struct {
    mel_value_type type;
    union {
#define X(_, N, TYPE) TYPE N;
        TYPES
#undef X
    } as;
} mel_value_t;

typedef enum mel_object_type {
    MEL_OBJECT_STRING,
    MEL_OBJECT_TABLE
} mel_object_type;

typedef struct mel_object {
    mel_object_type type;
} mel_object_t;

typedef struct {
    mel_object_t obj;
    int length;
    wchar_t *chars;
} mel_string_t;

typedef struct mel_table {
    mel_object_t obj;
    size_t cap;
    size_t bucketsz;
    size_t nbuckets;
    size_t count;
    size_t mask;
    size_t growat;
    size_t shrinkat;
    uint8_t loadfactor;
    uint8_t growpower;
    void *buckets;
    void *spare;
    void *edata;
} mel_table_t;

typedef struct mel_vm {
    unsigned char *pc;
    mel_value_t *stack;
    mel_value_t current;
    mel_value_t previous;
    mel_table_t *globals;
} mel_vm_t;

bool mel_object_is(mel_value_t value, mel_object_type type);
mel_object_t* mel_obj_new(mel_object_type type, size_t size);
void mel_obj_destroy(mel_object_t *obj);
mel_string_t* mel_string_new(const wchar_t *str, int length);
#define mel_is_string(VAL) (mel_object_is((VAL), MEL_OBJECT_STRING))
#define mel_as_string(VAL) ((mel_string_t*)mel_as_obj((VAL)))
const wchar_t* mel_string_cstr(mel_value_t melv);
int mel_string_length(mel_value_t melv);
mel_table_t* mel_table_new(void);
#define mel_is_table(VAL) (mel_object_is((VAL), MEL_OBJECT_TABLE))
#define mel_as_table(VAL) ((mel_table_t*)mel_as_obj((VAL)))
int mel_table_set(mel_value_t melv, const wchar_t *key, mel_value_t val);
mel_value_t* mel_table_get(mel_value_t melv, const wchar_t *key);
int mel_table_del(mel_value_t melv, const wchar_t *key);
void mel_table_clear(mel_value_t melv);
int mel_table_count(mel_value_t melv);

mel_value_t mel_nil(void);
#define X(T, N, TYPE) \
mel_value_t mel_##N(TYPE v);
TYPES
#undef X

typedef enum mel_result {
    MEL_OK,
    MEL_COMPILE_ERROR,
    MEL_RUNTIME_ERROR
} mel_result;

void mel_fprint(FILE *stream, mel_value_t v);
void mel_print(mel_value_t v);

void mel_init(mel_vm_t *vm);
void mel_destroy(mel_vm_t *vm);

mel_result mel_eval(mel_vm_t *vm, const unsigned char *str, int str_length);
mel_result mel_eval_file(mel_vm_t *vm, const char *path);

#ifdef __cplusplus
}
#endif
#endif // MEL_HEADER

#ifdef MEL_IMPLEMENTATION
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdio.h>
#include <wchar.h>
#include <locale.h>
#include <stdbool.h>
#include <assert.h>

#include "utils.inl"
#include "types.inl"
#include "lexer.inl"

void mel_fprint(FILE *stream, mel_value_t v) {
    switch (v.type) {
        case MEL_VALUE_NIL:
            fprintf(stream, "NIL\n");
            break;
        case MEL_VALUE_BOOLEAN:
            fprintf(stream, "%s\n", v.as.boolean ? "T" : "NIL");
            break;
        case MEL_VALUE_NUMBER:
            fprintf(stream, "%g\n", v.as.number);
            break;
        case MEL_VALUE_OBJECT: {
            mel_object_t *obj = mel_as_obj(v);
            switch (obj->type) {
                case MEL_OBJECT_STRING: {
                    mel_string_t *str = (mel_string_t*)obj;
                    fwprintf(stream, L"%.*ls\n", str->length, str->chars);
                    break;
                }
                default:
                    abort();
            }
            break;
        }
        default:
            abort();
    }
}

void mel_print(mel_value_t v) {
    mel_fprint(stdout, v);
}

void mel_init(mel_vm_t *vm) {
#ifdef __APPLE__
    // XCode won't print wprintf otherwise...
    setlocale(LC_ALL, "en_US.UTF-8");
#else
    setlocale(LC_ALL, "");
#endif
    memset(vm, 0, sizeof(mel_vm_t));
}

void mel_destroy(mel_vm_t *vm) {
    if (vm->stack)
        garry_free(vm->stack);
}

mel_result mel_eval(mel_vm_t *vm, const unsigned char *str, int str_length) {
    mel_result ret = MEL_COMPILE_ERROR;
    if (!str || !str_length)
        goto BAIL;
    const wchar_t *wstr = to_wide(str, str_length, &str_length);
    mel_lexer_t lexer;
    lexer_init(&lexer, wstr, str_length);
    for (;;) {
        lexer_consume(&lexer);
        switch (lexer.current.type) {
            case MEL_TOKEN_ERROR:
            case MEL_TOKEN_EOF:
                goto BAIL;
            default:
                print_token(&lexer.current);
                break;
        }
    }
BAIL:
    lexer_free(&lexer);
    return ret;
}

mel_result mel_eval_file(mel_vm_t *vm, const char *path) {
    int src_length;
    const unsigned char *src = read_file(path, &src_length);
    if (!src || !src_length)
        return MEL_COMPILE_ERROR;
    mel_result ret = mel_eval(vm, src, src_length);
    free((void*)src);
    return ret;
}
#endif
