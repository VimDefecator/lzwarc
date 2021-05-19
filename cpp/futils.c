#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>

#include "futils.h"

FILE *fopen_mkdir(const char *path, const char *mode)
{
    for (char *p = path+1; p = strchr(p, '/'); ++p) {
        *p = '\0';
        mkdir(path, 0777);
        *p = '/';
    }
    return fopen(path, mode);
}

void fcopy(FILE *dst, FILE *src, size_t nbytes)
{
    char buf[0x4000];

    while (nbytes > sizeof(buf))
        nbytes -= fwrite(buf, 1, fread(buf, 1, sizeof(buf), src), dst);
    while (nbytes)
        nbytes -= fwrite(buf, 1, fread(buf, 1, nbytes, src), dst);
}

void fxor(FILE *dst, FILE *src, size_t nbytes, char *key)
{
    if (key) {
        size_t klen = strlen(key);
        while (nbytes--) fputc(fgetc(src)^key[nbytes%klen], dst);
    } else {
        fcopy(dst, src, nbytes);
    }
}

void fputs0(const char *str, FILE *file)
{
    fputs(str, file);
    fputc('\0', file);
}

void fgets0(char *str, FILE *file)
{
    int ch;
    while (ch = fgetc(file), ch && ch != EOF)
        *str++ = ch;
    *str = '\0';
}

