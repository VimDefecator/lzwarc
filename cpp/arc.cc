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
#include "pathtree.h"

extern "C" {
#include "../common/lzw.h"
#include "../common/huffman.h"
#include "../common/futils.h"
}

using namespace std;
namespace fs = filesystem;

const char *strusage =
    "usage:\n"
    "$ ./lzwarc [-p password] [-h] a <archive-name> <item1> ...\n"
    "$ ./lzwarc [-p password] x <archive-name> [<dest-path> [<item1> ...]]\n"
    "$ ./lzwarc l <arhive-name>\n";

enum { ALGO_LZW, ALGO_HUFFMAN };

int nthr;

void archive(char **ppath, char *key, char algo);
void extract(char **ppath, char *key);
void listContents(char **ppath);
void explore(char **ppath);

int main(int argc, char **argv)
{
    if (argc == 1) {
        fputs(strusage, stderr);
        return -1;
    }

    nthr = thread::hardware_concurrency();

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
        listContents(argv+1);
        break;
    case 'i':
        explore(argv+1);
        break;
    }
}

void (*encoders[])(FILE*,FILE*) = { lzw_encode, huffman_encode },
     (*decoders[])(FILE*,FILE*) = { lzw_decode, huffman_decode },
     (*encode)(FILE*,FILE*),
     (*decode)(FILE*,FILE*);

struct pathInfo {
    string path;
    int ltrim;
};

void doArchive(tqueue<pathInfo> &, FILE *, char *, mutex &);

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

    // create the threads and pass jobs via queue
    for (auto &t : threads) t = thread(doArchive, ref(queue), farc, key, ref(theMutex));

    for (; *ppath; ++ppath) {
        // full path is needed to open a file, but only subdirectories
        // hierarchy of every separately specified item should be preserved in
        // the resulting archive, so strings with prefix lengths are pushed

        char *sl = strrchr(*ppath, '/');
        int ltrim = sl ? sl + 1 - *ppath : 0;

        try {
            for(auto entry : fs::recursive_directory_iterator(*ppath)) {
                if (entry.is_regular_file()) {
                    queue.push({entry.path(), ltrim});
                    ++nfiles;
                }
            }
        } catch (...) {
            queue.push({string(*ppath), ltrim});
            ++nfiles;
        }
    }

    // tell the threads to terminate
    for (int i = 0; i < nthr; ++i) queue.push({"", -1});

    for (auto &t : threads) t.join();

    _szarc = ftell(farc);

    fclose(farc);

    printf("%d files added to archive\n"
           "total compressed size: %d bytes\n",
           nfiles, _szarc - szarc_);
}

void doArchive(tqueue<pathInfo> &queue, FILE *farc, char *key, mutex &theMutex)
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

void doExtract(tqueue<filePair> &);

void extract(char **ppath, char *key)
{
    thread threads[nthr];
    tqueue<filePair> queue;
    FILE *farc;

    farc = fopen(*ppath++, "rb");
    decode = decoders[fgetc(farc)];

    for (auto &t : threads) t = thread(doExtract, ref(queue));

    string dirPath(*ppath ? *ppath++ : "");
    char **p2xFirst, **p2xLast;
    p2xFirst = *ppath ? ppath+1 : ppath;
    for (p2xLast = p2xFirst; *p2xLast; ++p2xLast);

    for (char path[0x1000]; fgets0(path, farc), *path; )
    {
        uint32_t sz, sz_;
        fread(&sz, sizeof(uint32_t), 1, farc);
        fread(&sz_, sizeof(uint32_t), 1, farc);

        auto **p2xCur = find_if(
            p2xFirst, p2xLast,
            [path](auto *p2x){
                for (int i = 0; p2x[i]; ++i) {
                    if (p2x[i] != path[i]) return false;
                }
                return true;
            }
        );
        if (p2xCur != p2xLast || p2xFirst == p2xLast) {
            auto p2x = *p2xCur;
            char *sl = p2x ? strrchr(p2x, '/') : NULL;
            string fullPath = dirPath + (path + (sl ? sl+1 - p2x : 0));
            string dirsPath = fullPath.substr(0, fullPath.rfind('/'));

            FILE *fdst, *ftmp;
            fs::create_directories(dirsPath);
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

void doExtract(tqueue<filePair> &queue)
{
    for (filePair fp; fp = queue.pop(), fp.dst && fp.src; )
    {
        decode(fp.dst, fp.src);
        fclose(fp.dst);
        fclose(fp.src);
    }
}

pathtree<string, uint32_t> buildPathTree(const char *path);

void listContents(char **ppath)
{
    pathtree<string, uint32_t> tree = buildPathTree(*ppath);
    tree.print(cout);
}

void explore(pathtree<string, uint32_t> &tree)
{
    tree.print(cout, false, "| ");
    for (string item; item != ".."; ) {
        cout << "> ";
        getline(cin, item);
        if (item != "..") {
            try {
                explore(tree.getChild(item));
                tree.print(cout, false, "| ");
            } catch (...) {
                cout << "\"" << item << "\" not found. Maybe...\n";
                tree.iter([item](auto it){
                    if (it.substr(0, item.length()) == item) {
                        cout << "| " << it << endl;
                    }
                });
            }
        }
    }
}

void explore(char **ppath)
{
    pathtree<string, uint32_t> tree = buildPathTree(*ppath);
    explore(tree);
}

pathtree<string, uint32_t> buildPathTree(FILE *farc)
{
    pathtree<string, uint32_t> tree; fgetc(farc); 
    for (
        char name[0x1000] = "";
        fgets0(name, farc), *name;
    ) {
        uint32_t orsz, arsz;
        fread(&orsz, sizeof(uint32_t), 1, farc);
        fread(&arsz, sizeof(uint32_t), 1, farc);

        vector<string> path;
        for (
            char *tok = strtok(name, "/");
            tok;
            tok = strtok(NULL, "/")
        ) {
            path.push_back(tok);
        }
        tree.addPath(path, orsz);

        fseek(farc, arsz, SEEK_CUR);
    }

    rewind(farc);

    return tree;
}

pathtree<string, uint32_t> buildPathTree(const char *path)
{
    FILE *farc = fopen(path, "rb");
    pathtree<string, uint32_t> tree = buildPathTree(farc);
    fclose(farc);
    return tree;
}

