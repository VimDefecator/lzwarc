#ifndef QUEUE_H
#define QUEUE_H

void *queue_new(int size);
void queue_free(void *_this);
void queue_put(void *_this, void *item);
void *queue_take(void *_this);

#endif

