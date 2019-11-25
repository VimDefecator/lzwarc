#include <stdlib.h>
#include <string.h>
#include "misc.h"
#include "pathtree.h"

#define PATHTREE_NAME_IS(tree, str) \
    (0 == strcmp(((pathtree_t *)(tree))->name, str))

void *pathtree_new(char *name, pathtree_t *parent)
{
    pathtree_t *this = malloc(sizeof(pathtree_t) + strlen(name));

    this->child = NULL;
    this->parent = parent;
    strcpy(this->name, name);

    return this;
}

void pathtree_free(pathtree_t *this)
{
    if (this->child) {
        Apply(pathtree_free, this->child, LsSize(this->child));
        LsFree(this->child);
    }
    free(this);
}

void pathtree_add(pathtree_t *this, char *path)
{
    char *sl = strchr(path, '/');
    if (sl)
    {
        int idx;

        *sl = '\0';
        LsFind(this->child, path, PATHTREE_NAME_IS, idx);
        if (idx == -1) {
            LsAdd(this->child, pathtree_new(path, this));
            idx = LsSize(this->child) - 1;
        }
        *sl = '/';

        pathtree_add(this->child[idx], sl+1);
    }
    else
    {
        int idx;
        LsFind(this->child, path, PATHTREE_NAME_IS, idx)
        if (idx == -1) LsAdd(this->child, pathtree_new(path, this));
    }
}

int pathtree_cmp(pathtree_t **, pathtree_t **);

void pathtree_sort(pathtree_t *this)
{
    if (this->child) {
        qsort(this->child, LsSize(this->child), sizeof(void *),
              pathtree_cmp);
        for (int i = 0; i < LsSize(this->child); ++i)
            pathtree_sort(this->child[i]);
    }
}

int pathtree_cmp(pathtree_t **pthis1, pathtree_t **pthis2)
{
    int ret = !(*pthis1)->child - !(*pthis2)->child;

    return ret ? ret : strcmp((*pthis1)->name, (*pthis2)->name);
}
