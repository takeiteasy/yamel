#ifndef MEL_HEADER
#define MEL_HEADER
#ifdef __cplusplus
extern "C" {
#endif
#include <wchar.h>
#include <stdbool.h>

typedef unsigned long long mel_int;
typedef double mel_float;

#define TYPES \
    X(BOOLEAN, boolean, bool) \
    X(INTEGER, integer, mel_int) \
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
    MEL_OBJECT_STRING
} mel_object_type;

typedef struct mel_object {
    mel_object_type type;
} mel_object_t;

typedef struct {
    mel_object_t obj;
    int length;
    bool owns_chars;
    wchar_t chars[];
} mel_string_t;

typedef struct mel_chunk mel_chunk_t;

typedef struct mel_vm {
    mel_chunk_t *chunk;
    unsigned char *sp;
    mel_value_t *stack;
    mel_value_t current;
    mel_value_t previous;
    mel_object_t *objects;
    char *error;
} mel_vm_t;

bool mel_object_is(mel_value_t value, mel_object_type type);
mel_object_t* mel_obj_new(mel_object_type type, size_t size);
void mel_obj_destroy(mel_object_t *obj);
mel_string_t* mel_string_new(const wchar_t *str, int length, bool owns_chars);
#define mel_is_string(VAL) (mel_object_is((VAL), MEL_OBJECT_STRING))
#define mel_as_string(VAL) ((mel_string_t*)mel_as_obj((VAL)))
#define mel_as_cstring(VAL) ((mel_as_string((VAL)))->chars)
#define mel_string_length(VAL) ((mel_as_string((VAL)))->length)
void mel_print_value(mel_value_t value);

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

void mel_init(mel_vm_t *vm);
void mel_destroy(mel_vm_t *vm);

mel_result mel_exec(mel_vm_t *vm, const unsigned char *str, int str_length);
mel_result mel_exec_file(mel_vm_t *vm, const char *path);

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

#define __garry_raw(a)           ((int*)(void*)(a)-2)
#define __garry_m(a)             __garry_raw(a)[0]
#define __garry_n(a)             __garry_raw(a)[1]
#define __garry_needgrow(a,n)    ((a)==0 || __garry_n(a)+(n) >= __garry_m(a))
#define __garry_maybegrow(a,n)   (__garry_needgrow(a,(n)) ? __garry_grow(a,n) : 0)
#define __garry_grow(a,n)        (*((void **)&(a)) = __garry_growf((a), (n), sizeof(*(a))))
#define garry_free(a)           ((a) ? free(__garry_raw(a)),((a)=NULL) : 0)
#define garry_append(a,v)       (__garry_maybegrow(a,1), (a)[__garry_n(a)++] = (v))
#define garry_count(a)          ((a) ? __garry_n(a) : 0)
#define garry_last(a)           (void*)((a) ? &(a)[__garry_n(a)-1] : NULL)

static void *__garry_growf(void *arr, int increment, int itemsize) {
    int dbl_cur = arr ? 2 * __garry_m(arr) : 0;
    int min_needed = garry_count(arr) + increment;
    int m = dbl_cur > min_needed ? dbl_cur : min_needed;
    int *p = realloc(arr ? __garry_raw(arr) : 0, (itemsize * m) + (sizeof(int) * 2));
    if (p) {
        if (!arr)
            p[1] = 0;
        p[0] = m;
        return p + 2;
    }
    return NULL;
}

typedef struct trie {
    struct trie *children[26];
    int ischild;
} trie;

static trie* trie_create(void) {
    trie *result = malloc(sizeof(trie));
    memset(result, 0, sizeof(trie));
    return result;
}

static inline void trie_destroy(trie *node) {
    for (int i = 0; i < 26; i++)
        if (node->children[i])
            trie_destroy(node->children[i]);
    free(node);
}

static trie* trie_insert(trie *root, const char *word) {
    trie *cursor = root;
    for (const char *p = word; *p; p++) {
        char c = *p;
        switch (c) {
            case 'a' ... 'z':
                c = c - 32;
            case 'A' ... 'Z':;
                int i = c - 'A';
                if (!cursor->children[i])
                    cursor->children[i] = trie_create();
                cursor = cursor->children[i];
                break;
            default:
                return NULL;
        }
    }
    cursor->ischild = 1;
    return root;
}

static int trie_find(trie* root, const char* word) {
    trie* temp = root;
    for(int i = 0; word[i] != '\0'; i++) {
        int position = word[i] - 'a';
        if (!temp->children[position])
            return 0;
        temp = temp->children[position];
    }
    return temp != NULL && temp->ischild == 1;
}

static void MM86128(const void *key, const int len, uint32_t seed, void *out) {
#define ROTL32(x, r) ((x << r) | (x >> (32 - r)))
#define FMIX32(h) h^=h>>16; h*=0x85ebca6b; h^=h>>13; h*=0xc2b2ae35; h^=h>>16;
    const uint8_t * data = (const uint8_t*)key;
    const int nblocks = len / 16;
    uint32_t h1 = seed;
    uint32_t h2 = seed;
    uint32_t h3 = seed;
    uint32_t h4 = seed;
    uint32_t c1 = 0x239b961b;
    uint32_t c2 = 0xab0e9789;
    uint32_t c3 = 0x38b34ae5;
    uint32_t c4 = 0xa1e38b93;
    const uint32_t * blocks = (const uint32_t *)(data + nblocks*16);
    for (int i = -nblocks; i; i++) {
        uint32_t k1 = blocks[i*4+0];
        uint32_t k2 = blocks[i*4+1];
        uint32_t k3 = blocks[i*4+2];
        uint32_t k4 = blocks[i*4+3];
        k1 *= c1; k1  = ROTL32(k1,15); k1 *= c2; h1 ^= k1;
        h1 = ROTL32(h1,19); h1 += h2; h1 = h1*5+0x561ccd1b;
        k2 *= c2; k2  = ROTL32(k2,16); k2 *= c3; h2 ^= k2;
        h2 = ROTL32(h2,17); h2 += h3; h2 = h2*5+0x0bcaa747;
        k3 *= c3; k3  = ROTL32(k3,17); k3 *= c4; h3 ^= k3;
        h3 = ROTL32(h3,15); h3 += h4; h3 = h3*5+0x96cd1c35;
        k4 *= c4; k4  = ROTL32(k4,18); k4 *= c1; h4 ^= k4;
        h4 = ROTL32(h4,13); h4 += h1; h4 = h4*5+0x32ac3b17;
    }
    const uint8_t * tail = (const uint8_t*)(data + nblocks*16);
    uint32_t k1 = 0;
    uint32_t k2 = 0;
    uint32_t k3 = 0;
    uint32_t k4 = 0;
    switch(len & 15) {
        case 15:
            k4 ^= tail[14] << 16;
        case 14:
            k4 ^= tail[13] << 8;
        case 13:
            k4 ^= tail[12] << 0;
            k4 *= c4; k4  = ROTL32(k4,18); k4 *= c1; h4 ^= k4;
        case 12:
            k3 ^= tail[11] << 24;
        case 11:
            k3 ^= tail[10] << 16;
        case 10:
            k3 ^= tail[ 9] << 8;
        case 9:
            k3 ^= tail[ 8] << 0;
            k3 *= c3; k3  = ROTL32(k3,17); k3 *= c4; h3 ^= k3;
        case 8:
            k2 ^= tail[ 7] << 24;
        case 7:
            k2 ^= tail[ 6] << 16;
        case 6:
            k2 ^= tail[ 5] << 8;
        case 5:
            k2 ^= tail[ 4] << 0;
            k2 *= c2; k2  = ROTL32(k2,16); k2 *= c3; h2 ^= k2;
        case 4:
            k1 ^= tail[ 3] << 24;
        case 3:
            k1 ^= tail[ 2] << 16;
        case 2:
            k1 ^= tail[ 1] << 8;
        case 1:
            k1 ^= tail[ 0] << 0;
            k1 *= c1; k1  = ROTL32(k1,15); k1 *= c2; h1 ^= k1;
    };
    h1 ^= len; h2 ^= len; h3 ^= len; h4 ^= len;
    h1 += h2; h1 += h3; h1 += h4;
    h2 += h1; h3 += h1; h4 += h1;
    FMIX32(h1); FMIX32(h2); FMIX32(h3); FMIX32(h4);
    h1 += h2; h1 += h3; h1 += h4;
    h2 += h1; h3 += h1; h4 += h1;
    ((uint32_t*)out)[0] = h1;
    ((uint32_t*)out)[1] = h2;
    ((uint32_t*)out)[2] = h3;
    ((uint32_t*)out)[3] = h4;
}

static inline uint64_t murmur(const void *data, size_t len, uint32_t seed) {
    static char out[16];
    MM86128(data, (int)len, (uint32_t)seed, &out);
    return *(uint64_t*)out;
}

static int read_wide(const unsigned char* str, wchar_t* char_out) {
    wchar_t u = *str, l = 1;
    if ((u & 0xC0) == 0xC0) {
        int a = (u & 0x20) ? ((u & 0x10) ? ((u & 0x08) ? ((u & 0x04) ? 6 : 5) : 4) : 3) : 2;
        if (a < 6 || !(u & 0x02)) {
            u = ((u << (a + 1)) & 0xFF) >> (a + 1);
            for (int b = 1; b < a; ++b)
                u = (u << 6) | (str[l++] & 0x3F);
        }
    }
    if (char_out)
        *char_out = u;
    return l;
}

static int wide_length(const unsigned char *str, int str_length) {
    if (!str)
        return -1;
    if (!str_length)
        str_length = (int)strlen((const char*)str);
    const unsigned char *cursor = str;
    int length = 0, counter = 0;
    while (cursor[0] != L'\0' && counter++ < str_length) {
        int cl = read_wide(cursor, NULL);
        length += cl;
        cursor += cl;
    }
    return length;
}

static wchar_t *to_wide(const unsigned char *str, int str_length, int *out_length) {
    wchar_t *ret = malloc(sizeof(wchar_t) * (wide_length(str, str_length) + 1));
    const unsigned char *cursor = str;
    int length = 0, counter = 0;
    while (cursor[0] != L'\0' && counter < str_length) {
        int cl = read_wide(cursor, &ret[counter++]);
        length += cl;
        cursor += cl;
    }
    ret[counter] = '\0';
    if (out_length)
        *out_length = counter;
    return ret;
}

#if defined(_MSC_VER)
#define RESTRICT __restrict
#elif defined(__clang__) || defined(__GNUC__)
#define RESTRICT __restrict__
#elif defined(__IAR_SYSTEMS_ICC__)
#define RESTRICT __restrict
#else
#define RESTRICT
#endif

static unsigned char* wide_codepoint(const unsigned char *RESTRICT str, wchar_t *RESTRICT out_codepoint) {
  if (0xf0 == (0xf8 & str[0])) {
    /* 4 byte utf8 codepoint */
    *out_codepoint = ((0x07 & str[0]) << 18) | ((0x3f & str[1]) << 12) |
                     ((0x3f & str[2]) << 6) | (0x3f & str[3]);
    str += 4;
  } else if (0xe0 == (0xf0 & str[0])) {
    /* 3 byte utf8 codepoint */
    *out_codepoint =
        ((0x0f & str[0]) << 12) | ((0x3f & str[1]) << 6) | (0x3f & str[2]);
    str += 3;
  } else if (0xc0 == (0xe0 & str[0])) {
    /* 2 byte utf8 codepoint */
    *out_codepoint = ((0x1f & str[0]) << 6) | (0x3f & str[1]);
    str += 2;
  } else {
    /* 1 byte utf8 codepoint otherwise */
    *out_codepoint = str[0];
    str += 1;
  }

  return (unsigned char *)str;
}

static unsigned char *wide_cat_codepoint(unsigned char *str, wchar_t chr, size_t n) {
    if (0 == ((wchar_t)0xffffff80 & chr)) {
        /* 1-byte/7-bit ascii
         * (0b0xxxxxxx) */
        if (n < 1) {
            return 0;
        }
        str[0] = (unsigned char)chr;
        str += 1;
    } else if (0 == ((wchar_t)0xfffff800 & chr)) {
        /* 2-byte/11-bit utf8 code point
         * (0b110xxxxx 0b10xxxxxx) */
        if (n < 2) {
            return 0;
        }
        str[0] = (unsigned char)(0xc0 | (unsigned char)((chr >> 6) & 0x1f));
        str[1] = (unsigned char)(0x80 | (unsigned char)(chr & 0x3f));
        str += 2;
    } else if (0 == ((wchar_t)0xffff0000 & chr)) {
        /* 3-byte/16-bit utf8 code point
         * (0b1110xxxx 0b10xxxxxx 0b10xxxxxx) */
        if (n < 3) {
            return 0;
        }
        str[0] = (unsigned char)(0xe0 | (unsigned char)((chr >> 12) & 0x0f));
        str[1] = (unsigned char)(0x80 | (unsigned char)((chr >> 6) & 0x3f));
        str[2] = (unsigned char)(0x80 | (unsigned char)(chr & 0x3f));
        str += 3;
    } else { /* if (0 == ((int)0xffe00000 & chr)) { */
        /* 4-byte/21-bit utf8 code point
         * (0b11110xxx 0b10xxxxxx 0b10xxxxxx 0b10xxxxxx) */
        if (n < 4) {
            return 0;
        }
        str[0] = (unsigned char)(0xf0 | (unsigned char)((chr >> 18) & 0x07));
        str[1] = (unsigned char)(0x80 | (unsigned char)((chr >> 12) & 0x3f));
        str[2] = (unsigned char)(0x80 | (unsigned char)((chr >> 6) & 0x3f));
        str[3] = (unsigned char)(0x80 | (unsigned char)(chr & 0x3f));
        str += 4;
    }
    
    return str;
}

static wchar_t wide_upcase_codepoint(wchar_t cp) {
  if (((0x0061 <= cp) && (0x007a >= cp)) ||
      ((0x00e0 <= cp) && (0x00f6 >= cp)) ||
      ((0x00f8 <= cp) && (0x00fe >= cp)) ||
      ((0x03b1 <= cp) && (0x03c1 >= cp)) ||
      ((0x03c3 <= cp) && (0x03cb >= cp)) ||
      ((0x0430 <= cp) && (0x044f >= cp))) {
      cp -= 32;
  } else if ((0x0450 <= cp) && (0x045f >= cp)) {
      cp -= 80;
  } else if (((0x0100 <= cp) && (0x012f >= cp)) ||
             ((0x0132 <= cp) && (0x0137 >= cp)) ||
             ((0x014a <= cp) && (0x0177 >= cp)) ||
             ((0x0182 <= cp) && (0x0185 >= cp)) ||
             ((0x01a0 <= cp) && (0x01a5 >= cp)) ||
             ((0x01de <= cp) && (0x01ef >= cp)) ||
             ((0x01f8 <= cp) && (0x021f >= cp)) ||
             ((0x0222 <= cp) && (0x0233 >= cp)) ||
             ((0x0246 <= cp) && (0x024f >= cp)) ||
             ((0x03d8 <= cp) && (0x03ef >= cp)) ||
             ((0x0460 <= cp) && (0x0481 >= cp)) ||
             ((0x048a <= cp) && (0x04ff >= cp))) {
      cp &= ~0x1;
  } else if (((0x0139 <= cp) && (0x0148 >= cp)) ||
             ((0x0179 <= cp) && (0x017e >= cp)) ||
             ((0x01af <= cp) && (0x01b0 >= cp)) ||
             ((0x01b3 <= cp) && (0x01b6 >= cp)) ||
             ((0x01cd <= cp) && (0x01dc >= cp))) {
      cp -= 1;
      cp |= 0x1;
  } else {
      switch (cp) {
          default:
              break;
          case 0x00ff:
              cp = 0x0178;
              break;
          case 0x0180:
              cp = 0x0243;
              break;
          case 0x01dd:
              cp = 0x018e;
              break;
          case 0x019a:
              cp = 0x023d;
              break;
          case 0x019e:
              cp = 0x0220;
              break;
          case 0x0292:
              cp = 0x01b7;
              break;
          case 0x01c6:
              cp = 0x01c4;
              break;
          case 0x01c9:
              cp = 0x01c7;
              break;
          case 0x01cc:
              cp = 0x01ca;
              break;
          case 0x01f3:
              cp = 0x01f1;
              break;
          case 0x01bf:
              cp = 0x01f7;
              break;
          case 0x0188:
              cp = 0x0187;
              break;
          case 0x018c:
              cp = 0x018b;
              break;
          case 0x0192:
              cp = 0x0191;
              break;
          case 0x0199:
              cp = 0x0198;
              break;
          case 0x01a8:
              cp = 0x01a7;
              break;
          case 0x01ad:
              cp = 0x01ac;
              break;
          case 0x01b9:
              cp = 0x01b8;
              break;
          case 0x01bd:
              cp = 0x01bc;
              break;
          case 0x01f5:
              cp = 0x01f4;
              break;
          case 0x023c:
              cp = 0x023b;
              break;
          case 0x0242:
              cp = 0x0241;
              break;
          case 0x037b:
              cp = 0x03fd;
              break;
          case 0x037c:
              cp = 0x03fe;
              break;
          case 0x037d:
              cp = 0x03ff;
              break;
          case 0x03f3:
              cp = 0x037f;
              break;
          case 0x03ac:
              cp = 0x0386;
              break;
          case 0x03ad:
              cp = 0x0388;
              break;
          case 0x03ae:
              cp = 0x0389;
              break;
          case 0x03af:
              cp = 0x038a;
              break;
          case 0x03cc:
              cp = 0x038c;
              break;
          case 0x03cd:
              cp = 0x038e;
              break;
          case 0x03ce:
              cp = 0x038f;
              break;
          case 0x0371:
              cp = 0x0370;
              break;
          case 0x0373:
              cp = 0x0372;
              break;
          case 0x0377:
              cp = 0x0376;
              break;
          case 0x03d1:
              cp = 0x0398;
              break;
          case 0x03d7:
              cp = 0x03cf;
              break;
          case 0x03f2:
              cp = 0x03f9;
              break;
          case 0x03f8:
              cp = 0x03f7;
              break;
          case 0x03fb:
              cp = 0x03fa;
              break;
      }
  }
    
    return cp;
}

static size_t wide_codepoint_size(wchar_t chr) {
    if (0 == ((wchar_t)0xffffff80 & chr)) {
        return 1;
    } else if (0 == ((wchar_t)0xfffff800 & chr)) {
        return 2;
    } else if (0 == ((wchar_t)0xffff0000 & chr)) {
        return 3;
    } else { /* if (0 == ((int)0xffe00000 & chr)) { */
        return 4;
    }
}

static void wide_upcase(unsigned char *RESTRICT str) {
    wchar_t cp = 0;
    unsigned char *pn = wide_codepoint((const unsigned char*)str, &cp);
    while (cp != 0) {
        const wchar_t lwr_cp = wide_upcase_codepoint(cp);
        const size_t size = wide_codepoint_size(lwr_cp);
        if (lwr_cp != cp)
            wide_cat_codepoint((unsigned char*)str, lwr_cp, size);
        str = pn;
        pn = wide_codepoint((const unsigned char*)str, &cp);
    }
}

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
            if (string->owns_chars)
                free((char*)string->chars);
            free(string);
            break;
        }
    }
}

mel_string_t* mel_string_new(const wchar_t *chars, int length, bool owns_chars) {
    mel_string_t *result = malloc(sizeof(mel_string_t));
    result->obj.type = MEL_OBJECT_STRING;
    result->length = length;
//    if (chars && length) {
//        memcpy(result->chars, chars, length * sizeof(wchar_t));
//        result->chars[length] = '\0';
//    }
    return result;
}

void mel_print_value(mel_value_t value) {
    switch (value.type) {
        case MEL_VALUE_NIL:
            printf("NIL");
            break;
        case MEL_VALUE_BOOLEAN:
            printf("%s", value.as.boolean ? "T" : "NIL");
            break;
        case MEL_VALUE_INTEGER:
            printf("%llu", mel_as_integer(value));
            break;
        case MEL_VALUE_NUMBER:
            printf("%g", mel_as_number(value));
            break;
        case MEL_VALUE_OBJECT: {
            mel_object_t *obj = mel_as_obj(value);
            switch (obj->type) {
                case MEL_OBJECT_STRING:
                    wprintf(L"\"%.*ls\"", mel_string_length(value), mel_as_cstring(value));
                    break;
                default:
                    abort();
            }
            break;
        }
        default:
            abort();
    }
}

typedef struct mel_chunk_range {
    int offset, line;
} mel_chunk_range_t;

struct mel_chunk {
    unsigned char *data;
    mel_value_t *constants;
    mel_chunk_range_t *lines;
};

typedef enum mel_op {
    MEL_OP_RETURN,
    MEL_OP_CONSTANT,
    MEL_OP_CONSTANT_LONG
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
    mel_print_value(chunk->constants[c]);
    printf("'\n");
    return offset + 2;
}

static int long_constant_instruction(const char *name, mel_chunk_t *chunk, int offset) {
    uint32_t c = chunk->data[offset + 1] | (chunk->data[offset + 2] << 8) | (chunk->data[offset + 3] << 16);
    printf("%-16s %4d '", name, c);
    mel_print_value(chunk->constants[c]);
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

#define SIMPLE_CHARS \
    X(PERIOD, '.') \
    X(LPAREN, '(') \
    X(RPAREN, ')') \
    X(SLPAREN, '[') \
    X(SRPAREN, ']') \
    X(CLPAREN, '{') \
    X(CRPAREN, '}')

#define PREFIX_CHARS \
    X(SINGLE_QUOTE, '\'') \
    X(BACK_QUOTE, '`') \
    X(COMMA, ',') \
    X(ARROBA, '@') \
    X(HASH, '#') \
    X(SYMBOL, ':')

#define PRIMITIVES \
    X(QUOTE, "QUOTE") \
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
    X(ASSIGN, "=") \
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
    MEL_TOKEN_PRIMITIVE,
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

static void lexer_init(mel_lexer_t *l, const unsigned char *str, int str_length) {
    const wchar_t *wide_str = to_wide(str, str_length, NULL);
    l->source = wide_str;
    l->cursor = wide_str;
    l->primitives = trie_create();
#define X(N, S) \
    trie_insert(l->primitives, S);
    PRIMITIVES
#undef X
}

static void lexer_free(mel_lexer_t *l) {
    if (l->source)
        free((void*)l->source);
    if (l->primitives)
        trie_destroy(l->primitives);
}

static wchar_t lexer_peek(mel_lexer_t *p) {
    return *p->cursor;
}

static wchar_t lexer_eof(mel_lexer_t *p) {
    return lexer_peek(p) == '\0';
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
        buf[i] = *(l->source + i);
    buf[len] = '\0';
    int found = trie_find(l->primitives, buf);
    free(buf);
    return found ? MEL_TOKEN_PRIMITIVE : MEL_TOKEN_ATOM;
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
        case MEL_TOKEN_PRIMITIVE:
            return "PRIMITIVE";
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

static void emit(mel_lexer_t *parser, mel_chunk_t *chunk, uint8_t byte) {
    chunk_write(chunk, byte, parser->previous.line);
}

static void emit_op(mel_lexer_t *parser, mel_chunk_t *chunk, uint8_t byte1, uint8_t byte2) {
    emit(parser, chunk, byte1);
    emit(parser, chunk, byte2);
}

static void emit_constant(mel_lexer_t *parser, mel_chunk_t *chunk, mel_value_t value) {
    chunk_write_constant(chunk, value, parser->previous.line);
}

static void emit_number(mel_lexer_t *parser, mel_chunk_t *chunk) {
    static char buf[513];
    memset(buf, 0, sizeof(char) * 513);
    if (parser->current.length >= 512)
        abort();
    memcpy(buf, parser->cursor, parser->current.length);
    buf[parser->current.length+1] = '\0';
    double value = strtod(buf, NULL);
    emit_constant(parser, chunk, mel_number(value));
}

static mel_result mel_compile(mel_lexer_t *lexer, mel_chunk_t *chunk) {
    for (;;) {
        lexer->current = next_token(lexer);
        print_token(&lexer->current);
        switch (lexer->current.type) {
            case MEL_TOKEN_ERROR:
                return MEL_COMPILE_ERROR;
            case MEL_TOKEN_EOF:
                emit(lexer, chunk, MEL_OP_RETURN);
                return MEL_OK;
            case MEL_TOKEN_ATOM:
                break;
            case MEL_TOKEN_STRING:
                emit_constant(lexer, chunk, mel_obj(mel_string_new(lexer->current.cursor, lexer->current.length, false)));
                break;
            case MEL_TOKEN_NUMBER:
                emit_number(lexer, chunk);
                break;
            default:
                break;
        }
        lexer->previous = lexer->current;
    }
    return MEL_COMPILE_ERROR;
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

mel_result mel_exec(mel_vm_t *vm, const unsigned char *str, int str_length) {
    mel_result ret = MEL_COMPILE_ERROR;
    if (!str || !str_length)
        goto BAIL;
    mel_lexer_t lexer = {0};
    lexer_init(&lexer, str, str_length);
    mel_chunk_t chunk;
    chunk_init(&chunk);
    if (mel_compile(&lexer, &chunk) != MEL_OK)
        return MEL_COMPILE_ERROR;
    chunk_disassemble(&chunk, "test");
    vm->chunk = &chunk;
    vm->sp = vm->chunk->data;
    ret = MEL_OK;
BAIL:
    lexer_free(&lexer);
    chunk_free(&chunk);
    return ret;
}

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

mel_result mel_exec_file(mel_vm_t *vm, const char *path) {
    int src_length;
    const unsigned char *src = read_file(path, &src_length);
    if (!src || !src_length)
        return MEL_COMPILE_ERROR;
    mel_result ret = mel_exec(vm, src, src_length);
    free((void*)src);
    return ret;
}
#endif
