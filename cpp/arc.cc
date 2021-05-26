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
    "$ ./lzwarc [-h] a <archive-name> <item1> ...\n"
    "$ ./lzwarc x <archive-name> [<dest-path> [<item1> ...]]\n"
    "$ ./lzwarc l <arhive-name>\n";

enum { ALGO_LZW, ALGO_HUFFMAN };

int nthr;

void archive(char **, char);
void extract(char **);
void listContents(char **);
void explore(char **);

int main(int argc, char **argv)
{
    if (argc == 1) {
        fputs(strusage, stderr);
        return -1;
    }

    nthr = thread::hardware_concurrency();

    char algo = ALGO_LZW;

    while ((*++argv)[0] == '-') {
        switch ((*argv)[1]) {
        case 'h':
            algo = ALGO_HUFFMAN;
            break;
        }
    }

    switch ((*argv)[0]) {
    case 'a':
        archive(argv+1, algo);
        break;
    case 'x':
        extract(argv+1);
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

void doArchive(FILE *, mutex &, tqueue<pathInfo> &);

void archive(char **ppath, char algo)
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
    for (auto &t : threads) {
        t = thread(doArchive, farc, ref(theMutex), ref(queue));
    }

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

    for (auto &t : threads) queue.push({"", -1});
    for (auto &t : threads) t.join();

    _szarc = ftell(farc);

    fclose(farc);

    printf("%d files added to archive\n"
           "total compressed size: %d bytes\n",
           nfiles, _szarc - szarc_);
}

void doArchive(FILE *farc, mutex &theMutex, tqueue<pathInfo> &queue)
{
    for (
        pathInfo pi;
        (pi = queue.pop()).ltrim != -1;
    ) {
        FILE *file, *ftmp;
        uint32_t orsz, arsz;

        file = fopen(pi.path.c_str(), "rb");
        if (!file) return;

        // send encoder output to temporary file and write it to archive only
        // if the size was succesfully reduced, otherwise copy the source one

        ftmp = tmpfile();
        encode(ftmp, file);
        orsz = ftell(file);
        arsz = ftell(ftmp);
        if (arsz < orsz) {
            fclose(file);
            file = ftmp;
        } else {
            fclose(ftmp);
            arsz = orsz;
        }
        rewind(file);

        theMutex.lock();

        fputs0(pi.path.c_str() + pi.ltrim, farc);
        fwrite(&orsz, sizeof(uint32_t), 1, farc);
        fwrite(&arsz, sizeof(uint32_t), 1, farc);
        fcopy(farc, file, arsz);

        theMutex.unlock();

        fclose(file);
    }
}

void createFolders(string);

struct filePair {
    FILE *dst, *src;
};

struct fileInfo {
    string path;
    uint32_t orsz, arsz;
};

fileInfo readFileInfo(FILE *);

void extract1(FILE *, fileInfo, tqueue<filePair> &);

void extract2(tqueue<filePair> &);

void extract(char **ppath)
{
    thread threads[nthr];
    tqueue<filePair> queue;
    FILE *farc;

    farc = fopen(*ppath++, "rb");
    decode = decoders[fgetc(farc)];

    for (auto &t : threads) t = thread(extract2, ref(queue));

    string dirPath(*ppath ? *ppath++ : "");
    char **p2xFirst, **p2xLast;
    p2xFirst = *ppath ? ppath+1 : ppath;
    for (p2xLast = p2xFirst; *p2xLast; ++p2xLast);

    for (
        fileInfo finfo;
        (finfo = readFileInfo(farc)).path != "";
    ) {
        auto **p2xCur = find_if(
            p2xFirst, p2xLast,
            [&](auto *p2x){
                return !memcmp(finfo.path.c_str(), p2x, strlen(p2x));
            }
        );
        if (p2xCur != p2xLast || p2xFirst == p2xLast) {
            finfo.path = dirPath + finfo.path;
            extract1(farc, finfo, queue);
        } else {
            fseek(farc, finfo.arsz, SEEK_CUR);
        }
    }

    for (auto &t : threads) queue.push({NULL, NULL});
    for (auto &t : threads) t.join();

    fclose(farc);
}

pathtree buildPathTree(FILE *);
pathtree buildPathTree(const char *);

void listContents(char **ppath)
{
    pathtree tree = buildPathTree(*ppath);
    tree.print(cout);
}

void explore(FILE *farc, pathtree &tree, tqueue<filePair> &queue)
{
    tree.print(cout, false, "| ");
    for (string command; command != ".."; ) {
        cout << "> ";
        getline(cin, command);
        if (command != "..") {
            char action = command[0];
            string path = command.substr(2);
            try {
                auto &child = tree.getChild(path);
                switch (action) {
                    case 'l': {
                        explore(farc, child, queue);
                        tree.print(cout, false, "| ");
                        break;
                    }
                    case 'x': {
                        child.iterFpos([=, &queue](auto fpos){
                            fseek(farc, fpos, SEEK_SET);
                            extract1(farc, readFileInfo(farc), queue);
                        });
                    }
                }
            } catch (...) {
                cout << "\"" << path << "\" not found. Maybe...\n";
                tree.iterKeys([&](auto it){
                    if (it.substr(0, path.length()) == path) {
                        cout << "| " << it << endl;
                    }
                });
            }
        }
    }
}

void explore(char **ppath)
{
    FILE *farc = fopen(*ppath, "rb");
    decode = decoders[fgetc(farc)];

    thread threads[nthr];
    tqueue<filePair> queue;

    for (auto &t : threads) t = thread(extract2, ref(queue));

    auto tree = buildPathTree(farc);
    explore(farc, tree, queue);

    for (auto &t : threads) queue.push({NULL, NULL});
    for (auto &t : threads) t.join();

    fclose(farc);
}

void extract1(FILE *farc, fileInfo finfo, tqueue<filePair> &queue)
{
    FILE *fdst, *ftmp;

    createFolders(finfo.path);
    fdst = fopen(finfo.path.c_str(), "wb");

    if (finfo.orsz != finfo.arsz) {
        ftmp = tmpfile();
        fcopy(ftmp, farc, finfo.arsz);
        rewind(ftmp);
        queue.push({fdst, ftmp});
    } else {
        fcopy(fdst, farc, finfo.arsz);
        fclose(fdst);
    }
}

void extract2(tqueue<filePair> &queue)
{
    for (filePair fp; fp = queue.pop(), fp.dst && fp.src; )
    {
        decode(fp.dst, fp.src);
        fclose(fp.dst);
        fclose(fp.src);
    }
}

pathtree buildPathTree(FILE *farc)
{
    pathtree tree;

    auto initfpos = ftell(farc);
    fseek(farc, 1, SEEK_SET);
    for (char name[0x1000]; ; )
    {
        uint32_t fpos, orsz, arsz;

        fpos = ftell(farc);
        fgets0(name, farc);
        if (name[0] == '\0') break;

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
        tree.addPath(path, fpos, orsz, arsz);

        fseek(farc, arsz, SEEK_CUR);
    }
    fseek(farc, initfpos, SEEK_SET);

    return tree;
}

pathtree buildPathTree(const char *path)
{
    FILE *farc = fopen(path, "rb");
    pathtree tree = buildPathTree(farc);
    fclose(farc);
    return tree;
}

void createFolders(string filePath)
{
    auto sl = filePath.rfind('/');
    if (sl != string::npos) {
        fs::create_directories(filePath.substr(0, sl));
    }
}

fileInfo readFileInfo(FILE *farc)
{
    char path[0x1000];
    uint32_t orsz, arsz;

    fgets0(path, farc);
    fread(&orsz, sizeof(uint32_t), 1, farc);
    fread(&arsz, sizeof(uint32_t), 1, farc);

    return {path, orsz, arsz};
}

