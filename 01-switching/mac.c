#include "mac.h"
#include "log.h"

#include <pthread.h>
#include <stdlib.h>
#include <string.h>

mac_port_map_t mac_port_map;


#define atomic \
for(int __i = (pthread_mutex_lock(&mac_port_map.lock), 0); __i < 1; __i += 1, pthread_mutex_unlock(&mac_port_map.lock))

// initialize mac_port table
void init_mac_port_table()
{
	bzero(&mac_port_map, sizeof(mac_port_map_t));

	for (int i = 0; i < HASH_8BITS; i++) {
		init_list_head(&mac_port_map.hash_table[i]);
	}

	pthread_mutex_init(&mac_port_map.lock, NULL);

	pthread_create(&mac_port_map.thread, NULL, sweeping_mac_port_thread, NULL);
}

// destroy mac_port table
void destory_mac_port_table()
{
	atomic {
		mac_port_entry_t *entry, *q;
		for (int i = 0; i < HASH_8BITS; i++) {
			list_for_each_entry_safe(entry, q, &mac_port_map.hash_table[i], list) {
				list_delete_entry(&entry->list);
				free(entry);
			}
		}
	}
}


static mac_port_entry_t* lookup_entry(u8 mac[ETH_ALEN]) {
	mac_port_entry_t* ret = NULL;
	u8 index = hash8((char*)mac, ETH_ALEN);
	atomic {
		mac_port_entry_t *entry, *q;
		list_for_each_entry_safe(entry, q, &mac_port_map.hash_table[index], list) {
			if(strncmp((char*)mac, (char*)entry->mac, ETH_ALEN) == 0) {
				ret = entry;
			}
		}
	}
	return ret;
}

// lookup the mac address in mac_port table
iface_info_t *lookup_port(u8 mac[ETH_ALEN])
{
	mac_port_entry_t* ret_entry = lookup_entry(mac);
	iface_info_t* ret = NULL;
	if(ret_entry) {
		ret = ret_entry->iface;
		ret_entry->visited = time(NULL);
	}
	return ret;
}


// insert the mac -> iface mapping into mac_port table
void insert_mac_port(u8 mac[ETH_ALEN], iface_info_t *iface)
{
	mac_port_entry_t* old = lookup_entry(mac); 
	if(old == NULL) {
		// add a new entry that doesn't exist
		mac_port_entry_t* entry = malloc(sizeof(mac_port_entry_t));
		entry->iface = iface;
		entry->visited = time(NULL);
		strncpy((char*)entry->mac, (char*)mac, ETH_ALEN);

		u8 index = hash8((char*)mac, ETH_ALEN);
		atomic {
			list_add_head(&entry->list, &mac_port_map.hash_table[index]);
		}
	}
	else {
		// overwrite the old entry
		atomic {
			strncpy((char*)old->mac, (char*)mac, ETH_ALEN);
			old->visited = time(NULL);
		}
	}

}

// dumping mac_port table
void dump_mac_port_table()
{
	mac_port_entry_t *entry = NULL;
	time_t now = time(NULL);

	fprintf(stdout, "dumping the mac_port table:\n");
	pthread_mutex_lock(&mac_port_map.lock);
	for (int i = 0; i < HASH_8BITS; i++) {
		list_for_each_entry(entry, &mac_port_map.hash_table[i], list) {
			fprintf(stdout, ETHER_STRING " -> %s, %d\n", ETHER_FMT(entry->mac), \
					entry->iface->name, (int)(now - entry->visited));
		}
	}

	pthread_mutex_unlock(&mac_port_map.lock);
}

// sweeping mac_port table, remove the entry which has not been visited in the
// last 30 seconds.
int sweep_aged_mac_port_entry()
{
	int ret = 0;
	time_t now_time = time(NULL);
	atomic {
		mac_port_entry_t *entry, *q;
		for (int i = 0; i < HASH_8BITS; i++) {
			list_for_each_entry_safe(entry, q, &mac_port_map.hash_table[i], list) {
				if(now_time - entry->visited > MAC_PORT_TIMEOUT) {
					list_delete_entry(&entry->list);
					free(entry);
					ret += 1;
				}
			}
		}
	}
	return ret;
}

// sweeping mac_port table periodically, by calling sweep_aged_mac_port_entry
void *sweeping_mac_port_thread(void *nil)
{
	while (1) {
		sleep(1);
		int n = sweep_aged_mac_port_entry();

		if (n > 0)
			log(DEBUG, "%d aged entries in mac_port table are removed.", n);
	}

	return NULL;
}
