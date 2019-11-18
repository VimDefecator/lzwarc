#ifndef HTBL_H
#define HTBL_H

#ifndef HTBL_HASH
#define HTBL_HASH this->hash
#endif

#ifndef HTBL_EQUAL
#define HTBL_EQUAL this->equal
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct htbl {
    void **ptr;
    int    sz;
    int  (*hash)(void *, int);
    int  (*equal)(void *, void *);
    int    deleted;
} htbl_t;

static inline void htbl_init(htbl_t *this, int sz,
                             int (*hash)(void *, int),
                             int (*equal)(void *, void *))
{
    this->ptr = calloc(sz, sizeof(void *));
    this->sz = sz;
    this->hash = hash;
    this->equal = equal;
}

static inline void htbl_free(htbl_t *this, void (*item_free)(void *))
{
    if (item_free) {
        for (int i = 0; i < this->sz; ++i) {
            if (this->ptr[i] == NULL || this->ptr[i] == &this->deleted)
                continue;
            item_free(this->ptr[i]);
        }
    }
    free(this->ptr);
}

static inline void **htbl_find_(htbl_t *this, void *item)
{
    void **pdel = NULL;
    int i, j;

    for (i = HTBL_HASH(item, this->sz), j = 1;
         j < this->sz && this->ptr[i];
         i = (i+j)%this->sz, j += 2)
    {
        if (this->ptr[i] == &this->deleted)
            if (!pdel) pdel = &this->ptr[i]; else;
        else
            if (HTBL_EQUAL(this->ptr[i], item)) break;
    }

    return j >= this->sz ? pdel
                         : this->ptr[i] || !pdel ? &this->ptr[i]
                                                 : pdel;
}

static inline void *htbl_find(htbl_t *this, void *item)
{
    void **pptr = htbl_find_(this, item);
    return pptr && *pptr != &this->deleted ? *pptr
                                           : NULL;
}

static inline int htbl_add(htbl_t *this, void *item)
{
    void **pptr = htbl_find_(this, item);
    if (!pptr)
        return -1;
    if (*pptr == NULL || *pptr == &this->deleted)
        *pptr = item;
    return 0;
}

static inline void htbl_del(htbl_t *this, void *item)
{
    void **pptr = htbl_find_(this, item);
    if (pptr && *pptr) *pptr = &this->deleted;
}

#endif
