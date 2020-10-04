#ifndef PQUEUE_H
#define PQUEUE_H

#include <stdlib.h>

#define Swap(a,b) {     \
    typeof(a) _t = (a); \
    (a) = (b);          \
    (b) = _t;           \
}

_Thread_local void *g_pqueue[PQUEUE_SZ];
_Thread_local int g_pqueue_nitem;

static inline void pqueue_init()
{
    g_pqueue_nitem = 0;
}

static inline void pqueue_push(void *item)
{
    g_pqueue[++g_pqueue_nitem] = item;

    for (int i = g_pqueue_nitem; i/2 > 0; i /= 2) {
        if (PQUEUE_CMP(g_pqueue[i/2], g_pqueue[i])) break;
        Swap(g_pqueue[i], g_pqueue[i/2]);
    }
}

static inline void *pqueue_pop()
{
    if (g_pqueue_nitem == 0) return NULL;

    void *ret = g_pqueue[1];

    g_pqueue[1] = g_pqueue[g_pqueue_nitem];
    for (int i = 1, j; ; i = j) {
        if (i*2   < g_pqueue_nitem &&
            PQUEUE_CMP(g_pqueue[i*2  ], g_pqueue[i])) j = i*2;
        else
        if (i*2+1 < g_pqueue_nitem &&
            PQUEUE_CMP(g_pqueue[i*2+1], g_pqueue[i])) j = i*2+1;
        else break;

        Swap(g_pqueue[i], g_pqueue[j]);
    }
    --g_pqueue_nitem;

    return ret;
}

#endif

