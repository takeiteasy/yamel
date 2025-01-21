mel_value_t mel_nil(void) {
    return (mel_value_t) {
        .type = MEL_VALUE_NIL
    };
}

#define X(T, N, TYPE) \
mel_value_t mel_##N(TYPE v) { \
    return (mel_value_t) { \
        .type = MEL_VALUE_##T, \
        .as.N = v \
    }; \
} \
bool mel_is_##N(mel_value_t v) { \
    return v.type == MEL_VALUE_##T; \
} \
TYPE mel_as_##N(mel_value_t v) { \
    return (TYPE)v.as.N; \
}
TYPES
#undef X

bool mel_object_is(mel_value_t value, mel_object_type type) {
    return mel_is_obj(value) && ((mel_object_t*)mel_as_obj(value))->type == type;
}

mel_object_t* mel_obj_new(mel_object_type type, size_t size) {
    mel_object_t *result = malloc(size);
    result->type = type;
    return result;
}

void mel_obj_destroy(mel_object_t *obj) {
    switch (obj->type) {
        case MEL_OBJECT_STRING: {
            mel_string_t* string = (mel_string_t*)obj;
            free((void*)string->chars);
            free(string);
            break;
        }
    }
}

mel_string_t* mel_string_new(const wchar_t *chars, int length) {
    mel_string_t *result = malloc(sizeof(mel_string_t));
    if (!result)
        return NULL;
    result->obj.type = MEL_OBJECT_STRING;
    result->length = length;
    if (!(result->chars = malloc(sizeof(wchar_t) * (length + 1)))) {
        free(result);
        return NULL;
    }
    memcpy(result->chars, chars, length * sizeof(wchar_t));
    result->chars[length] = L'\0';
    return result;
}
