#ifndef LIST_H_
#define LIST_H_

struct list_head {
    struct list_item *next;
    struct list_item *prev;
};

struct list_item {
    struct list_head *head;
    struct list_item *next;
    struct list_item *prev;
};

#endif
