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
    mel_t mel;
    mel_init(&mel);
    int src_length;
    const unsigned char *src = read_file("t/test.lisp", &src_length);
    mel_token_t *tokens = NULL;
    if (mel_parse(&mel, src, src_length, &tokens) != MEL_OK)
        abort();
    free((void*)src);
    return 0;
}
