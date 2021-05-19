#ifndef MISC_H
#define MISC_H

#include <stdlib.h>
#include <string.h>

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

#endif

