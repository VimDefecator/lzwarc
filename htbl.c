#include "htbl.h"

typedef struct htbl {
    void **ptr;
    int    sz;
    int  (*hash)(void *, int);
    int  (*cmp)(void *, void *);
    int    deleted;
} htbl_t;

void *htbl_new(int sz, int (*hash)(void *, int), int (*cmp)(void *, void *))
{
    htbl_t *this = calloc(1, sizeof(htbl_t));

    this->ptr = calloc(sz, sizeof(void *));
    this->sz = sz;
    this->hash = hash;
    this->cmp = cmp;

    return this;
}

void htbl_free(void *_this, void (*item_free)(void *))
{
    htbl_t *this = _this;

    if (item_free) {
        for (int i = 0; i < this->sz; ++i) {
            if (this->ptr[i] == NULL || this->ptr[i] == &this->deleted)
                continue;
            item_free(this->ptr[i]);
        }
    }
    free(this->ptr);
    free(this);
}

void **htbl_find_(void *_this, void *item)
{
    htbl_t *this = _this;
    int h, i, j;
    void **pdel = NULL;

    for (i = this->hash(item, this->sz), h = i ? i : 1, j = this->sz;
         j && this->ptr[i];
         --j, i = (i+h) % this->sz)
    {
        if (this->ptr[i] == &this->deleted)
            if (!pdel) pdel = &this->ptr[i]; else;
        else
            if (!this->cmp(this->ptr[i], item)) break;
    }

    return !j ? pdel
              : this->ptr[i] || !pdel ? &this->ptr[i]
                                      : pdel;
}

void *htbl_find(void *_this, void *item)
{
    htbl_t *this = _this;

    void **pptr = htbl_find_(this, item);
    return pptr && *pptr != &this->deleted ? *pptr
                                           : NULL;
}

int htbl_add(void *_this, void *item)
{
    htbl_t *this = _this;

    void **pptr = htbl_find_(this, item);
    if (!pptr)
        return -1;
    if (*pptr == NULL || *pptr == &this->deleted)
        *pptr = item;
    return 0;
}

void htbl_del(void *_this, void *item)
{
    htbl_t *this = _this;

    void **pptr = htbl_find_(_this, item);
    if (pptr && *pptr) *pptr = &this->deleted;
}

int memhash(const char *ptr, int len, int rng)
{
    int ret = 0;
    for (int rem = 1; len--; ++ptr) {
        ret = (ret + rem * (unsigned char )*ptr) % rng;
        rem = rem * 0x100 % rng;
    }
    return ret;
}

int strhash(const char *str, int rng)
{
    return memhash(str, strlen(str), rng);
}
