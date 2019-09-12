#include "bitio.h"

#define UINTWID (sizeof(uint32_t) * 8)

typedef struct {
    FILE *file;
    uint32_t ibuf, ipos,
             obuf, opos;
} bfile_t;

void *bopen(FILE *file)
{
    bfile_t *this = malloc(sizeof(bfile_t));

    this->file = file;
    this->ibuf = 0;
    this->ipos = UINTWID;
    this->obuf = 0;
    this->opos = 0;

    return this;
}

int bget(void *_this, uint32_t *pbits, uint32_t nbits)
{
    bfile_t *this = _this;

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

void bput(void *_this, uint32_t bits, uint32_t nbits)
{
    bfile_t *this = _this;

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

void bflush(void *_this)
{
    bfile_t *this = _this;

    fwrite(&this->obuf, (this->opos+7)/8, 1, this->file);
    this->obuf = this->opos = 0;
}
