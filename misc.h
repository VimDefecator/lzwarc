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

typedef char * It;

#define ItInt1(this) (((int *)(this))[-1])
#define ItInt2(this) (((int *)(this))[-2])

#define ItInit(this, str, i1, i2) {                             \
    (this) = (int *)malloc(2*sizeof(int) + strlen(str)+1) + 2;  \
    ItInt1(this) = (i1);                                        \
    ItInt2(this) = (i2);                                        \
    strcpy(this, str);                                          \
}

#define ItFree(this) free(&ItInt2(this))

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
