#include "ip.h"
#include "icmp.h"
#include "rtable.h"
#include <stdio.h>
#include <stdlib.h>

// handle ip packet
//
// If the packet is ICMP echo request and the destination IP address is equal to
// the IP address of the iface, send ICMP echo reply; otherwise, forward the
// packet.
void handle_ip_packet(iface_info_t *iface, char *packet, int len)
{
	struct iphdr* p = packet_to_ip_hdr(packet);
	if(p->daddr == iface->ip) {
		icmp_send_packet(iface, (const char*)p, p->tot_len, ICMP_ECHOREPLY, 0);
	}
	else {
		if(p->ttl == 0) {
			icmp_send_packet(iface, (const char*)p, p->tot_len, ICMP_TIME_EXCEEDED, ICMP_EXC_TTL);
		}	
		else {
            rt_entry_t* rt = longest_prefix_match(p->daddr);
            if(rt) {
                iface_send_packet(rt->iface, (const char*)p, p->tot_len);
            }
            else {
                icmp_send_packet(iface, (const char*)p, p->tot_len, ICMP_DEST_UNREACH, ICMP_HOST_UNREACH);
            }
		}
	}
}
