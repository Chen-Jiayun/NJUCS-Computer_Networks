#include "arpcache.h"
#include "arp.h"
#include "base.h"
#include "ether.h"
#include "icmp.h"
#include "list.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <signal.h>
#include <stdbool.h>

static arpcache_t arpcache;


#define atomic \
for(int __i = (pthread_mutex_lock(&arpcache.lock), 0); __i < 1; __i += 1, pthread_mutex_unlock(&arpcache.lock))

// initialize IP->mac mapping, request list, lock and sweeping thread
void arpcache_init()
{
	bzero(&arpcache, sizeof(arpcache_t));

	init_list_head(&(arpcache.req_list));

	pthread_mutex_init(&arpcache.lock, NULL);

	pthread_create(&arpcache.thread, NULL, arpcache_sweep, NULL);
}

// release all the resources when exiting
void arpcache_destroy()
{
	atomic {
		struct arp_req *req_entry = NULL, *req_q;
		list_for_each_entry_safe(req_entry, req_q, &(arpcache.req_list), list) {
			struct cached_pkt *pkt_entry = NULL, *pkt_q;
			list_for_each_entry_safe(pkt_entry, pkt_q, &(req_entry->cached_packets), list) {
				list_delete_entry(&(pkt_entry->list));
				free(pkt_entry->packet);
				free(pkt_entry);
			}

			list_delete_entry(&(req_entry->list));
			free(req_entry);
		}

		pthread_kill(arpcache.thread, SIGTERM);
	}
}

// lookup the IP->mac mapping
//
// traverse the table to find whether there is an entry with the same IP
// and mac address with the given arguments
// return 1 if the entry is found, else return 0
int arpcache_lookup(u32 ip4, u8 mac[ETH_ALEN])
{
	int ret = 0;
	atomic {
		for(int i = 0; i < MAX_ARP_SIZE; i += 1) {
			arp_cache_entry_t* e = arpcache.entries + i;
			if(e->valid) {
				if(ip4 == e->ip4) {
					// side effect to  return found-mac
					strncpy((char*)mac, (const char*)e->mac, ETH_ALEN);
					ret = 1; 
					break;
				}
			}
		}
	}
	return ret;
}

static void append_cache_packet(arp_req_t* req, char* packet, int len) {
	cached_pkt_t* c = (cached_pkt_t*)malloc(sizeof(cached_pkt_t));
	c->len = len;
	c->packet = packet;
	list_add_tail(&req->cached_packets, &c->list);
}

// append the packet to arpcache
//
// Lookup in the list which stores pending packets, if there is already an
// entry with the same IP address and iface (which means the corresponding arp
// request has been sent out), just append this packet at the tail of that entry
// (the entry may contain more than one packet); otherwise, malloc a new entry
// with the given IP address and iface, append the packet, and send arp request.
void arpcache_append_packet(iface_info_t *iface, u32 ip4, char *packet, int len)
{
	atomic {
		bool find = false;
		arp_req_t* req, *req_q;
		list_for_each_entry_safe(req, req_q, &(arpcache.req_list), list) {
			if(req->iface == iface && req->ip4 == ip4) {
				find = true;
				req->retries += 1;
				req->sent = time(0);
				append_cache_packet(req, packet, len);
			}
		}

		if(!find) {
			arp_req_t* a = (arp_req_t*)malloc(sizeof(arp_req_t));
			a->iface = iface;
			a->ip4 = ip4;
			a->sent = time(0);
			a->retries = 0;
			list_add_tail(&(arpcache.req_list), &a->list);
			append_cache_packet(a, packet, len);
			arp_send_request(iface, ip4);
		}
		
	}
}

// insert the IP->mac mapping into arpcache, if there are pending packets
// waiting for this mapping, fill the ethernet header for each of them, and send
// them out
void arpcache_insert(u32 ip4, u8 mac[ETH_ALEN])
{
	atomic {
		for(int i = 0; i < MAX_ARP_SIZE; i += 1) {
			arp_cache_entry_t* e = arpcache.entries + i;
			if(!e->valid) {
				e->ip4 = ip4;
				strncpy((char*) e->mac, (const char*)mac, ETH_ALEN);
				e->added = time(0);
				e->valid = 1;
			}
		}
		arp_req_t* req, *req_q;
		list_for_each_entry_safe(req, req_q, &(arpcache.req_list), list) {
			if(req->ip4 == ip4) {
				cached_pkt_t* pkt, *pkt_q;
				list_for_each_entry_safe(pkt,pkt_q, &(req->cached_packets), list) {
					iface_send_packet_by_arp(req->iface, req->ip4, pkt->packet, pkt->len);
					list_delete_entry(&(pkt->list));
					free(pkt->packet);
					free(pkt);
				}
			}
			list_delete_entry(&(req->list));
			free(req);
		}
	}
}

// sweep arpcache periodically
//
// For the IP->mac entry, if the entry has been in the table for more than 15
// seconds, remove it from the table.
// For the pending packets, if the arp request is sent out 1 second ago, while 
// the reply has not been received, retransmit the arp request. If the arp
// request has been sent 5 times without receiving arp reply, for each
// pending packet, send icmp packet (DEST_HOST_UNREACHABLE), and drop these
// packets.
void *arpcache_sweep(void *arg) 
{
	while (1) {
		sleep(1);
		atomic {
			// rm old entries
			time_t now_time = time(0);
			for(int i = 0; i < MAX_ARP_SIZE; i += 1) {
				arp_cache_entry_t* e = arpcache.entries + i;
				if(e->valid) {
					double time_diff = difftime(now_time, e->added);
					if(time_diff > 15) {
						e->valid = false;
					}
				}
			}
			// TODO: adjust the request entry

		}
	}

	return NULL;
}
