#ifndef DITER_H
#define DITER_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <linux/limits.h>

void *dopen  (char *path);
void  dclose (void *this);

int dnext(void *this, char *dest);

#endif
