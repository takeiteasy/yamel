#define MEL_IMPLEMENTATION
#include "mel.h"
#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

static unsigned char* read_file(const char *path, int *size) {
    unsigned char *result = NULL;
    size_t _size = 0;
    FILE *file = fopen(path, "r");
    if (!file)
        goto BAIL; // _size = 0 failed to open file
    fseek(file, 0, SEEK_END);
    _size = ftell(file);
    rewind(file);
    if (!(result = malloc(sizeof(unsigned char) * _size + 1)))
        goto BAIL; // _size > 0 failed to alloc memory
    if (fread(result, sizeof(unsigned char), _size, file) != _size) {
        free(result);
        _size = -1;
        goto BAIL; // _size = -1 failed to read file
    }
    result[_size] = '\0';
BAIL:
    if (size)
        *size = (int)_size;
    return result;
}

int main(int argc, const char *argv[]) {
#ifdef __APPLE__
    // XCode won't print wprintf otherwise...
    setlocale(LC_ALL, "en_US.UTF-8");
#else
    setlocale(LC_ALL, "");
#endif
    int src_length;
    const unsigned char *src = read_file("t/test.lisp", &src_length);
    mel_parser_t parser = {0};
    if (mel_parse(&parser, src, src_length) != MEL_OK)
        abort();
    mel_ast_t *ast = NULL;
    if (mel_lexer(&parser, &ast) != MEL_OK || !ast)
        abort();
    free((void*)src);
    return 0;
}
