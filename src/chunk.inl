typedef struct mel_chunk_range {
    int offset, line;
} mel_chunk_range_t;

typedef struct mel_chunk {
    unsigned char *data;
    mel_value_t *constants;
    mel_chunk_range_t *lines;
} mel_chunk_t;

typedef enum mel_op {
    MEL_OP_EXIT,
    MEL_OP_RETURN,
    MEL_OP_CONSTANT,
    MEL_OP_CONSTANT_LONG,
    MEL_OP_EVAL,
#define X(N, _) MEL_OP_##N = MEL_TOKEN_##N,
    BIN_OPS
#undef X
} mel_op;

static void chunk_init(mel_chunk_t *chunk) {
    chunk->data = NULL;
    chunk->constants = NULL;
    chunk->lines = NULL;
}

static void chunk_free(mel_chunk_t *chunk) {
    garry_free(chunk->data);
    garry_free(chunk->constants);
    garry_free(chunk->lines);
    memset(chunk, 0, sizeof(mel_chunk_t));
}

static void chunk_write(mel_chunk_t *chunk, uint8_t byte, int line) {
    garry_append(chunk->data, byte);
    if (chunk->lines && garry_count(chunk->lines) > 0) {
        mel_chunk_range_t *linestart = garry_last(chunk->lines);
        if (linestart && linestart->line == line)
            return;
    }
    mel_chunk_range_t result = {
        .offset = garry_count(chunk->data) - 1,
        .line = line
    };
    garry_append(chunk->lines, result);
}

static int get_line(mel_chunk_t *chunk, int instruction) {
    int start = 0;
    int end = garry_count(chunk->lines) - 1;
    for (;;) {
        int mid = (start + end) / 2;
        mel_chunk_range_t *line = &chunk->lines[mid];
        if (instruction < line->offset)
            end = mid - 1;
        else if (mid == garry_count(chunk->lines) - 1 || instruction < chunk->lines[mid + 1].offset)
            return line->line;
        else
            start = mid + 1;
    }
}

static int constant_instruction(const char *name, mel_chunk_t *chunk, int offset) {
    uint8_t c = chunk->data[offset + 1];
    printf("%-16s %4d '", name, c);
    mel_print(chunk->constants[c]);
    printf("'\n");
    return offset + 2;
}

static int long_constant_instruction(const char *name, mel_chunk_t *chunk, int offset) {
    uint32_t c = chunk->data[offset + 1] | (chunk->data[offset + 2] << 8) | (chunk->data[offset + 3] << 16);
    printf("%-16s %4d '", name, c);
    mel_print(chunk->constants[c]);
    printf("'\n");
    return offset + 4;
}

static int simple_instruction(const char* name, int offset) {
    printf("%s\n", name);
    return offset + 1;
}

static int disassemble_instruction(mel_chunk_t *chunk, int offset) {
    printf("%04d ", offset);
    int line = get_line(chunk, offset);
    if (offset > 0 && line == get_line(chunk, offset - 1))
        printf("   | ");
    else
        printf("%4d ", line);
    uint8_t instruction = chunk->data[offset];
    switch (instruction) {
        case MEL_OP_RETURN:
            return simple_instruction("OP_RETURN", offset);
        case MEL_OP_EXIT:
            return simple_instruction("OP_EXIT", offset);
        case MEL_OP_CONSTANT:
            return constant_instruction("OP_CONSTANT", chunk, offset);
        case MEL_OP_CONSTANT_LONG:
            return long_constant_instruction("OP_CONSTANT_LONG", chunk, offset);
        default:
            printf("Unknown opcode %d\n", instruction);
            return offset + 1;
    }
}

static void chunk_disassemble(mel_chunk_t *chunk, const char *name) {
    printf("== %s ==\n", name);
    for (int offset = 0; offset < garry_count(chunk->data);)
        offset = disassemble_instruction(chunk, offset);
}

static int chunk_add_constant(mel_chunk_t *chunk, mel_value_t value) {
    garry_append(chunk->constants, value);
    return garry_count(chunk->constants) - 1;
}

static void chunk_write_constant(mel_chunk_t *chunk, mel_value_t value, int line) {
    int index = chunk_add_constant(chunk, value);
    if (index < 256) {
        chunk_write(chunk, MEL_OP_CONSTANT, line);
        chunk_write(chunk, (uint8_t)index, line);
    } else {
        chunk_write(chunk, MEL_OP_CONSTANT_LONG, line);
        chunk_write(chunk, (uint8_t)(index & 0xff), line);
        chunk_write(chunk, (uint8_t)((index >> 8) & 0xff), line);
        chunk_write(chunk, (uint8_t)((index >> 16) & 0xff), line);
    }
}
