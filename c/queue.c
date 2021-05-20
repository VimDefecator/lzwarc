#include <stdlib.h>
#include <string.h>
#include <pthread.h>

#include "queue.h"

typedef struct
{
    void *buf;
    int maxitems, szitem, head, tail;

    pthread_mutex_t mutex;
    pthread_cond_t cndput, cndtake;
}
queue_t;

void *queue_new(int maxitems, int szitem)
{
    queue_t *this = malloc(sizeof(queue_t));

    this->buf = malloc(maxitems * szitem);
    this->maxitems = maxitems;
    this->szitem = szitem;
    this->head = 0;
    this->tail = 0;

    pthread_mutex_init(&this->mutex, NULL);
    pthread_cond_init(&this->cndput, NULL);
    pthread_cond_init(&this->cndtake, NULL);

    return this;
}

void queue_free(void *_this)
{
    queue_t *this = _this;

    pthread_cond_destroy(&this->cndtake);
    pthread_cond_destroy(&this->cndput);
    pthread_mutex_destroy(&this->mutex);

    free(this->buf);
    free(this);
}

#define QUEUE_EMPTY(this) (this->head == this->tail)
#define QUEUE_FULL(this) ((this->tail + 1) % this->maxitems == this->head)

void queue_put(void *_this, void *src)
{
    queue_t *this = _this;
    char (*buf)[this->szitem] = this->buf;

    pthread_mutex_lock(&this->mutex);

    while QUEUE_FULL(this)
        pthread_cond_wait(&this->cndtake, &this->mutex);

    memcpy(buf[this->tail], src, sizeof(*buf));
    this->tail = (this->tail + 1) % this->maxitems;

    pthread_mutex_unlock(&this->mutex);
    pthread_cond_signal(&this->cndput);
}

void queue_take(void *_this, void *dst)
{
    queue_t *this = _this;
    char (*buf)[this->szitem] = this->buf;

    pthread_mutex_lock(&this->mutex);

    while QUEUE_EMPTY(this)
        pthread_cond_wait(&this->cndput, &this->mutex);

    memcpy(dst, buf[this->head], sizeof(*buf));
    this->head = (this->head + 1) % this->maxitems;

    pthread_mutex_unlock(&this->mutex);
    pthread_cond_signal(&this->cndtake);
}

