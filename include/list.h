#ifndef LIST_H_
#define LIST_H_

#include <pthread.h>

struct list_head {
    struct list_item *next;
    struct list_item *prev;
    int count;
    pthread_mutex_t lock;
};

struct list_item {
    struct list_head *head;
    struct list_item *next;
    struct list_item *prev;
};

#endif
