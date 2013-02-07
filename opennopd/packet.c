#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>

#include "packet.h"

struct packet *newpacket(void)
{
    struct packet *thispacket = NULL;

    thispacket = calloc(1,sizeof(struct packet)); // Allocate space for a new packet.
    thispacket->head = NULL;
    thispacket->next = NULL;
    thispacket->prev = NULL;

    return thispacket;
}

int save_packet(struct packet *thispacket,struct nfq_q_handle *hq, u_int32_t id, int ret, __u8 *originalpacket, struct session *thissession)
{
	thispacket->hq = hq; // Save the queue handle.
	thispacket->id = id; // Save this packets id.
	memmove(thispacket->data, originalpacket, ret); // Save the packet.
	
	return 0;
}
