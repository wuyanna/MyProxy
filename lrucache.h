#include <stdio.h>
#include "csapp.h"

struct entry {
    char *key;
    void *data;
    size_t  size;
    struct entry *next;
};

typedef struct entry entry;

void initialize_cache(size_t maxcache, size_t maxobj);
void cache_put(char *key, void *data, size_t size);
void *cache_get(char *key, size_t *size);
void print_cachelist();