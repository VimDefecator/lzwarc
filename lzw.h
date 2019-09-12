#ifndef LZW_H
#define LZW_H

#include <stdio.h>
#include <stdlib.h>

void lzw_encode(FILE *in, FILE *out);
void lzw_decode(FILE *in, FILE *out);

#endif
