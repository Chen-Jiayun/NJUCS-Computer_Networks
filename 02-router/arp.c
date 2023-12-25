#include "arp.h"
#include "base.h"
#include "types.h"
#include "ether.h"
#include "arpcache.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

#include "log.h"

static void lib_log(const char *format, ...) {
    va_list args;
    va_start(args, format);

    FILE *file = fopen("/tmp/cn/output.txt", "a");

    vfprintf(file, format, args);

    va_end(args);
    fclose(file);
}


// send an arp request: encapsulate an arp request packet, send it out through
// iface_send_packet
void arp_send_request(iface_info_t *iface, u32 dst_ip)
{
	ether_arp_t* arp = malloc(sizeof(ether_arp_t));
	arp->arp_hrd = HTYPE_ETH;
	arp->arp_pro = PTYPE_IPV4;
	arp->arp_hln = HLEN_ETH;
	arp->arp_pln = PLEN_IPV4;
	arp->arp_op = ARPOP_REQUEST;

	// set sender mac
	memcpy(arp->arp_sha, iface->mac, HLEN_ETH);
	// set send ip
	arp->arp_spa = iface->ip;
	arp->arp_tpa = dst_ip;

	iface_send_packet(iface, (const char*)arp, sizeof(ether_arp_t));
}

// send an arp reply packet: encapsulate an arp reply packet, send it out
// through iface_send_packet
void arp_send_reply(iface_info_t *iface, struct ether_arp *req_hdr)
{
	ether_arp_t* arp = malloc(sizeof(ether_arp_t));
	arp->arp_hrd = htons(HTYPE_ETH);
	arp->arp_pro = htons(PTYPE_IPV4);
	arp->arp_hln = HLEN_ETH;
	arp->arp_pln = PLEN_IPV4;
	arp->arp_op = htons(ARPOP_REPLY);

	// set sender mac
	memcpy(arp->arp_sha, iface->mac, HLEN_ETH);
	// set send ip
	arp->arp_spa = htonl(iface->ip);

	// set target mac
	memcpy(arp->arp_tha, req_hdr->arp_sha, HLEN_ETH);	
	
	// set target ip
	arp->arp_tpa = req_hdr->arp_spa;

	iface_send_packet(iface, (const char*)arp, sizeof(ether_arp_t));
}

void handle_arp_packet(iface_info_t *iface, char *packet, int len)
{
    packet += ETHER_HDR_SIZE;
	ether_arp_t* arp = (ether_arp_t*)packet;
    u16 arp_op = ntohs(arp->arp_op);
    switch (arp_op) {
        case ARPOP_REPLY: { 
            arpcache_insert(arp->arp_spa, arp->arp_sha); 
            break;
        }
        case ARPOP_REQUEST:  { 
            arp_send_reply(iface, (ether_arp_t*)packet); 
            break;
        }
        default: break;
    }
}

// send (IP) packet through arpcache lookup 
//
// Lookup the mac address of dst_ip in arpcache. If it is found, fill the
// ethernet header and emit the packet by iface_send_packet, otherwise, pending 
// this packet into arpcache, and send arp request.
void iface_send_packet_by_arp(iface_info_t *iface, u32 dst_ip, char *packet, int len)
{
	struct ether_header *eh = (struct ether_header *)packet;
	memcpy(eh->ether_shost, iface->mac, ETH_ALEN);
	eh->ether_type = htons(ETH_P_IP);

	u8 dst_mac[ETH_ALEN];
	int found = arpcache_lookup(dst_ip, dst_mac);
	if (found) {
		// log(DEBUG, "found the mac of %x, send this packet", dst_ip);
		memcpy(eh->ether_dhost, dst_mac, ETH_ALEN);
		iface_send_packet(iface, packet, len);
	}
	else {
		// log(DEBUG, "lookup %x failed, pend this packet", dst_ip);
		arpcache_append_packet(iface, dst_ip, packet, len);
	}
}
