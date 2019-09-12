#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include "diter.h"
#include "futils.h"
#include "lzw.h"

#define USAGE {                                                               \
    fputs(                                                                    \
        "usage:\n"                                                            \
        "$ ./lzwarc a archivename item1 item2 ...\n"                          \
        "$ ./lzwarc ap password archivename item1 item2 ...\n"                \
        "$ ./lzwarc x archivename [dest_path/ [pref1 pref2 ...]]\n"           \
        "$ ./lzwarc xp password archivename [dest_path/ [pref1 pref2 ...]]\n" \
        "$ ./lzwarc l archivename [pref1 pref2 ...]\n",                       \
        stderr);                                                              \
    return 1;                                                                 \
}

void archive(char **ppath, char *key);
void extract(char **ppath, char *key);
void lstcont(char **ppath);

int main(int argc, char **argv)
{
    if (argc < 3) USAGE;

    switch (argv[1][0]) {
        case 'a':
            if (argv[1][1] == 'p') {
                if (argc < 5) USAGE;
                archive(argv+3, argv[2]);
                break;
            } else {
                if (argc < 4) USAGE;
                archive(argv+2, NULL);
                break;
            }
        case 'x':
            if (argv[1][1] == 'p') {
                if (argc < 4) USAGE;
                extract(argv+3, argv[2]);
                break;
            } else {
                extract(argv+2, NULL);
                break;
            }
        case 'l':
            lstcont(argv+2);
            break;
        default:
            USAGE;
    }
}

void archive(char **ppath, char *key)
{
    FILE *farc = fopen(*ppath++, "ab");

    for (; *ppath; ++ppath)
    {
        void *diter = dopen(*ppath);

        for (char fpath[PATH_MAX]; dnext(diter, fpath); )
        {
            FILE *file, *tfile;
            uint32_t sz, sz_;

            file = fopen(fpath, "rb");
            if (!file) continue;

            tfile = tmpfile();
            lzw_encode(file, tfile);
            sz = ftell(file);
            sz_ = ftell(tfile);
            if (sz_ < sz) {
                fclose(file);
                file = tfile;
            } else {
                fclose(tfile);
                sz_ = sz;
            }
            rewind(file);

            fputs0(fpath, farc);
            fwrite(&sz, sizeof(uint32_t), 1, farc);
            fwrite(&sz_, sizeof(uint32_t), 1, farc);
            fxor(farc, file, sz_, key);

            fclose(file);
        }
        dclose(diter);
    }
    fclose(farc);
}

void extract(char **ppath, char *key)
{
    FILE *farc = fopen(*ppath++, "rb");

    char dpath[PATH_MAX], *_path;
    strcpy(dpath, *ppath ? *ppath : "");
    _path = dpath + strlen(dpath);

    if (*ppath) ++ppath;

    while (fgets0(_path, farc), *_path)
    {
        uint32_t sz, sz_;
        fread(&sz, sizeof(uint32_t), 1, farc);
        fread(&sz_, sizeof(uint32_t), 1, farc);

        char **pp;
        for (pp = ppath; *pp && memcmp(_path, *pp, strlen(*pp)); ++pp);
        if (*pp || !*ppath)
        {
            FILE *file, *tfile;
            file = fopen_mkdir(dpath, "wb");
            if (sz_ < sz) {
                tfile = tmpfile();
                fxor(tfile, farc, sz_, key);
                rewind(tfile);
                lzw_decode(tfile, file);
                fclose(tfile);
            } else {
                fxor(file, farc, sz_, key);
            }
            fclose(file);
        }
        else
        {
            fseek(farc, sz_, SEEK_CUR);
        }
    }
    fclose(farc);
}

void lstcont(char **ppath)
{
    printf("original size | arhived size | path/filename\n"
           "--------------|--------------|--------------\n");

    FILE *farc = fopen(*ppath++, "rb");
    for (char _path[PATH_MAX] = ""; fgets0(_path, farc), *_path; )
    {
        uint32_t sz, sz_;
        fread(&sz, sizeof(uint32_t), 1, farc);
        fread(&sz_, sizeof(uint32_t), 1, farc);

        char **pp;
        for (pp = ppath; *pp && memcmp(_path, *pp, strlen(*pp)); ++pp);
        if (*pp || !*ppath)
            printf(" %12u | %12u | %s\n", sz, sz_, _path);
        fseek(farc, sz_, SEEK_CUR);
    }
    fclose(farc);
}
