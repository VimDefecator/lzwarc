#ifndef BITIO_H
#define BITIO_H

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

#define BIOBUFW 64

typedef struct {
    FILE *file;
    uint64_t ibuf, ipos,
             obuf, opos;
} bitio_t;

#define BITIO_INIT {NULL, 0, BIOBUFW, 0, 0}

static inline int bget(bitio_t *this, uint64_t *pbits, int nbits)
{
    nbits = nbits <= BIOBUFW ? nbits : BIOBUFW;
    *pbits = (this->ipos < BIOBUFW)
           ? (this->ibuf >> this->ipos) & ((uint64_t )-1 >> (BIOBUFW - nbits))
           : 0;
    this->ipos += nbits;
    if (this->ipos <= BIOBUFW)
        return 0;

    this->ibuf = 0;
    this->ipos -= BIOBUFW;
    int rcnt = 8 * fread(&this->ibuf, 1, sizeof(uint64_t), this->file);
    if (rcnt < this->ipos)
        return -1;

    *pbits |= (this->ibuf & ((uint64_t )-1 >> (BIOBUFW - this->ipos)))
              << (nbits - this->ipos);
    this->ibuf <<= BIOBUFW - rcnt;
    this->ipos += BIOBUFW - rcnt;
        return 0;
}

static inline void bput(bitio_t *this, uint64_t bits, int nbits)
{
    nbits = nbits <= BIOBUFW ? nbits : BIOBUFW;
    if (nbits != BIOBUFW) bits &= (1 << nbits) - 1;
    this->obuf |= bits << this->opos;
    this->opos += nbits;
    if (this->opos < BIOBUFW)
        return;

    fwrite(&this->obuf, sizeof(uint64_t), 1, this->file);
    this->opos -= BIOBUFW;
    this->obuf = this->opos ? bits >> (nbits - this->opos) : 0;
}

static inline void bflush(bitio_t *this)
{
    fwrite(&this->obuf, (this->opos+7)/8, 1, this->file);
    this->obuf = this->opos = 0;
}

#endif

