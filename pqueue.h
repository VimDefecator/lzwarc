#ifndef PQUEUE_H
#define PQUEUE_H

#include <stdlib.h>

#define Swap(a,b) {     \
    typeof(a) _t = (a); \
    (a) = (b);          \
    (b) = _t;           \
}

typedef void ** PQ;

#define PQinit(this) {                                                      \
    (this) = (size_t *)malloc(2*sizeof(size_t) + 0x200*sizeof(void *)) + 2; \
    PQsize(this) = 0x200;                                                   \
    PQnitem(this) = 0;                                                      \
}

#define PQsize(this) (((size_t *)(this))[-2])

#define PQnitem(this) (((size_t *)(this))[-1])

#define PQpush(this, item, higher)                          \
{                                                           \
    if (++PQnitem(this) == PQsize(this)) {                  \
        (this) = (size_t *)realloc(                         \
            &PQsize(this),                                  \
           sizeof(size_t)*2 + sizeof(void *)*2*PQsize(this) \
        ) + 2;                                              \
    }                                                       \
    (this)[PQnitem(this)] = (item);                         \
                                                            \
    for (int _i = PQnitem(this); _i/2 > 0; _i /= 2) {       \
        if (higher((this)[_i/2], (this)[_i])) break;        \
        Swap((this)[_i], (this)[_i/2]);                     \
    }                                                       \
}

#define PQpop(this, item, higher) if (PQnitem(this) != 0)       \
{                                                               \
    (item) = (this)[1];                                         \
    (this)[1] = (this)[PQnitem(this)];                          \
                                                                \
    for (int _i = 1, _j; ; _i = _j)                             \
    {                                                           \
        if (_i*2   < PQnitem(this) &&                           \
            higher((this)[_i*2  ], (this)[_i])) _j = _i*2;      \
        else                                                    \
        if (_i*2+1 < PQnitem(this) &&                           \
            higher((this)[_i*2+1], (this)[_i])) _j = _i*2+1;    \
        else break;                                             \
                                                                \
        Swap((this)[_i], (this)[_j]);                           \
    }                                                           \
    --PQnitem(this);                                            \
}                                                               \
else (item) = NULL;

#define PQfree(this) (free(&PQsize(this)))

#endif

