#define MEL_IMPLEMENTATION
#include "mel.h"
#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

int main(int argc, const char *argv[]) {    
    mel_vm_t vm;
    mel_init(&vm);
    if (mel_eval_file(&vm, "t/test.lisp") != MEL_OK)
        return 1;
    mel_destroy(&vm);
    return 0;
}
