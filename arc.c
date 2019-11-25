#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <termios.h>
#include <linux/limits.h>

#include "lzw.h"
#include "huffman.h"
#include "futils.h"
#include "diter.h"
#include "queue.h"
#include "pathtree.h"
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

    int szarc_, _szarc, nfiles = 0;

    queue = queue_new(100);

    farc = fopen(*ppath, "rb");
    if (farc) {
        // arhive exists: detect previously used algo
        algo = fgetc(farc);
        fclose(farc);
        farc = fopen(*ppath, "ab");
    } else {
        // new archive: specify algo in first byte
        farc = fopen(*ppath, "wb");
        fwrite(&algo, 1, 1, farc);
    }
    ++ppath;

    szarc_ = ftell(farc);

    args[QUEUE] = queue;
    args[FARC]  = farc;
    args[KEY]   = key;
    args[MUTEX] = &mutex;

    encode = encoders[algo];

    // create multiple threads and feed them with jobs via queue
    for (int i = 0; i < nthr; ++i)
        pthread_create(&thr[i], NULL, parchive, args);

    for (; *ppath; ++ppath)
    {
        // diter recursively returns paths to all underlying files, if a
        // directory is specified, or a single file - same as given

        // full path is needed to open a file, but only subdirectories
        // hierarchy of every separately specified item should be preserved in
        // the resulting archive, so strings with prefix lengths are pushed

        char *sl = strrchr(*ppath, '/');
        sl = sl ? sl+1 : *ppath;

        void *diter = dopen(*ppath);
        for(char fpath[PATH_MAX];
            dnext(diter, fpath);
            ++nfiles)
        {
            It item;
            ItInit(item, fpath, sl-*ppath, 0);
            queue_put(queue, item);
        }
        dclose(diter);
    }

    // tell the threads to terminate by feeding each one with a NULL
    for (int i = 0; i < nthr; ++i)
        queue_put(queue, NULL);

    for (int i = 0; i < nthr; ++i)
        pthread_join(thr[i], NULL);

    _szarc = ftell(farc);

    fclose(farc);
    queue_free(queue);

    pthread_mutex_destroy(&mutex);

    printf("%d files added to archive\n"
           "total compressed size: %d bytes\n",
           nfiles, _szarc - szarc_);
}

void *parchive(void *args)
{
    void            *queue  = (void            *)((void **)args)[QUEUE];
    FILE            *farc   = (FILE            *)((void **)args)[FARC ];
    char            *key    = (char            *)((void **)args)[KEY  ];
    pthread_mutex_t *pmutex = (pthread_mutex_t *)((void **)args)[MUTEX];

    for (It item; item = queue_take(queue); ItFree(item))
    {
        FILE *file, *tfile;
        uint32_t sz, sz_;

        file = fopen(item, "rb");
        if (!file) continue;

        // send encoder output to temporary file and write it to archive only
        // if the size was succesfully reduced, otherwise copy the source one

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

        fputs0(item + ItInt1(item), farc);
        fwrite(&sz, sizeof(uint32_t), 1, farc);
        fwrite(&sz_, sizeof(uint32_t), 1, farc);
        fxor(farc, file, sz_, key);
        // fxor calls fcopy, if key is NULL

        pthread_mutex_unlock(pmutex);

        fclose(file);
    }
}

enum { DFILE, TFILE };

Ls interact(FILE *farc);

void *pextract(void *);

void extract(char **ppath, char *key)
{
    pthread_t thr[nthr];
    void *queue;
    FILE *farc;

    queue = queue_new(100);
    farc = fopen(*ppath++, "rb");
    decode = decoders[fgetc(farc)];

    for (int i = 0; i < nthr; ++i)
        pthread_create(&thr[i], NULL, pextract, queue);

    char dpath[PATH_MAX], *_path;
    strcpy(dpath, *ppath ? *ppath : "");
    _path = dpath + strlen(dpath);
    if (*ppath) ++ppath;

    if (*ppath && **ppath == '\0')
        ppath = interact(farc);

    if (ppath) for (char path[PATH_MAX], **_ppath; fgets0(path, farc), *path; )
    {
        uint32_t sz, sz_;
        fread(&sz, sizeof(uint32_t), 1, farc);
        fread(&sz_, sizeof(uint32_t), 1, farc);

        if (!*ppath || *(_ppath = pstrstr_(ppath, path)))
        {
            // fopen_mkdir opens new file under specified path,
            // creating all the missing directories along it

            char *sl = strrchr(*_ppath, '/');
            strcpy(_path, path + (sl ? sl+1 - *_ppath : 0));

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
            // delegate only decoder-involved extracting to parallel threads
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

// lstcont collects all paths from archive to memory, sorts lexicographically
// and invokes recursive subroutine, which goes through the list via common
// pointer-to-pointer, twice per segment on each level for directories-first
// ordering, and does all the indentation

int pstrcmp(char **, char **);

void rlstcont(It **, int, int);

void lstcont(char **ppath)
{
    Ls ls = LsNew;

    FILE *farc = fopen(*ppath, "rb");
    fgetc(farc);
    for (char name[PATH_MAX] = ""; fgets0(name, farc), *name; )
    {
        uint32_t sz, sz_;
        fread(&sz, sizeof(uint32_t), 1, farc);
        fread(&sz_, sizeof(uint32_t), 1, farc);

        It it;
        ItInit(it, name, sz, sz_);
        LsAdd(ls, it);

        fseek(farc, sz_, SEEK_CUR);
    }
    fclose(farc);

    qsort(ls, LsSize(ls), sizeof(*ls), pstrcmp);
    LsAdd(ls, NULL);

    It *pit = &ls[0];
    rlstcont(&pit, 0, 0);

    Apply(ItFree, ls, LsSize(ls)-1);
    LsFree(ls);
}

void rlstcont(It **ppit, int lind, int lpref)
{
    static const char *ind = "| | | | | | | | | | | | | | | | | | | | | | | "
                             "| | | | | | | | | | | | | | | | | | | | | | | "
                             "| | | | | | | | | | | | | | | | | | | | | | | ";

    It *pit = *ppit;

    char *pref = *pit;

    for (; **ppit && !memcmp(**ppit, pref, lpref); ++*ppit)
    {
        char *sl = strchr(**ppit + lpref, '/');
        if (!sl++) continue;

        fwrite(ind, 1, lind, stdout);
        int llpref = sl - (**ppit + lpref);
        fwrite(**ppit + lpref, 1, llpref, stdout);
        putchar('\n');
        rlstcont(ppit, lind + 2, lpref + llpref);
    }

    *ppit = pit;

    for (; **ppit && !memcmp(**ppit, pref, lpref); ++*ppit)
    {
        char *sl = strchr(**ppit + lpref, '/');
        if (sl) continue; 
        fwrite(ind, 1, lind, stdout);
        printf("%-*s - %8u > %8u\n",
               57 - lind,
               **ppit + lpref,
               ItInt1(**ppit),
               ItInt2(**ppit));
    }
}

int pstrcmp(char **pstr1, char **pstr2)
{
    return strcmp(*pstr1, *pstr2);
}

// visual selection of files to extract

Ls interact(FILE *farc)
{
    Ls paths = NULL;

    pathtree_t *tree = pathtree_new(".", NULL);

    for(char path[PATH_MAX];
        fgets0(path, farc), *path;
        pathtree_add(tree, path))
    {
        uint32_t sz, sz_;
        fread(&sz, sizeof(uint32_t), 1, farc);
        fread(&sz_, sizeof(uint32_t), 1, farc);
        fseek(farc, sz_, SEEK_CUR);
    }
    pathtree_sort(tree);

    pathtree_t *current = tree;
    int scroll = 0, cursor = 0;
    char path[PATH_MAX] = {};

    struct termios oldt, newt;

    tcgetattr(STDIN_FILENO, &oldt);
    newt = oldt;
    newt.c_lflag &= ~(ICANON);
    newt.c_lflag &= ~(ECHO);

    tcsetattr(STDIN_FILENO, TCSANOW, &newt);

    printf("W - up, S - down, D - open, A - back, X - select, SPACE - done\n\n"
           "\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n");

    #define DISPLAY {                                                         \
        printf("\e[21A");                                                     \
        int i;                                                                \
        for (i = scroll; i < scroll+20 && i < LsSize(current->child); ++i) {  \
            printf("%c %-76s%c\n",                                            \
                   i == cursor ? '>' : ' ',                                   \
                   current->child[i]->name,                                   \
                   current->child[i]->child ? '/' : ' ');                     \
        }                                                                     \
        for (; i < scroll+21; ++i) puts("\e[2K");                             \
    }

    DISPLAY;

    for (int key; ' ' != (key = getchar()); )
    {
        switch (key) {
        case 'W': case 'w':
            if (cursor-1 >= 0) {
                --cursor;
                if (cursor < scroll)
                    --scroll;
                DISPLAY;
            }
            break;
        case 'S': case 's':
            if (cursor+1 < LsSize(current->child)) {
                ++cursor;
                if (cursor >= scroll + 20)
                    ++scroll;
                DISPLAY;
            }
            break;
        case 'D': case 'd':
            if (current->child[cursor]->child) {
                current = current->child[cursor];
                scroll = cursor = 0;
                DISPLAY;
                strcat(path, current->name);
                strcat(path, "/");
            }
            break;
        case 'A': case 'a':
            if (current->parent) {
                path[strlen(path)-strlen(current->name)-1] = '\0';
                current = current->parent;
                scroll = cursor = 0;
                DISPLAY;
            }
            break;
        case 'X': case 'x':
            strcat(path, current->child[cursor]->name);
            LsAdd(paths, strcpy(malloc(strlen(path)+1), path));
            path[strlen(path)-strlen(current->child[cursor]->name)] = '\0';
            printf("\e[1Amarked\n");
            break;
        }
    }

    #undef DISPLAY

    tcsetattr(STDIN_FILENO, TCSANOW, &oldt);

    pathtree_free(tree);

    fseek(farc, 1, SEEK_SET);

    if (paths) LsAdd(paths, NULL);
    return paths;
}
