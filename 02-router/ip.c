#include "ip.h"
#include "icmp.h"
#include "include/arp.h"
#include "include/arpcache.h"
#include "include/ether.h"
#include "rtable.h"
#include <netinet/in.h>
#include <stdio.h>

#include "log.h"

// handle ip packet
//
// If the packet is ICMP echo request and the destination IP address is equal to
// the IP address of the iface, send ICMP echo reply; otherwise, forward the
// packet.
void handle_ip_packet(iface_info_t *iface, char *packet, int len)
{
	struct iphdr* p = packet_to_ip_hdr(packet);
	u32 daddr = ntohl(p->daddr);

	ether_header_t* eth = (ether_header_t*)packet;
	arpcache_insert(ntohl(p->saddr), eth->ether_shost);

	if(daddr == iface->ip) {
		icmp_send_packet(iface, (const char*)p, ntohs(p->tot_len), ICMP_ECHOREPLY, 0);
	}
	else {
		p->ttl -= 1;
		if(p->ttl == 0) {
			log(DEBUG, RED "one hup works not well" CLR);
			icmp_send_packet(iface, (const char*)p, IP_BASE_HDR_SIZE + 8, ICMP_TIME_EXCEEDED, ICMP_EXC_TTL);
		}	
		else {
            rt_entry_t* rt = longest_prefix_match(daddr);
            if(rt) {
				// effect checksum??
				p->checksum = ip_checksum(p);
				// p->ttl += 1;
				log(DEBUG, GREEN "forward to %s" CLR, rt->if_name);
				iface_send_packet_by_arp(rt->iface, daddr, packet, len);
            }
            else {
				log(DEBUG, YELLOW "nowhere to forward" CLR);
                icmp_send_packet(iface, (const char*)p, ntohs(p->tot_len), ICMP_DEST_UNREACH, ICMP_HOST_UNREACH);
            }
		}
	}
}
