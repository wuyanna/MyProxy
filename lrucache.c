#include "lrucache.h"

size_t max_cache_size;
size_t max_object_size;
size_t used_size = 0;

/* cache table pointers */
static entry *cache_list = NULL;
static entry *last;

/* locks */
static sem_t mutex_get;
static sem_t mutex_put;

/* 
 * initialize_cache - initialize the cache with max cache size and max object size) 
 */
void initialize_cache(size_t maxcache, size_t maxobj)
{
    max_cache_size = maxcache;
    max_object_size = maxobj;
    if (cache_list == NULL) {
        cache_list = Malloc(sizeof(entry));
    }
    last = cache_list;
}

/* 
 * cache_put - put the data into cache table 
 */
void cache_put(char *key, void *data, size_t size)
{
    // if size exceeds max object size, do nothing
    if (size > max_object_size) {
        return;
    }
    
    Sem_init(&mutex_put, 0, 1);
    P(&mutex_put);
    // add the new entry to the last node of linked list
    entry *p = Malloc(sizeof(entry));
    p->key = Malloc(strlen(key));
    strcpy(p->key,key);
    p->data = data;
    p->size = size;
    p->next = NULL;
    used_size += size;
    last->next = p;
    last = p;
    
    // evict lru data
    while (used_size > max_cache_size) {
        entry *np = cache_list->next->next;
        used_size -= cache_list->next->size;
        Free(cache_list->next->data);
        Free(cache_list->next->key);
        Free(cache_list->next);
        cache_list->next = np;
    }
    V(&mutex_put);
}

/* 
 * cache_get - get data from cache 
 */
void *cache_get(char *key, size_t *size)
{
    entry *p;
    entry *prev;
    void *result = NULL;
    Sem_init(&mutex_get, 0, 1);
    P(&mutex_get);
    for (prev = cache_list, p = cache_list->next; p != NULL; prev = p, p = p->next) {
        if (!strcasecmp(p->key,key)) {
            // put the accessed data to the last node of linked list
            if (p != last) {
                prev->next = p->next;
                last->next = p;
                last = p;
                p->next = NULL;
            }
            *size = p->size;
            result = p->data;
        }
    }
    V(&mutex_get);
    return result;
}

/* 
 * print_cachelist - print the cache table 
 */
void print_cachelist()
{
    entry *p;
    printf("==Cache Used size: %ldbytes out of %ldbytes==\n",used_size,max_cache_size);
    for (p = cache_list; p != NULL; p = p->next) {
        printf("--%s--Size:%ldbytes--\n",p->key,p->size);
    }
}