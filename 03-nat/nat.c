#include "nat.h"
#include "include/arp.h"
#include "include/base.h"
#include "include/hash.h"
#include "include/list.h"
#include "include/tcp.h"
#include "ip.h"
#include "icmp.h"
#include "log.h"

#include <assert.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <stdbool.h>

#include <arpa/inet.h>

static struct nat_table nat;

#define atomic \
for(int __i = (pthread_mutex_lock(&nat.lock), 0); __i < 1; __i += 1, pthread_mutex_unlock(&nat.lock))

static void add_rule(u32 e_ip, u16 e_port, u32 i_ip, u16 i_port) {
	dnat_rule_t* r = (dnat_rule_t*)malloc(sizeof(dnat_rule_t));
	init_list_head(&r->list);
	r->external_ip = e_ip;
	r->external_port = e_port;
	r->internal_ip = i_ip;
	r->internal_port = i_port;
	atomic {
		list_add_tail(&r->list, &nat.rules);
		log(DEBUG, GREEN "add a rule: %x:%hu -> %x:%hu" CLR, e_ip, e_port, i_ip, i_port);
	}
}

static void init_nat_coon(struct nat_connection* c) {
	memset(c, 0, sizeof(*c));
}

static void add_mapping(u32 e_ip, u16 e_port, u32 i_ip, u16 i_port, u32 r_ip, u16 r_port) {
	u8 key = hash8((char*)(&i_ip), sizeof(i_ip));
	nat_mapping_t* r = (nat_mapping_t*)malloc(sizeof(nat_mapping_t));
	init_list_head(&r->list);
	r->external_ip = e_ip;
	r->external_port = e_port;
	r->internal_ip = i_ip;
	r->internal_port = i_port;
	r->remote_ip = r_ip;
	r->remote_port = r_port;
	r->update_time = time(0);
	init_nat_coon(&r->conn);

	nat.port_2_map[e_port] = r;

	atomic {
		list_add_tail(&r->list, &nat.nat_mapping_list[key]);
		log(DEBUG, GREEN "add a rule: %x:%hu -> %x:%hu" CLR, e_ip, e_port, i_ip, i_port);
	}
}


// get the interface from iface name
static iface_info_t *if_name_to_iface(const char *if_name)
{
	iface_info_t *iface = NULL;
	list_for_each_entry(iface, &instance->iface_list, list) {
		if (strcmp(iface->name, if_name) == 0)
			return iface;
	}

	log(ERROR, "Could not find the desired interface according to if_name '%s'", if_name);
	return NULL;
}

static u16 get_new_free_port() {
	u16 ret = 0;
	atomic {
		for(int i = 1; i < 65536; i += 1) {
			if(nat.assigned_ports[i] == 0) {
				ret = i;
				nat.assigned_ports[i] = 1;
				break;
			}
		}
	}
	return ret;
}

// determine the direction of the packet, DIR_IN / DIR_OUT / DIR_INVALID
static int get_packet_direction(iface_info_t* iface, char *packet)
{
	int ret = DIR_INVALID;
	struct iphdr *ip = packet_to_ip_hdr(packet);
	struct tcphdr* tcp = packet_to_tcp_hdr(packet);
	// DIR_OUT: s_ip and iface's ip is in the same subnet && d_ip is not in the same subnet
	u32 s_ip = ntohl(ip->saddr);
	u32 d_ip = ntohl(ip->daddr);
	u32 s_ip_h = s_ip & iface->mask;
	u32 d_ip_h = d_ip & iface->mask;
	u32 iface_ip_h = iface->ip & iface->mask;
	u16 s_port = ntohs(tcp->sport);
	u16 d_port = ntohs(tcp->dport);
	if(s_ip_h == iface_ip_h && d_ip_h != iface_ip_h) {
		ret = DIR_OUT;
		log(DEBUG, YELLOW "\tout(before nat):" CLR " from %x:%hu, to %x:%hu, via %x[%s]:[port: to be fixed]", s_ip_h, s_port, d_ip_h, d_port, iface_ip_h, iface->name);
	}
	// DIR_IN: d_ip and iface's ip is in the same subnet
	else if(d_ip_h == iface_ip_h && iface == nat.external_iface){
		ret = DIR_IN;
		log(DEBUG, YELLOW "\tin(before nat):" CLR " from %x:%hu, to %x:%hu, via %x[%s]:[port: to be fixed]", s_ip_h, s_port, d_ip_h, d_port, iface_ip_h, iface->name);
	}
	return ret;
}

#define MAX(x, y) ((x) > (y) ? (x) : (y))
// do translation for the packet: replace the ip/port, recalculate ip & tcp
// checksum, update the statistics of the tcp connection
static void do_translation(iface_info_t *iface, char *packet, int len, int dir)
{
	struct iphdr *ip = packet_to_ip_hdr(packet);
	struct tcphdr* tcp = packet_to_tcp_hdr(packet);
	u32 s_ip = ntohl(ip->saddr);
	u16 s_port = ntohs(tcp->sport);
	u32 d_ip = ntohl(ip->daddr);
	u16 d_port = ntohs(tcp->dport);
	if(dir == DIR_OUT) {
		u8 key = hash8((char*)(&s_ip), sizeof(s_ip));
		
		nat_mapping_t *e = NULL;
		bool match = false;
		u32 e_ip = 0;
		u16 e_port = 0;
		list_for_each_entry(e, &nat.nat_mapping_list[key], list) {
			if(e->internal_ip == s_ip && e->internal_port == s_port) {
				e_ip = e->external_ip;
				e_port = e->external_port;
				match = true;

				e->update_time = time(0);
				e->conn.internal_fin = tcp->flags & TCP_FIN;
				e->conn.internal_ack = MAX(ntohl(tcp->ack), e->conn.internal_ack);
				e->conn.internal_seq_end = MAX(ntohl(tcp->seq), e->conn.internal_seq_end);
				break;
			}
		}

		if(!match) {
			e_ip = nat.external_iface->ip;
			e_port = get_new_free_port();
			add_mapping(e_ip, e_port, s_ip, s_port, d_ip, d_port);
		}

		log(DEBUG, YELLOW "\tout(after nat):" CLR " from %x:%hu, to %x:%hu, via %x:%hu\n", s_ip, s_port, d_ip, d_port, e_ip, e_port);

		ip->saddr = htonl(e_ip);
		tcp->sport = htons(e_port);
		tcp->checksum = tcp_checksum(ip, tcp);	
		ip->checksum = ip_checksum(ip);

		iface_send_packet_by_arp(nat.external_iface, d_ip, packet, len);
	}
	else {
		// check dst port first
		u32 i_ip = 0;
		u16 i_port = 0;
		nat_mapping_t* e = nat.port_2_map[d_port];
		if(e) {
			log(DEBUG, GREEN "find old mapping: %x:%hu -> %x:%hu\n" CLR, e->internal_ip, e->internal_port, e->external_ip, e->external_port);
			i_ip = e->internal_ip;
			i_port = e->internal_port;
			e->update_time = time(0);
			e->conn.external_fin = tcp->flags & TCP_FIN;
			e->conn.external_ack = MAX(ntohl(tcp->ack), e->conn.external_ack);
			e->conn.external_seq_end = MAX(ntohl(tcp->seq), e->conn.external_seq_end);
		}
		else {
			// then check the table
			dnat_rule_t *r = NULL;
			list_for_each_entry(r, &nat.rules, list) {
				if(r->external_ip == d_ip && r->external_port == d_port) {
					i_ip = r->internal_ip;
					i_port = r->internal_port;
					add_mapping(r->external_ip, r->external_port, r->internal_ip, r->internal_port, s_ip, s_port);
				}
			}
			log(DEBUG, "ignore the packet not in both rule and hash");
		}
		log(DEBUG, YELLOW "\tin(after nat):" CLR " from %x:%hu, to %x:%hu\n", s_ip, s_port, i_ip, i_port);


		ip->daddr = htonl(i_ip);
		tcp->dport = htons(i_port);
		tcp->checksum = tcp_checksum(ip, tcp);	
		ip->checksum = ip_checksum(ip);

		iface_send_packet_by_arp(nat.internal_iface, i_ip, packet, len);
	}
}

void nat_translate_packet(iface_info_t *iface, char *packet, int len)
{
	int dir = get_packet_direction(iface, packet);
	if (dir == DIR_INVALID) {
		log(ERROR, "invalid packet direction, drop it.");
		icmp_send_packet(iface, packet, len, ICMP_DEST_UNREACH, ICMP_HOST_UNREACH);
		free(packet);
		return ;
	}

	struct iphdr *ip = packet_to_ip_hdr(packet);
	if (ip->protocol != IPPROTO_TCP) {
		log(ERROR, "received non-TCP packet (0x%0hhx), drop it", ip->protocol);
		free(packet);
		return ;
	}

	do_translation(iface, packet, len, dir);
}

// check whether the flow is finished according to FIN bit and sequence number
// XXX: seq_end is calculated by `tcp_seq_end` in tcp.h
static int is_flow_finished(struct nat_connection *conn)
{
    return (conn->internal_fin && conn->external_fin) && \
            (conn->internal_ack >= conn->external_seq_end) && \
            (conn->external_ack >= conn->internal_seq_end);
}

// nat timeout thread: find the finished flows, remove them and free port
// resource
void *nat_timeout()
{
	while (1) {
		atomic {
			for(int i = 0; i < HASH_8BITS; i += 1) {
				nat_mapping_t* e = NULL;
				list_for_each_entry(e, &nat.nat_mapping_list[i], list) {
					bool timeout = (e->update_time - time(0)) > TCP_ESTABLISHED_TIMEOUT;
					if(timeout) {
						u16 e_port = e->external_port;
						list_delete_entry(&e->list);
						free(e);
						nat.assigned_ports[e_port] = 0;
						nat.port_2_map[e_port] = NULL;
					}
				}
			}
		}
		sleep(1);
	}

	return NULL;
}
#define MAX_LINE_LENGTH 256


int parse_config(const char *filename)
{
    FILE *file = fopen(filename, "r");
    if (file == NULL) {
        perror("Error opening file");
        return 1;
    }

	char internal_iface[MAX_LINE_LENGTH];
    char external_iface[MAX_LINE_LENGTH];
    char line[MAX_LINE_LENGTH];
	char e_ip[MAX_LINE_LENGTH];
	u16 e_port;
	char i_ip[MAX_LINE_LENGTH];
	u16 i_port;

    while (fgets(line, sizeof(line), file) != NULL) {
        line[strcspn(line, "\n")] = '\0';

        if (sscanf(line, "internal-iface: %s", internal_iface) == 1) {
			nat.internal_iface = if_name_to_iface(internal_iface);
			assert(nat.internal_iface);
			log(DEBUG, GREEN "add iface %s" CLR, nat.internal_iface->name);
        } else if (sscanf(line, "external-iface: %s", external_iface) == 1) {
			nat.external_iface = if_name_to_iface(external_iface);
			assert(nat.external_iface);
			log(DEBUG, GREEN "add iface %s" CLR, nat.external_iface->name);
        } else if (sscanf(line, "dnat-rules: %15[^:]:%hu -> %15[^:]:%hu",
							e_ip,
                          &e_port,
						    i_ip,
                          &i_port) == 4) {
				u32 eip = 0, iip = 0;
				inet_pton(AF_INET, e_ip, &eip);
				inet_pton(AF_INET, i_ip, &iip);
				eip = ntohl(eip);
				iip = ntohl(iip);
				add_rule(eip, e_port, iip, i_port);
        }
    }

    fclose(file);
    return 0;
}

// initialize
void nat_init(const char *config_file)
{
	memset(&nat, 0, sizeof(nat));

	for (int i = 0; i < HASH_8BITS; i++)
		init_list_head(&nat.nat_mapping_list[i]);

	init_list_head(&nat.rules);

	// seems unnecessary
	memset(nat.assigned_ports, 0, sizeof(nat.assigned_ports));
	for(int i = 0; i < 65535; i += 1) {
		nat.port_2_map[i] = NULL;
	}

	parse_config(config_file);

	pthread_mutex_init(&nat.lock, NULL);

	pthread_create(&nat.thread, NULL, nat_timeout, NULL);
}

void nat_exit()
{
	for(int i = 0; i < HASH_8BITS; i += 1) {
		nat_mapping_t* e = NULL;
		list_for_each_entry(e, &nat.nat_mapping_list[i], list) {
			bool timeout = (e->update_time - time(0)) > TCP_ESTABLISHED_TIMEOUT;
			if(timeout) {
				u16 e_port = e->external_port;
				list_delete_entry(&e->list);
				free(e);
				nat.assigned_ports[e_port] = 0;
				nat.port_2_map[e_port] = NULL;
			}
		}
	}
	dnat_rule_t* e = NULL;
	list_for_each_entry(e, &nat.rules, list) {
		free(e);
	}
}
