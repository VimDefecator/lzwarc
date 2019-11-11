#include "lzw.h"
#include "bitio.h"

#define HTBL_HASH(key, rng)     (*(uint32_t *)(key) % (rng))
#define HTBL_EQUAL(key1, key2)  (*(uint32_t *)(key1) == *(uint32_t *)(key2))
#include "htbl.h"

#define DICTSIZE 0x4000

enum { Prev, Suff };

typedef struct {
    int16_t (*ent)[2], nent;
    htbl_t htbl;
} dict_t;

inline void dict_add(dict_t *this, int prev, int suff)
{
    if (this->nent == DICTSIZE) return;

    this->ent[this->nent][Prev] = prev;
    this->ent[this->nent][Suff] = suff;
    htbl_add(&this->htbl, this->ent[this->nent]);

    ++this->nent;
}

inline int dict_find(dict_t *this, int prev, int suff)
{
    int16_t ent[2];
    ent[Prev] = prev;
    ent[Suff] = suff;

    int16_t (*pent)[2] = htbl_find(&this->htbl, ent);
    return pent ? pent - this->ent : -1;
}

inline void dict_init(dict_t *this)
{
    this->ent = calloc(DICTSIZE, 2 * sizeof(int16_t));
    this->nent = 0;
    htbl_init(&this->htbl, 33013, NULL, NULL);

    unsigned char ch = 0;
    do dict_add(this, -1, ch); while (++ch);
}

inline void dict_free(dict_t *this)
{
    htbl_free(&this->htbl, NULL);
    free(this->ent);
}

void lzw_encode(FILE *in, FILE *out)
{
    dict_t dict;
    dict_init(&dict);

    bitio_t bout = BITIO_INIT;
    bout.file = out;

    for (int prev, next, ch = 0, nbits = 9, stop = 0x100; ch != EOF; )
    {
        for(next = -1;
            prev = next,
            EOF != (ch = fgetc(in)) &&
             -1 != (next = dict_find(&dict, prev, ch)); );
        if (prev >= stop) {
            bput(&bout, stop, nbits);
            ++nbits;
            stop <<= 1;
        }
        bput(&bout, prev, nbits);
        dict_add(&dict, prev, ungetc(ch, in));
    }
    bflush(&bout);

    dict_free(&dict);
}

void lzw_decode(FILE *in, FILE *out)
{
    dict_t dict;
    dict_init(&dict);

    bitio_t bin = BITIO_INIT;
    bin.file = in;

    for(int prev = -1, suff, next, nbits = 9, stop = 0x100;
        -1 != bget(&bin, &next, nbits);
        prev = next)
    {
        if (next == stop) {
            ++nbits;
            stop <<= 1;
            next = prev;
            continue;
        }

        int add = next == dict.nent;
        if (add) dict_add(&dict, prev, suff);

        unsigned char buf[DICTSIZE];
        int pos = DICTSIZE, len = 0, curr = next;
        do buf[--pos] = dict.ent[curr][Suff], ++len;
            while(-1 != (curr = dict.ent[curr][Prev]));

        fwrite(buf+pos, 1, len, out);
        suff = buf[pos];
        if (!add && prev != -1)
            dict_add(&dict, prev, suff);
    }

    dict_free(&dict);
}
