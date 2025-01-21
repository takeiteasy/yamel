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

static void free_table_elements(mel_table_t *table) {
    for (size_t i = 0; i < table->nbuckets; i++) {
        struct bucket *bucket = bucket_at(table, i);
        if (bucket->dib) {
            mel_value_t *value = bucket_item(bucket);
            if (mel_is_obj(*value))
                mel_obj_destroy(mel_as_obj(*value));
        }
    }
}

void mel_obj_destroy(mel_object_t *obj) {
    switch (obj->type) {
        case MEL_OBJECT_STRING: {
            mel_string_t* string = (mel_string_t*)obj;
            free((void*)string->chars);
            free(string);
            break;
        }
        case MEL_OBJECT_TABLE: {
            mel_table_t* table = (mel_table_t*)obj;
            free_table_elements(table);
            free(table);
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

mel_table_t* mel_table_new(void) {
    size_t cap = 16;
    size_t bucketsz = sizeof(struct bucket) + sizeof(mel_value_t);
    while (bucketsz & (sizeof(uintptr_t)-1))
        bucketsz++;
    // hashmap + spare + edata
    size_t size = sizeof(mel_table_t)+bucketsz*2;
    mel_table_t *table = malloc(size);
    if (!table)
        return NULL;
    memset(table, 0, sizeof(mel_table_t));
    table->bucketsz = bucketsz;
    table->spare = ((char*)table)+sizeof(mel_table_t);
    table->edata = (char*)table->spare+bucketsz;
    table->cap = cap;
    table->nbuckets = cap;
    table->mask = table->nbuckets-1;
    table->buckets = malloc(table->bucketsz*table->nbuckets);
    if (!table->buckets) {
        free(table);
        return NULL;
    }
    memset(table->buckets, 0, table->bucketsz*table->nbuckets);
    table->growpower = 1;
    table->loadfactor = clamp_load_factor(HASHMAP_LOAD_FACTOR, GROW_AT) * 100;
    table->growat = table->nbuckets * (table->loadfactor / 100.0);
    table->shrinkat = table->nbuckets * SHRINK_AT;
    return table;
}

static bool table_resize(mel_table_t *table, size_t new_cap) {
    mel_table_t *map2 = mel_table_new();
    if (!map2)
        abort();
    for (size_t i = 0; i < table->nbuckets; i++) {
        struct bucket *entry = bucket_at(table, i);
        if (!entry->dib)
            continue;
        entry->dib = 1;
        size_t j = entry->hash & map2->mask;
        for (;;) {
            struct bucket *bucket = bucket_at(map2, j);
            if (bucket->dib == 0) {
                memcpy(bucket, entry, table->bucketsz);
                break;
            }
            if (bucket->dib < entry->dib) {
                memcpy(map2->spare, bucket, table->bucketsz);
                memcpy(bucket, entry, table->bucketsz);
                memcpy(entry, map2->spare, table->bucketsz);
            }
            j = (j + 1) & map2->mask;
            entry->dib += 1;
        }
    }
    free(table->buckets);
    table->buckets = map2->buckets;
    table->nbuckets = map2->nbuckets;
    table->mask = map2->mask;
    table->growat = map2->growat;
    table->shrinkat = map2->shrinkat;
    free(map2);
    return true;
}

int mel_table_set(mel_value_t obj, const wchar_t *key, mel_value_t val) {
    assert(mel_is_table(obj));
    mel_table_t *table = mel_as_table(obj);
    if (table->count >= table->growat)
        if (!table_resize(table, table->nbuckets*(1<<table->growpower)))
            return -1;
    
    struct bucket *entry = table->edata;
    uint64_t hash = clip_hash(murmur((void*)key, wcslen(key) * 4, 0));
    entry->hash = hash;
    entry->dib = 1;
    void *eitem = bucket_item(entry);
    memcpy(eitem, &val, sizeof(mel_value_t));
    
    void *bitem;
    size_t i = entry->hash & table->mask;
    for (;;) {
        struct bucket *bucket = bucket_at(table, i);
        if (bucket->dib == 0) {
            memcpy(bucket, entry, table->bucketsz);
            table->count++;
            return 0;
        }
        bitem = bucket_item(bucket);
        if (entry->hash == bucket->hash) {
            memcpy(table->spare, bitem, sizeof(mel_value_t));
            memcpy(bitem, eitem, sizeof(mel_value_t));
            return 1;
        }
        if (bucket->dib < entry->dib) {
            memcpy(table->spare, bucket, table->bucketsz);
            memcpy(bucket, entry, table->bucketsz);
            memcpy(entry, table->spare, table->bucketsz);
            eitem = bucket_item(entry);
        }
        i = (i + 1) & table->mask;
        entry->dib += 1;
    }
}

mel_value_t* mel_table_get(mel_value_t obj, const wchar_t *key) {
    assert(mel_is_table(obj));
    mel_table_t *table = mel_as_table(obj);
    uint64_t hash = clip_hash(murmur((void*)key, wcslen(key) * 4, 0));
    size_t i = hash & table->mask;
    while(1) {
        struct bucket *bucket = bucket_at(table, i);
        if (!bucket->dib)
            return NULL;
        if (bucket->hash == hash)
            return bucket_item(bucket);
        i = (i + 1) & table->mask;
    }
    return NULL;
}

int mel_table_del(mel_value_t obj, const wchar_t *key) {
    assert(mel_is_table(obj));
    mel_table_t *table = mel_as_table(obj);
    uint64_t hash = clip_hash(murmur((void*)key, wcslen(key) * 4, 0));
    size_t i = hash & table->mask;
    while(1) {
        struct bucket *bucket = bucket_at(table, i);
        if (!bucket->dib)
            return 0;
        void *bitem = bucket_item(bucket);
        if (bucket->hash == hash) {
            memcpy(table->spare, bitem, sizeof(mel_value_t));
            bucket->dib = 0;
            while(1) {
                struct bucket *prev = bucket;
                i = (i + 1) & table->mask;
                bucket = bucket_at(table, i);
                if (bucket->dib <= 1) {
                    prev->dib = 0;
                    break;
                }
                memcpy(prev, bucket, table->bucketsz);
                prev->dib--;
            }
            table->count--;
            if (table->nbuckets > table->cap && table->count <= table->shrinkat) {
                // Ignore the return value. It's ok for the resize operation to
                // fail to allocate enough memory because a shrink operation
                // does not change the integrity of the data.
                table_resize(table, table->nbuckets/2);
            }
            return 1;
        }
        i = (i + 1) & table->mask;
    }
}

void mel_table_clear(mel_value_t obj) {
    assert(mel_is_table(obj));
    mel_table_t *table = mel_as_table(obj);
    table->count = 0;
    free_table_elements(table);
    void *new_buckets = malloc(table->bucketsz*table->cap);
    if (new_buckets) {
        free(table->buckets);
        table->buckets = new_buckets;
    }
    table->nbuckets = table->cap;
    memset(table->buckets, 0, table->bucketsz*table->nbuckets);
    table->mask = table->nbuckets-1;
    table->growat = table->nbuckets * (table->loadfactor / 100.0) ;
    table->shrinkat = table->nbuckets * SHRINK_AT;
}

int mel_table_count(mel_value_t obj) {
    assert(mel_is_table(obj));
    return (int)mel_as_table(obj)->count;
}
