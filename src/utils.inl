#define __garry_raw(a)           ((int*)(void*)(a)-2)
#define __garry_m(a)             __garry_raw(a)[0]
#define __garry_n(a)             __garry_raw(a)[1]
#define __garry_needgrow(a,n)    ((a)==0 || __garry_n(a)+(n) >= __garry_m(a))
#define __garry_maybegrow(a,n)   (__garry_needgrow(a,(n)) ? __garry_grow(a,n) : 0)
#define __garry_grow(a,n)        (*((void **)&(a)) = __garry_growf((a), (n), sizeof(*(a))))
#define __garry_needshrink(a)    (__garry_m(a) > 4 && __garry_n(a) <= __garry_m(a) / 4)
#define __garry_maybeshrink(a)   (__garry_needshrink(a) ? __garry_shrink(a) : 0)
#define __garry_shrink(a)        (*((void **)&(a)) = __garry_shrinkf((a), sizeof(*(a))))
#define garry_free(a)           ((a) ? free(__garry_raw(a)),((a)=NULL) : 0)
#define garry_append(a,v)       (__garry_maybegrow(a,1), (a)[__garry_n(a)++] = (v))
#define garry_count(a)          ((a) ? __garry_n(a) : 0)
#define garry_last(a)           (void*)((a) ? &(a)[__garry_n(a)-1] : NULL)
#define garry_pop(a)            (--__garry_n(a), __garry_maybeshrink(a))

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

static void *__garry_shrinkf(void *arr, int itemsize) {
    int new_capacity = __garry_m(arr) / 2;
    int *p = realloc(arr ? __garry_raw(arr) : 0, itemsize * new_capacity + sizeof(int) * 2);
    if (p) {
        p[0] = new_capacity;
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
        int position = word[i];
        switch (word[i]) {
            case 'a' ... 'z':
                position -= 'a';
                break;
            case 'A' ... 'Z':
                position -= 'A';
                break;
            default:
                return 0;
        }
        if (!temp->children[position])
            return 0;
        temp = temp->children[position];
    }
    return temp != NULL && temp->ischild == 1;
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
