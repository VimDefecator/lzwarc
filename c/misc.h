#ifndef MISC_H
#define MISC_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>

#ifndef SZREALLOC
#define SZREALLOC 0x40
#endif

typedef void ** Ls;

#define LsNew ((int *)calloc(1, sizeof(int )) + 1)

#define LsSize(this) (((int *)(this))[-1])

#define LsAdd(this, item) {                                         \
    if ((this) == NULL) (this) = LsNew;                             \
    if (LsSize(this) % SZREALLOC == 0) {                            \
        (this) = (int *)realloc(                                    \
            &LsSize(this),                                          \
            sizeof(int) + (LsSize(this)+SZREALLOC)*sizeof(void *)   \
        ) + 1;                                                      \
    }                                                               \
    ((Ls )(this))[LsSize(this)++] = (item);                         \
}

#define LsFind(this, item, equal, idx) {                    \
    (idx) = -1;                                             \
    if (this) for (int _i = 0; _i < LsSize(this); ++_i) {   \
        if (equal(((Ls )(this))[_i], item)) {               \
            (idx) = _i;                                     \
            break;                                          \
        }                                                   \
    }                                                       \
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

#define Apply(func, arr, n) {           \
    for (int _i = 0; _i < (n); ++_i)    \
        func((arr)[_i]);                \
}

static inline char **pstrstr_(char **pstr, char *str_)
{
    for (; *pstr; ++pstr)
    {
        int len = strlen(*pstr);
        if (0 == memcmp(*pstr, str_, len)
            && (  str_[len] == '\0'
               || str_[len] == '/')) break;
    }
    return pstr;
}

static inline FILE *fopen_mkdir(char *path, char *mode)
{
    for (char *p = path+1; p = strchr(p, '/'); ++p) {
        *p = '\0';
        mkdir(path, 0777);
        *p = '/';
    }
    return fopen(path, mode);
}

#endif

