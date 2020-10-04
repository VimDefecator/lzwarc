#include <stdio.h>
#include <stdint.h>

#include "huffman.h"
#include "bitio.h"

#ifdef HUFFCHARSIZE
#define CHSIZ HUFFCHARSIZE
#else
#define CHSIZ 1
#endif

#define CHWID (CHSIZ * 8)
#define CHRNG (1 << CHWID)
#define MAXTS (CHRNG * 2)

#define ROOT 0
#define LEAF (CHRNG - 1)

typedef struct {
    uint32_t occ, ch, bit,
             par, br0, br1;
} node_t;

typedef struct {
    unsigned ch  :  CHWID;
    unsigned br0 :  CHWID + 4;
    unsigned br1 :  CHWID + 4;
} cnode_t;

#define PQUEUE_SZ MAXTS
#define PQUEUE_CMP(a, b) (((node_t *)(a))->occ < ((node_t *)(b))->occ)

#include "pqueue.h"

void writetree(node_t *tree, uint32_t nnod, FILE *file);

void huffman_encode(FILE *fdst, FILE *fsrc)
{
    _Thread_local static node_t tree[MAXTS] = {};

    for(uint32_t ch = 0;
        fread(&ch, CHSIZ, 1, fsrc);
        ++tree[ch].occ);

    uint32_t szfile = ftell(fsrc);
    rewind(fsrc);

    _Thread_local static uint32_t tmap[CHRNG];
    uint32_t nnod = 0;
    for (uint32_t ch = 0; ch < CHRNG; ++ch) {
        if (tree[ch].occ) {
            tree[nnod].occ = tree[ch].occ;
            tree[nnod].ch = ch;
            tree[nnod].br0 = tree[nnod].br1 = LEAF;
            tmap[ch] = nnod;
            ++nnod;
        }
    }

    pqueue_init();

    for (uint32_t i = 0; i < nnod; ++i)
        pqueue_push(tree+i);

    while (1) {
        node_t *nod0, *nod1;
        nod0 = pqueue_pop();
        nod1 = pqueue_pop();

        if (nod1 == NULL) {
            nod0->par = ROOT;
            break;
        }

        nod0->bit = 0;
        nod1->bit = 1;
        nod0->par = nod1->par = nnod;

        tree[nnod].br0 = nod0 - tree;
        tree[nnod].br1 = nod1 - tree;
        tree[nnod].occ = nod0->occ + nod1->occ;

        pqueue_push(tree+nnod);

        ++nnod;
    }

    fwrite(&szfile, sizeof(uint32_t), 1, fdst);
    writetree(tree, nnod, fdst);

    bitio_t bout = BITIO_INIT;
    bout.file = fdst;

    rewind(fsrc);

    for (uint32_t ch = 0; fread(&ch, CHSIZ, 1, fsrc); )
    {
        for (uint32_t i = tmap[ch]; tree[i].par != ROOT; )
        {
            uint64_t bits, nbits;

            for(bits = 0, nbits = 0;
                nbits < BIOBUFW &&
                tree[i].par != ROOT;
                bits = (bits << 1) | tree[i].bit,
                i = tree[i].par, ++nbits);

            bput(&bout, bits, nbits);
        }
    }
    bflush(&bout);
}

uint32_t readtree(cnode_t *tree, FILE *file);

void huffman_decode(FILE *fdst, FILE *fsrc)
{
    cnode_t tree[MAXTS];
    uint32_t nnod, outputlen;

    fread(&outputlen, sizeof(uint32_t), 1, fsrc);
    nnod = readtree(tree, fsrc);

    bitio_t bin = BITIO_INIT;
    bin.file = fsrc;

    while (outputlen--)
    {
        uint64_t i, bit;
        for(i = nnod-1;
            tree[i].br0 != LEAF &&
            -1 != bget(&bin, &bit, 1);
            i = bit ? tree[i].br1 : tree[i].br0);

        fputc(tree[i].ch, fdst);
    }
}

void writetree(node_t *tree, uint32_t nnod, FILE *file)
{
    fwrite(&nnod, sizeof(uint32_t), 1, file);

    for (uint32_t i = 0; i < nnod; ++i) {
        cnode_t cnode;
        cnode.ch  = tree[i].ch;
        cnode.br0 = tree[i].br0;
        cnode.br1 = tree[i].br1;
        fwrite(&cnode, sizeof(cnode_t), 1, file);
    }
}

uint32_t readtree(cnode_t *tree, FILE *file)
{
    uint32_t nnod;

    fread(&nnod, sizeof(uint32_t), 1, file);
    fread(tree, sizeof(cnode_t), nnod, file);

    return nnod;
}

