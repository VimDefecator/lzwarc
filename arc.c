#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <pthread.h>
#include "diter.h"
#include "futils.h"
#include "lzw.h"
#include "queue.h"
#include "misc.h"

#define USAGE {                                                               \
    fputs(                                                                    \
        "usage:\n"                                                            \
        "$ ./lzwarc a archivename item1 item2 ...\n"                          \
        "$ ./lzwarc ap password archivename item1 item2 ...\n"                \
        "$ ./lzwarc x archivename [dest_path/ [pref1 pref2 ...]]\n"           \
        "$ ./lzwarc xp password archivename [dest_path/ [pref1 pref2 ...]]\n" \
        "$ ./lzwarc l archivename\n",                                         \
        stderr);                                                              \
    return 1;                                                                 \
}

int nthr;

int numcores();

void archive(char **ppath, char *key);
void extract(char **ppath, char *key);
void lstcont(char **ppath);

int main(int argc, char **argv)
{
    nthr = numcores();

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

int numcores()
{
    FILE *fcpuinfo;
    char line[0x1000];
    
    fcpuinfo = fopen("/proc/cpuinfo", "r");
    do fgets(line, sizeof(line), fcpuinfo);
        while (!strstr(line, "cpu cores"));
    fclose(fcpuinfo);

    int ncores;
    sscanf(strchr(line, ':') + 1, "%d", &ncores);
    return ncores;
}

enum { QUEUE, FARC, KEY, MUTEX, NARGS };

void *parchive(void *);

void archive(char **ppath, char *key)
{
    pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

    void *args[NARGS];
    args[QUEUE] = queue_new(100);
    args[FARC]  = fopen(*ppath++, "ab");
    args[KEY]   = key;
    args[MUTEX] = &mutex;

    pthread_t thr[nthr];
    for (int i = 0; i < nthr; ++i)
        pthread_create(&thr[i], NULL, parchive, args);

    for (; *ppath; ++ppath)
    {
        void *diter = dopen(*ppath);

        for(char fpath[PATH_MAX];
            dnext(diter, fpath);
            queue_put(args[QUEUE], strcpy(malloc(strlen(fpath)+1), fpath)));

        dclose(diter);
    }

    for (int i = 0; i < nthr; ++i)
        queue_put(args[QUEUE], NULL);

    for (int i = 0; i < nthr; ++i)
        pthread_join(thr[i], NULL);

    fclose(args[FARC]);
    queue_free(args[QUEUE]);

    pthread_mutex_destroy(&mutex);
}

void *parchive(void *args)
{
    void            *queue  = (void            *)((void **)args)[QUEUE];
    FILE            *farc   = (FILE            *)((void **)args)[FARC ];
    char            *key    = (char            *)((void **)args)[KEY  ];
    pthread_mutex_t *pmutex = (pthread_mutex_t *)((void **)args)[MUTEX];

    for (char *fpath; fpath = queue_take(queue); )
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

        pthread_mutex_lock(pmutex);

        fputs0(fpath, farc);
        fwrite(&sz, sizeof(uint32_t), 1, farc);
        fwrite(&sz_, sizeof(uint32_t), 1, farc);
        fxor(farc, file, sz_, key);

        pthread_mutex_unlock(pmutex);

        fclose(file);
    }
}

enum { TFILE, DFILE };

void *pextract(void *);

void extract(char **ppath, char *key)
{
    FILE *farc = fopen(*ppath++, "rb");

    char dpath[PATH_MAX], *_path;
    strcpy(dpath, *ppath ? *ppath : "");
    _path = dpath + strlen(dpath);

    if (*ppath) ++ppath;

    void *queue = queue_new(100);

    pthread_t thr[nthr];
    for (int i = 0; i < nthr; ++i)
        pthread_create(&thr[i], NULL, pextract, queue);

    while (fgets0(_path, farc), *_path)
    {
        uint32_t sz, sz_;
        fread(&sz, sizeof(uint32_t), 1, farc);
        fread(&sz_, sizeof(uint32_t), 1, farc);

        if (!*ppath || pstrstr_(ppath, _path))
        {
            FILE *dfile, *tfile;
            dfile = fopen_mkdir(dpath, "wb");
            if (sz_ < sz)
            {
                tfile = tmpfile();
                fxor(tfile, farc, sz_, key);
                rewind(tfile);

                void **item = malloc(2 * sizeof(void *));
                item[TFILE] = tfile;
                item[DFILE] = dfile;
                queue_put(queue, item);
            }
            else
            {
                fxor(dfile, farc, sz_, key);
                fclose(dfile);
            }
        }
        else fseek(farc, sz_, SEEK_CUR);
    }

    for (int i = 0; i < nthr; ++i)
        queue_put(queue, NULL);

    for (int i = 0; i < nthr; ++i)
        pthread_join(thr[i], NULL);

    queue_free(queue);
    fclose(farc);
}

void *pextract(void *queue)
{
    for (void **item; item = queue_take(queue); )
    {
        lzw_decode(item[TFILE], item[DFILE]);

        fclose(item[TFILE]);
        fclose(item[DFILE]);
        free(item);
    }
}

int pitcmp(It *, It *);

void rlstcont(It **, int, int);

void lstcont(char **ppath)
{
    Ls ls;
    LsNew(ls);

    FILE *farc = fopen(*ppath, "rb");
    for (char name[PATH_MAX] = ""; fgets0(name, farc), *name; )
    {
        uint32_t sz, sz_;
        fread(&sz, sizeof(uint32_t), 1, farc);
        fread(&sz_, sizeof(uint32_t), 1, farc);

        It it;
        ItNew(it, name, sz, sz_);
        LsAdd(ls, it);

        fseek(farc, sz_, SEEK_CUR);
    }
    fclose(farc);

    qsort(ls, LsSize(ls), sizeof(*ls), pitcmp);
    LsAdd(ls, NULL);

    It *pit = &ls[0];
    rlstcont(&pit, 0, 0);

    for (int i = 0; i < LsSize(ls) - 1; ++i) free(ls[i]);
    LsFree(ls);
}

void rlstcont(It **ppit, int lind, int lpref)
{
    static const char *ind = "| | | | | | | | | | | | | | | | | | | | | | | "
                             "| | | | | | | | | | | | | | | | | | | | | | | "
                             "| | | | | | | | | | | | | | | | | | | | | | | ";

    char *pref = ItName(**ppit);

    for (; **ppit && !memcmp(ItName(**ppit), pref, lpref); ++*ppit)
    {
        fwrite(ind, 1, lind, stdout);

        char *sl = strchr(ItName(**ppit) + lpref, '/');
        if (sl++) {
            int llpref = sl - (ItName(**ppit) + lpref);
            fwrite(ItName(**ppit) + lpref, 1, llpref, stdout);
            putchar('\n');
            rlstcont(ppit, lind + 2, lpref + llpref);
        } else {
            printf("%-*s - %8u > %8u\n",
                   57 - lind,
                   ItName(**ppit) + lpref,
                   ItSz(**ppit),
                   ItSz_(**ppit));
        }
    }
}

int pitcmp(It *pit1, It *pit2)
{
    char *s1 = ItName(*pit1),
         *s2 = ItName(*pit2),
         *sl1, *sl2;

    sl1 = strrchr(s1, '/');
    sl1 = sl1 ? sl1+1 : s1+1;
    sl2 = strrchr(s2, '/');
    sl2 = sl2 ? sl2+1 : s2+1;

    if (sl1 - s1 != sl2 - s2) {
        int ret = -1;
        if (sl1 - s1 < sl2 - s2) {
            Swap(s1, s2);
            Swap(sl1, sl2);
            ret = 1;
        }
        if (!memcmp(s1, s2, sl2 - s2))
            return ret;
    }
    return strcmp(ItName(*pit1), ItName(*pit2));
}
