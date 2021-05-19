#include <iostream>
#include <string>
#include <vector>
#include <algorithm>
#include <thread>
#include <mutex>
#include <filesystem>
#include <cstdio>
#include <cstdlib>
#include <cstdint>
#include <cstring>
#include "tqueue.h"

extern "C" {
#include <unistd.h>
#include <termios.h>
#include <linux/limits.h>
#include "lzw.h"
#include "huffman.h"
#include "futils.h"
#include "misc.h"
}

using namespace std;
namespace fs = filesystem;

const char *strusage =
    "usage:\n"
    "$ ./lzwarc [-p password] [-h] a <archive-name> <item1> ...\n"
    "$ ./lzwarc [-p password] x <archive-name> [<dest-path> [<item1> ...]]\n"
    "$ ./lzwarc l <arhive-name>\n";

enum { ALGO_LZW, ALGO_HUFFMAN, ALGO_LZ8HUFFMAN };

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
            algo = (*argv)[2] ? ALGO_LZ8HUFFMAN : ALGO_HUFFMAN;
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

void lz8_huffman_decode(FILE *fdst, FILE *fsrc);
void lz8_huffman_encode(FILE *fdst, FILE *fsrc);

void (*encoders[])(FILE*,FILE*) = { lzw_encode, huffman_encode, lz8_huffman_encode },
     (*decoders[])(FILE*,FILE*) = { lzw_decode, huffman_decode, lz8_huffman_decode },
     (*encode)(FILE*,FILE*),
     (*decode)(FILE*,FILE*);

struct pathInfo {
    string path;
    int ltrim;
};

void parchive(tqueue<pathInfo> &, FILE *, char *, mutex &);

void archive(char **ppath, char *key, char algo)
{
    thread threads[nthr];
    mutex theMutex;
    tqueue<pathInfo> queue;
    FILE *farc;

    int szarc_, _szarc, nfiles = 0;

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

    encode = encoders[algo];

    // create multiple threads and feed them with jobs via queue
    for (auto &t : threads) t = thread(parchive, ref(queue), farc, key, ref(theMutex));

    char fpath[PATH_MAX];
    for (; *ppath; ++ppath) {
        // diter recursively returns paths to all underlying files, if a
        // directory is specified, or a single file - same as given

        // full path is needed to open a file, but only subdirectories
        // hierarchy of every separately specified item should be preserved in
        // the resulting archive, so strings with prefix lengths are pushed

        char *sl = strrchr(*ppath, '/');
        int ltrim = sl ? sl + 1 - *ppath : 0;

        for(auto entry : fs::recursive_directory_iterator(*ppath)) {
            if (entry.is_regular_file()) {
                strcpy(fpath, entry.path().c_str());
                
                queue.push({string(fpath), ltrim});
                ++nfiles;
            }
        }
    }

    // tell the threads to terminate by feeding each one with a NULL
    for (int i = 0; i < nthr; ++i) queue.push({"", -1});

    for (auto &t : threads) t.join();

    _szarc = ftell(farc);

    fclose(farc);

    printf("%d files added to archive\n"
           "total compressed size: %d bytes\n",
           nfiles, _szarc - szarc_);
}

void parchive(tqueue<pathInfo> &queue, FILE *farc, char *key, mutex &theMutex)
{
    for (pathInfo pi; (pi = queue.pop()).ltrim != -1; )
    {
        FILE *file, *ftmp;
        uint32_t sz, sz_;

        file = fopen(pi.path.c_str(), "rb");
        if (!file) continue;

        // send encoder output to temporary file and write it to archive only
        // if the size was succesfully reduced, otherwise copy the source one

        ftmp = tmpfile();
        encode(ftmp, file);
        sz = ftell(file);
        sz_ = ftell(ftmp);
        if (sz_ < sz) {
            fclose(file);
            file = ftmp;
        } else {
            fclose(ftmp);
            sz_ = sz;
        }
        rewind(file);

        theMutex.lock();

        fputs0(pi.path.c_str() + pi.ltrim, farc);
        fwrite(&sz, sizeof(uint32_t), 1, farc);
        fwrite(&sz_, sizeof(uint32_t), 1, farc);
        fxor(farc, file, sz_, key);
        // fxor calls fcopy, if key is NULL

        theMutex.unlock();

        fclose(file);
    }
}

struct filePair {
    FILE *dst, *src;
};

void pextract(tqueue<filePair> &);

void extract(char **ppath, char *key)
{
    thread threads[nthr];
    tqueue<filePair> queue;
    FILE *farc;

    farc = fopen(*ppath++, "rb");
    decode = decoders[fgetc(farc)];

    for (auto &t : threads) t = thread(pextract, ref(queue));

    string dirPath(*ppath ? *ppath : "");
    if (*ppath) ++ppath;


    FILE *files[2];
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
            string fullPath = dirPath + (path + (sl ? sl+1 - *_ppath : 0));

            FILE *fdst, *ftmp;
            fs::create_directories(fullPath.substr(0, fullPath.rfind('/')));
            fdst = fopen(fullPath.c_str(), "wb");
            if (sz_ < sz)
            {
                ftmp = tmpfile();
                fxor(ftmp, farc, sz_, key);
                rewind(ftmp);

                queue.push({fdst, ftmp});
            }
            else
            {
                fxor(fdst, farc, sz_, key);
                fclose(fdst);
            }
            // delegate only decoder-involved extracting to parallel threads
        }
        else fseek(farc, sz_, SEEK_CUR);
    }

    for (int i = 0; i < nthr; ++i) queue.push({NULL, NULL});

    for (auto &t : threads) t.join();

    fclose(farc);
}

void pextract(tqueue<filePair> &queue)
{
    for (filePair fp; fp = queue.pop(), fp.dst && fp.src; )
    {
        decode(fp.dst, fp.src);
        fclose(fp.dst);
        fclose(fp.src);
    }
}

void lz8_huffman_encode(FILE *fdst, FILE *fsrc)
{
    FILE *ftmp = tmpfile();
    lz8_encode(ftmp, fsrc);
    rewind(ftmp);
    huffman_encode(fdst, ftmp);
    fclose(ftmp);
}

void lz8_huffman_decode(FILE *fdst, FILE *fsrc)
{
    FILE *ftmp = tmpfile();
    huffman_decode(ftmp, fsrc);
    rewind(ftmp);
    lz8_decode(fdst, ftmp);
    fclose(ftmp);
}

// lstcont collects all paths from archive to memory, sorts lexicographically
// and invokes recursive subroutine, which goes through the list via common
// pointer-to-pointer, twice per segment on each level for directories-first
// ordering, and does all the indentation

struct fileInfo {
    string name;
    uint32_t orsz, arsz;
};

void rlstcont(
    vector<fileInfo>::iterator &iter,
    vector<fileInfo>::iterator iterEnd,
    int lind, int lpref
);

void lstcont(char **ppath)
{
    vector<fileInfo> ls;

    FILE *farc = fopen(*ppath, "rb");
    fgetc(farc);
    for (char name[PATH_MAX] = ""; fgets0(name, farc), *name; )
    {
        uint32_t orsz, arsz;
        fread(&orsz, sizeof(uint32_t), 1, farc);
        fread(&arsz, sizeof(uint32_t), 1, farc);

        ls.push_back({name, orsz, arsz});

        fseek(farc, arsz, SEEK_CUR);
    }
    fclose(farc);

    sort(ls.begin(), ls.end(),
         [](fileInfo fi1, fileInfo fi2){return fi1.name < fi2.name;});

    auto iter = ls.begin();
    rlstcont(iter, ls.end(), 0, 0);
}

void rlstcont(
    vector<fileInfo>::iterator &iter,
    vector<fileInfo>::iterator iterEnd,
    int lind, int lpref
) {
    static const char *ind = "| | | | | | | | | | | | | | | | | | | | | | | "
                             "| | | | | | | | | | | | | | | | | | | | | | | "
                             "| | | | | | | | | | | | | | | | | | | | | | | ";

    auto iterBegin = iter;
    auto nameBegin = iterBegin->name.c_str();
    for (; iter != iterEnd; ++iter)
    {
        const char *name, *sl;
        name = iter->name.c_str();
        if (memcmp(name, nameBegin, lpref)) break;
        sl = strchr(name + lpref, '/');
        if (!sl++) continue;

        fwrite(ind, 1, lind, stdout);
        int llpref = sl - (name + lpref);
        fwrite(name + lpref, 1, llpref, stdout);
        putchar('\n');
        rlstcont(iter, iterEnd, lind + 2, lpref + llpref);
    }

    iter = iterBegin;

    for (; iter != iterEnd; ++iter)
    {
        const char *name, *sl;
        name = iter->name.c_str();
        if (memcmp(name, nameBegin, lpref)) break;
        sl = strchr(name + lpref, '/');
        if (sl) continue; 

        fwrite(ind, 1, lind, stdout);
        printf("%-*s - %8u > %8u\n",
               57 - lind, name + lpref, iter->orsz, iter->arsz);
    }
}

