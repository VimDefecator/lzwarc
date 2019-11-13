#ifndef DITER_H
#define DITER_H

void *dopen  (char *path);
void  dclose (void *this);

int dnext(void *this, char *dest);

#endif
