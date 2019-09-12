#ifndef HTBL_H
#define HTBL_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void *htbl_new  (int sz, int (*hash)(void *, int), int (*cmp)(void *, void *));
void  htbl_free (void *this, void (*item_free)(void *));

void **htbl_find_(void *this, void *item);
void  *htbl_find (void *this, void *item);
int    htbl_add  (void *this, void *item);
void   htbl_del  (void *this, void *item);

int memhash(const char *, int, int);
int strhash(const char *, int);

#endif
