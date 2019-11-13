#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <linux/limits.h>

#include "diter.h"

typedef struct diter {
    struct diter *child;
    DIR *dir;
    char path[PATH_MAX];
} diter_t;

void *dopen(char *path)
{
    diter_t *this = NULL;

    struct stat s;
    stat(path, &s);
    if (S_ISDIR(s.st_mode) || S_ISREG(s.st_mode)) {
        this = malloc(sizeof(diter_t));
        this->child = NULL;
        this->dir = S_ISDIR(s.st_mode) ? opendir(path) : NULL;
        strcpy(this->path, path);
    }
    return this;
}

void dclose(void *_this)
{
    diter_t *this = _this;
    if (this == NULL) return;

    dclose(this->child);
    if (this->dir) closedir(this->dir);
    free(this);
}

int dnext(void *_this, char *dest)
{
    diter_t *this = _this;
    if (this == NULL) return 0;

    if (this->dir == NULL) {
        strcpy(dest, this->path);
        this->path[0] = '\0';
        return dest[0] != '\0';
    }

    while (!dnext(this->child, dest)) {
        dclose(this->child);
        this->child = NULL;

        struct dirent *dent;
        do dent = readdir(this->dir); while (dent != NULL
                                             && (!strcmp(dent->d_name, ".") ||
                                                 !strcmp(dent->d_name, "..")));
        if (dent == NULL) return 0;

        char path[PATH_MAX];
        sprintf(path, "%s/%s", this->path, dent->d_name);

        this->child = dopen(path);
    }
    return 1;
}
