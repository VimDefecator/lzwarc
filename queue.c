#include "queue.h"

typedef struct
{
    void **pitem;
    int size, head, tail;

    pthread_mutex_t mutex;
    pthread_cond_t cndput, cndtake;
}
queue_t;

void *queue_new(int size)
{
    queue_t *this = malloc(sizeof(queue_t));

    this->pitem = malloc(size * sizeof(void *));
    this->size = size;
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

    free(this->pitem);
    free(this);
}

#define QUEUE_EMPTY(this) (this->head == this->tail)
#define QUEUE_FULL(this) ((this->tail + 1) % this->size == this->head)

void queue_put(void *_this, void *item)
{
    queue_t *this = _this;

    pthread_mutex_lock(&this->mutex);

    while QUEUE_FULL(this)
        pthread_cond_wait(&this->cndtake, &this->mutex);

    this->pitem[this->tail] = item;
    this->tail = (this->tail + 1) % this->size;

    pthread_mutex_unlock(&this->mutex);
    pthread_cond_signal(&this->cndput);
}

void *queue_take(void *_this)
{
    queue_t *this = _this;

    pthread_mutex_lock(&this->mutex);

    while QUEUE_EMPTY(this)
        pthread_cond_wait(&this->cndput, &this->mutex);

    void *ret = this->pitem[this->head];
    this->head = (this->head + 1) % this->size;

    pthread_mutex_unlock(&this->mutex);
    pthread_cond_signal(&this->cndtake);

    return ret;
}
