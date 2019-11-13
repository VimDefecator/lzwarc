#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <pthread.h>
#include <linux/limits.h>

#include "lzw.h"
#include "huffman.h"
#include "futils.h"
#include "diter.h"
#include "queue.h"
#include "misc.h"

char *strusage =
    "usage:\n"
    "$ ./lzwarc [-p password] [-h] a <archive-name> <item1> ...\n"
    "$ ./lzwarc [-p password] x <archive-name> [<dest-path> [<item1> ...]]\n"
    "$ ./lzwarc l <arhive-name>\n";

enum { ALGO_LZW, ALGO_HUFFMAN };

int nthr;
int numcores();

void archive(char **ppath, char *key, char algo);
void extract(char **ppath, char *key);
void lstcont(char **ppath);

int main(int argc, char **argv)
{
    if (argc == 1) {
        fputs(strusage, stderr);
        return -1;
    }

    nthr = numcores();

    char *key = NULL;
    char algo = ALGO_LZW;

    while ((*++argv)[0] == '-') {
        switch ((*argv)[1]) {
        case 'p':
            key = *++argv;
            break;
        case 'h':
            algo = ALGO_HUFFMAN;
            break;
        }
    }

    switch ((*argv)[0]) {
    case 'a':
        archive(argv+1, key, algo);
        break;
    case 'x':
        extract(argv+1, key);
        break;
    case 'l':
        lstcont(argv+1);
        break;
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

void (*encoders[])(FILE*,FILE*) = { lzw_encode, huffman_encode },
     (*decoders[])(FILE*,FILE*) = { lzw_decode, huffman_decode },
     (*encode)(FILE*,FILE*),
     (*decode)(FILE*,FILE*);

enum { QUEUE, FARC, KEY, MUTEX, NARGS };

void *parchive(void *);

void archive(char **ppath, char *key, char algo)
{
    pthread_t thr[nthr];
    void *args[NARGS];
    void *queue;
    FILE *farc;
    pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

    queue = queue_new(100);
    farc = fopen(*ppath++, "wb");
    fwrite(&algo, 1, 1, farc);

    args[QUEUE] = queue;
    args[FARC]  = farc;
    args[KEY]   = key;
    args[MUTEX] = &mutex;

    encode = encoders[algo];

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
        encode(tfile, file);
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

enum { DFILE, TFILE };

void *pextract(void *);

void extract(char **ppath, char *key)
{
    pthread_t thr[nthr];
    void *queue;
    FILE *farc;
    char dpath[PATH_MAX], *_path;

    queue = queue_new(100);
    farc = fopen(*ppath++, "rb");
    decode = decoders[fgetc(farc)];

    for (int i = 0; i < nthr; ++i)
        pthread_create(&thr[i], NULL, pextract, queue);

    strcpy(dpath, *ppath ? *ppath : "");
    _path = dpath + strlen(dpath);
    if (*ppath) ++ppath;

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
                item[DFILE] = dfile;
                item[TFILE] = tfile;
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
        decode(item[DFILE], item[TFILE]);

        fclose(item[DFILE]);
        fclose(item[TFILE]);
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
    fgetc(farc);
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

    It *pit = *ppit;

    char *pref = ItName(*pit);

    for (; **ppit && !memcmp(ItName(**ppit), pref, lpref); ++*ppit)
    {
        char *sl = strchr(ItName(**ppit) + lpref, '/');
        if (!sl++) continue;

        fwrite(ind, 1, lind, stdout);
        int llpref = sl - (ItName(**ppit) + lpref);
        fwrite(ItName(**ppit) + lpref, 1, llpref, stdout);
        putchar('\n');
        rlstcont(ppit, lind + 2, lpref + llpref);
    }

    *ppit = pit;

    for (; **ppit && !memcmp(ItName(**ppit), pref, lpref); ++*ppit)
    {
        char *sl = strchr(ItName(**ppit) + lpref, '/');
        if (sl) continue;

        fwrite(ind, 1, lind, stdout);
        printf("%-*s - %8u > %8u\n",
               57 - lind,
               ItName(**ppit) + lpref,
               ItSz(**ppit),
               ItSz_(**ppit));
    }
}

int pitcmp(It *pit1, It *pit2)
{
    return strcmp(ItName(*pit1), ItName(*pit2));
}
