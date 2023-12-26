#include "icmp.h"
#include "ip.h"
#include "base.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// send icmp packet
void icmp_send_packet(iface_info_t *iface, const char *in_pkt, int len, u8 type, u8 code)
{
	icmphdr_t* p = (icmphdr_t*)malloc(ICMP_HDR_SIZE + len);		
	p->type = type;
	p->code = code;
	void* payload = (void*)p + ICMP_HDR_SIZE;
	memcpy((char*)payload, in_pkt, len);
	
	p->checksum = icmp_checksum(p, ICMP_HDR_SIZE + len);

	ip_send_packet(iface, (char*)p, ICMP_HDR_SIZE + len);	
}
