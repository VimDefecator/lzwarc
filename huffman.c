#include <stdio.h>
#include <stdint.h>

#include "pqueue.h"
#include "bitio.h"
#include "huffman.h"

#define ROOT 0
#define LEAF 0x1ff

typedef struct {
    uint32_t occ;
    uint8_t  ch;
    uint8_t  bit;
    uint16_t par, br0, br1;
} node_t;

typedef struct {
    unsigned ch  :  8;
    unsigned br0 :  12;
    unsigned br1 :  12;
} cnode_t;

#define nodehigher(n1, n2) (((node_t *)(n1))->occ < ((node_t *)(n2))->occ)

void writetree(node_t *tree, uint16_t nnod, FILE *file);

void huffman_encode(FILE *fdst, FILE *fsrc)
{
    node_t tree[0x200] = {};
    for (int ch; EOF != (ch = fgetc(fsrc)); )
        ++tree[ch].occ;

    uint32_t szfile = ftell(fsrc);
    rewind(fsrc);

    uint16_t tmap[0x100], nnod = 0;
    for (int ch = 0; ch < 0x100; ++ch) {
        if (tree[ch].occ) {
            tree[nnod].occ = tree[ch].occ;
            tree[nnod].ch = ch;
            tree[nnod].br0 = tree[nnod].br1 = LEAF;
            tmap[ch] = nnod;
            ++nnod;
        }
    }

    PQ pq;
    PQinit(pq);

    for (int i = 0; i < nnod; ++i)
        PQpush(pq, tree+i, nodehigher);

    while (1) {
        node_t *nod0, *nod1;
        PQpop(pq, nod0, nodehigher);
        PQpop(pq, nod1, nodehigher);

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

        PQpush(pq, tree+nnod, nodehigher)

        ++nnod;
    }
    PQfree(pq);

    fwrite(&szfile, sizeof(uint32_t), 1, fdst);
    writetree(tree, nnod, fdst);

    bitio_t bout = BITIO_INIT;
    bout.file = fdst;

    rewind(fsrc);

    for (int ch; EOF != (ch = fgetc(fsrc)); )
    {
        for (int i = tmap[ch]; tree[i].par != ROOT; )
        {
            uint64_t bits, nbits;

            for(bits = 0, nbits = 0;
                nbits < UINTWID &&
                tree[i].par != ROOT;
                bits = (bits << 1) | tree[i].bit,
                i = tree[i].par, ++nbits);

            bput(&bout, bits, nbits);
        }
    }
    bflush(&bout);
}

uint16_t readtree(cnode_t *tree, FILE *file);

void huffman_decode(FILE *fdst, FILE *fsrc)
{
    cnode_t tree[0x200];
    uint16_t nnod;
    uint32_t outputlen;

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

void writetree(node_t *tree, uint16_t nnod, FILE *file)
{
    fwrite(&nnod, sizeof(uint16_t), 1, file);

    for (int i = 0; i < nnod; ++i) {
        cnode_t cnode;
        cnode.ch  = tree[i].ch;
        cnode.br0 = tree[i].br0;
        cnode.br1 = tree[i].br1;
        fwrite(&cnode, sizeof(cnode_t), 1, file);
    }
}

uint16_t readtree(cnode_t *tree, FILE *file)
{
    uint16_t nnod;

    fread(&nnod, sizeof(uint16_t), 1, file);
    fread(tree, sizeof(cnode_t), nnod, file);

    return nnod;
}
