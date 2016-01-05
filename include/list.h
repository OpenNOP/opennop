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

void insert_list_item(struct list_head *head, struct list_item *item){

	if(item != NULL){

		if(head->next == NULL){
			head->next = item;
			head->prev = item;
			item->head = head;
			item->next = NULL;
			item->prev = NULL;
			head->count++;
		}else{
			head->prev->next = item;
			item->prev = head->prev;
			head->prev = item;
			item->next = NULL;
			item->head = head;
			head->count++;
		}
	}
};

#endif
