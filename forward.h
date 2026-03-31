#ifndef _FORWARD_H_
#define _FORWARD_H_

// Antetul functiilor corespunzatoare Protocolului IPv4
struct route_table_entry *longest_prefix_match(struct ip_hdr ip_header, struct route_table_entry *route_table, int route_table_len);
struct arp_table_entry* get_mac_entry(int current_ip_address, struct arp_table_entry *arp_table, int arp_table_len);

#endif