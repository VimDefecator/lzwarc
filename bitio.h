#ifndef BITIO_H
#define BITIO_H

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

#define BIOBUFW 64

_Thread_local static FILE *g_bfdst, *g_bfsrc;
_Thread_local static uint64_t g_bibuf, g_bipos, g_bobuf, g_bopos;

static inline void binit(FILE *fdst, FILE *fsrc)
{
    g_bfdst = fdst;
    g_bfsrc = fsrc;
    g_bibuf = 0;
    g_bipos=  BIOBUFW;
    g_bobuf = 0;
    g_bopos = 0;
}

static inline int bget(uint64_t *pbits, int nbits)
{
    nbits = nbits <= BIOBUFW ? nbits : BIOBUFW;
    *pbits = (g_bipos < BIOBUFW)
           ? (g_bibuf >> g_bipos) & ((uint64_t )-1 >> (BIOBUFW - nbits))
           : 0;
    g_bipos += nbits;
    if (g_bipos <= BIOBUFW)
        return 0;

    g_bibuf = 0;
    g_bipos -= BIOBUFW;
    int rcnt = 8 * fread(&g_bibuf, 1, sizeof(uint64_t), g_bfsrc);
    if (rcnt < g_bipos)
        return -1;

    *pbits |= (g_bibuf & ((uint64_t )-1 >> (BIOBUFW - g_bipos)))
              << (nbits - g_bipos);
    g_bibuf <<= BIOBUFW - rcnt;
    g_bipos += BIOBUFW - rcnt;
        return 0;
}

static inline void bput(uint64_t bits, int nbits)
{
    nbits = nbits <= BIOBUFW ? nbits : BIOBUFW;
    if (nbits != BIOBUFW) bits &= (1 << nbits) - 1;
    g_bobuf |= bits << g_bopos;
    g_bopos += nbits;
    if (g_bopos < BIOBUFW)
        return;

    fwrite(&g_bobuf, sizeof(uint64_t), 1, g_bfdst);
    g_bopos -= BIOBUFW;
    g_bobuf = g_bopos ? bits >> (nbits - g_bopos) : 0;
}

static inline void bflush()
{
    fwrite(&g_bobuf, (g_bopos+7)/8, 1, g_bfdst);
    g_bobuf = g_bopos = 0;
}

#endif

