#ifndef BITIO_H
#define BITIO_H

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

#define UINTWID (sizeof(uint32_t) * 8)

typedef struct {
    FILE *file;
    uint32_t ibuf, ipos,
             obuf, opos;
} bitio_t;

#define BITIO_INIT {NULL, 0, UINTWID, 0, 0}

inline int bget(bitio_t *this, uint32_t *pbits, uint32_t nbits)
{
    nbits = nbits <= UINTWID ? nbits : UINTWID;
    *pbits = (this->ipos < UINTWID)
           ? (this->ibuf >> this->ipos) & ((uint32_t )-1 >> (UINTWID - nbits))
           : 0;
    this->ipos += nbits;
    if (this->ipos <= UINTWID)
        return 0;

    this->ibuf = 0;
    this->ipos -= UINTWID;
    uint32_t rcnt = 8 * fread(&this->ibuf, 1, sizeof(uint32_t), this->file);
    if (rcnt < this->ipos)
        return -1;

    *pbits |= (this->ibuf & ((uint32_t )-1 >> (UINTWID - this->ipos)))
              << (nbits - this->ipos);
    this->ibuf <<= UINTWID - rcnt;
    this->ipos += UINTWID - rcnt;
        return 0;
}

inline void bput(bitio_t *this, uint32_t bits, uint32_t nbits)
{
    nbits = nbits <= UINTWID ? nbits : UINTWID;
    if (nbits != UINTWID) bits &= (1 << nbits) - 1;
    this->obuf |= bits << this->opos;
    this->opos += nbits;
    if (this->opos < UINTWID)
        return;

    fwrite(&this->obuf, sizeof(uint32_t), 1, this->file);
    this->opos -= UINTWID;
    this->obuf = this->opos ? bits >> (nbits - this->opos) : 0;
}

inline void bflush(bitio_t *this)
{
    fwrite(&this->obuf, (this->opos+7)/8, 1, this->file);
    this->obuf = this->opos = 0;
}

#endif
