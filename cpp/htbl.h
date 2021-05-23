#ifndef HTBL_H
#define HTBL_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

_Thread_local struct {
    void  *item[HTBL_SZ];
    int    deleted;
} g_htbl;

static inline void htbl_init() {
    memset(g_htbl.item, 0, sizeof(g_htbl.item));
}

static inline void **htbl_find_(
    void *item
) {
    void **pdel = NULL;
    int i, j;

    for (i = HTBL_HASH(item, HTBL_SZ), j = 1;
         j < HTBL_SZ && g_htbl.item[i];
         i = (i+j)%HTBL_SZ, j += 2)
    {
        if (g_htbl.item[i] == &g_htbl.deleted)
            if (!pdel) pdel = &g_htbl.item[i]; else;
        else
            if (HTBL_EQUAL(g_htbl.item[i], item)) break;
    }

    return j >= HTBL_SZ
           ? pdel
           : g_htbl.item[i] || !pdel
             ? &g_htbl.item[i]
             : pdel;
}

static inline void *htbl_find(
    void *item
) {
    void **pptr = htbl_find_(item);
    return pptr && *pptr != &g_htbl.deleted ? *pptr
                                            : NULL;
}

static inline int htbl_add(
    void *item
) {
    void **pptr = htbl_find_(item);
    if (!pptr)
        return -1;
    if (*pptr == NULL || *pptr == &g_htbl.deleted)
        *pptr = item;
    return 0;
}

static inline void htbl_del(
    void *item
) {
    void **pptr = htbl_find_(item);
    if (pptr && *pptr) *pptr = &g_htbl.deleted;
}

#endif

