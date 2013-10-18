#ifndef SESSIONMANAGER_H_
#define SESSIONMANAGER_H_
#define _GNU_SOURCE

#include <linux/types.h>

#include <netinet/ip.h> // for tcpmagic and TCP options
#include <netinet/tcp.h> // for tcpmagic and TCP options

#include "session.h"

#define SESSIONBUCKETS 65536 // Number of buckets in the hash table for sessoin.
__u16 sessionhash(__u32 largerIP, __u16 largerIPPort, __u32 smallerIP,
		__u16 smallerIPPort);
void freemem(struct session_head *currentlist);
struct session *insertsession(__u32 largerIP, __u16 largerIPPort,
		__u32 smallerIP, __u16 smallerIPPort);
struct session *getsession(__u32 largerIP, __u16 largerIPPort, __u32 smallerIP,
		__u16 smallerIPPort);
struct session *clearsession(struct session *currentsession);
void sort_sockets(__u32 *largerIP, __u16 *largerIPPort, __u32 *smallerIP,
		__u16 *smallerIPPort, __u32 saddr, __u16 source, __u32 daddr,
		__u16 dest);
void initialize_sessiontable();
void clear_sessiontable();
struct session_head *getsessionhead(int i);
struct commandresult cli_show_sessionss(int client_fd, char **parameters, int numparameters, void *data);
int updateseq(__u32 largerIP, struct iphdr *iph, struct tcphdr *tcph,
		struct session *thissession);
int sourceisclient(__u32 largerIP, struct iphdr *iph, struct session *thisession);
int saveacceleratorid(__u32 largerIP, __u32 acceleratorID, struct iphdr *iph, struct session *thissession);

#endif /*SESSIONMANAGER_H_*/
