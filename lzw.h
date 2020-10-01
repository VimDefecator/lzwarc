#ifndef LZW_H
#define LZW_H

#include <stdio.h>

void lzw_encode(FILE *fdst, FILE *fsrc);
void lzw_decode(FILE *fdst, FILE *fsrc);

#endif

