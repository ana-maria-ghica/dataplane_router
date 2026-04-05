#ifndef _FORWARD_H_
#define _FORWARD_H_

#include "protocols.h"
#include "queue.h"
#include "lib.h"
#include "forward.h"
#include <arpa/inet.h> 
#include <stdbool.h>
#include <string.h>
#include "trie.h"

// Antetul functiilor corespunzatoare Protocolului IPv4
// struct route_table_entry *longest_prefix_match(struct ip_hdr *ip_header, struct route_table_entry *route_table, int route_table_len);
struct route_table_entry* longest_prefix_match_trie(struct ip_hdr* ip_header, struct trie *trie);
struct arp_table_entry* get_mac_entry(int current_ip_address, struct arp_table_entry *arp_table, int arp_table_len);

#endif