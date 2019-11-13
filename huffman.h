#ifndef HUFFMAN_H
#define HUFFMAN_H

#include <stdio.h>

void huffman_encode(FILE *fdst, FILE *fsrc);
void huffman_decode(FILE *fdst, FILE *fsrc);

#endif
