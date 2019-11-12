#ifndef MISC_H
#define MISC_H

#define SZREALLOC 0x400

typedef void ** Ls;

#define LsSize(this) (((int *)(this))[-1])

#define LsNew(this) ((this) = (int *)malloc(sizeof(int )) + 1)

#define LsAdd(this, item) {                                                 \
    if (LsSize(this) % SZREALLOC == 0) {                                    \
        (this) = (int *)realloc(&LsSize(this),                              \
                                sizeof(int)                                 \
                                + (LsSize(this)+SZREALLOC)*sizeof(void *))  \
                 + 1;                                                       \
    }                                                                       \
    ((Ls )(this))[LsSize(this)++] = (item);                                 \
}

#define LsFree(this) free(&LsSize(this))

typedef void * It;

#define ItSz(this)           (((uint32_t *)(this))[0])
#define ItSz_(this)          (((uint32_t *)(this))[1])
#define ItName(this) ((char *)((uint32_t *)(this) +2))

#define ItNew(this, name, sz, sz_) {                        \
    (this) = malloc(2*sizeof(uint32_t) + strlen(name)+1);   \
    ItSz(this) = (sz);                                      \
    ItSz_(this) = (sz_);                                    \
    strcpy(ItName(this), name);                             \
}

inline char *pstrstr_(char **pstr, char *str_)
{
    for (; *pstr; ++pstr)
    {
        int len = strlen(*pstr);
        if (!memcmp(*pstr, str_, len)
            && (  str_[len] == '\0'
               || str_[len] == '/')) break;
    }
    return *pstr;
}

#endif
