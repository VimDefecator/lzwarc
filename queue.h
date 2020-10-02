#ifndef QUEUE_H
#define QUEUE_H

void *queue_new(int maxitems, int szitem);
void queue_free(void *_this);
void queue_put(void *_this, void *src);
void queue_take(void *_this, void *dst);

#endif

