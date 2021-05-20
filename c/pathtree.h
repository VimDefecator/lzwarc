#ifndef PATHTREE_H
#define PATHTREE_H

typedef struct pathtree {
    struct pathtree **child;
    struct pathtree *parent;
    char name[1];
} pathtree_t;

void *pathtree_new(char *name, pathtree_t *parent);
void pathtree_free(pathtree_t *this);

void pathtree_add(pathtree_t *this, char *path);
void pathtree_sort(pathtree_t *this);

#endif

