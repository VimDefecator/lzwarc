#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

#include "bitio.h"
#include "lzw.h"

#define HTBL_SZ                 33013
#define HTBL_HASH(key, rng)     (*(uint32_t *)(key) % (rng))
#define HTBL_EQUAL(key1, key2)  (*(uint32_t *)(key1) == *(uint32_t *)(key2))
#include "htbl.h"

#define DICTSIZE 0x4000

enum { Prev, Suff };

_Thread_local struct {
    int16_t ent[DICTSIZE][2];
    int16_t nent;
} g_dict;

static inline void dict_add(int prev, int suff)
{
    if (g_dict.nent == DICTSIZE) return;

    g_dict.ent[g_dict.nent][Prev] = prev;
    g_dict.ent[g_dict.nent][Suff] = suff;
    htbl_add(g_dict.ent[g_dict.nent]);

    ++g_dict.nent;
}

static inline int dict_find(int prev, int suff)
{
    int16_t ent[2];
    ent[Prev] = prev;
    ent[Suff] = suff;

    int16_t (*pent)[2] = htbl_find(ent);
    return pent ? pent - g_dict.ent : -1;
}

static inline void dict_init()
{
    g_dict.nent = 0;
    htbl_init();

    unsigned char ch = 0;
    do dict_add(-1, ch); while (++ch);
}

void lzw_encode(FILE *fdst, FILE *fsrc)
{
    dict_init();

    binit(fdst, NULL);

    for (uint64_t prev, next, ch = 0, nbits = 9, stop = 0x100; ch != EOF; )
    {
        for(next = -1;
            prev = next,
            EOF != (ch = fgetc(fsrc)) &&
             -1 != (next = dict_find(prev, ch)); );
        if (prev >= stop) {
            bput(stop, nbits);
            ++nbits;
            stop <<= 1;
        }
        bput(prev, nbits);
        dict_add(prev, ungetc(ch, fsrc));
    }
    bflush();
}

void lzw_decode(FILE *fdst, FILE *fsrc)
{
    dict_init();

    binit(NULL, fsrc);

    for(uint64_t prev = -1, suff, next, nbits = 9, stop = 0x100;
        -1 != bget(&next, nbits);
        prev = next)
    {
        if (next == stop) {
            ++nbits;
            stop <<= 1;
            next = prev;
            continue;
        }

        int add = next == g_dict.nent;
        if (add) dict_add(prev, suff);

        unsigned char buf[DICTSIZE];
        int pos = DICTSIZE, len = 0, curr = next;
        do buf[--pos] = g_dict.ent[curr][Suff], ++len;
            while(-1 != (curr = g_dict.ent[curr][Prev]));

        fwrite(buf+pos, 1, len, fdst);
        suff = buf[pos];
        if (!add && prev != -1) dict_add(prev, suff);
    }
}

