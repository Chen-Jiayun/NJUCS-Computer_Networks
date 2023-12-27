#include "include/ether.h"
#include "ip.h"
#include "icmp.h"
#include "arpcache.h"
#include "rtable.h"
#include "arp.h"

// #include "log.h"

#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// initialize ip header 
void ip_init_hdr(struct iphdr *ip, u32 saddr, u32 daddr, u16 len, u8 proto)
{
	ip->version = 4;
	ip->ihl = 5;
	ip->tos = 0;
	ip->tot_len = htons(len);
	ip->id = rand();
	ip->frag_off = htons(IP_DF);
	ip->ttl = DEFAULT_TTL;
	ip->protocol = proto;
	ip->saddr = htonl(saddr);
	ip->daddr = htonl(daddr);
	ip->checksum = ip_checksum(ip);
}

// lookup in the routing table, to find the entry with the same and longest prefix.
// the input address is in host byte order
rt_entry_t *longest_prefix_match(u32 dst)
{
    rt_entry_t* ret = NULL;
    u32 match_num = 0;
	rt_entry_t *entry = NULL;
	list_for_each_entry(entry, &rtable, list) {
        u32 masked_ip = entry->mask & entry->dest; 
        u32 masked_dst_ip = entry->mask & dst;
        if(masked_ip == masked_dst_ip && masked_dst_ip > match_num) {
            ret = entry;
            match_num = masked_dst_ip;
        }
	}

    return ret;
}

// send IP packet
//
// Different from forwarding packet, ip_send_packet sends packet generated by
// router itself. This function is used to send ICMP packets.
void ip_send_packet(iface_info_t *iface, char *packet, int len)
{
	void* out = malloc(len + IP_BASE_HDR_SIZE + ETHER_HDR_SIZE);

	ether_header_t* eth = (ether_header_t*)out;
	memcpy(eth->ether_shost, iface->mac, ETH_ALEN);
	eth->ether_type = htons(ETH_P_IP);

    iphdr_t* in_ip = (iphdr_t*)(packet + ICMP_HDR_SIZE);
	iphdr_t* ip = (iphdr_t*)(out + ETHER_HDR_SIZE);

	ip_init_hdr(ip, iface->ip, ntohl(in_ip->saddr), len + IP_BASE_HDR_SIZE, IPPROTO_ICMP);

	memcpy(out + IP_BASE_HDR_SIZE + ETHER_HDR_SIZE, packet, len);

    iface_send_packet_by_arp(iface, ntohl(ip->daddr), out, len + ETHER_HDR_SIZE + IP_BASE_HDR_SIZE);
}
