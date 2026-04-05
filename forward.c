#include "forward.h"
#include "protocols.h"
#include "queue.h"
#include "lib.h"
#include "trie.h"

// implementarea functiilor corespunzatoare Protocolului IPv4

// LPM - cautare liniara
/*
struct route_table_entry *longest_prefix_match(struct ip_hdr* ip_header, struct route_table_entry *route_table, int route_table_len) {
    struct route_table_entry *best = NULL;
    for (int i = 0; i < route_table_len; i++) {
        if ((ip_header->dest_addr & route_table[i].mask) == route_table[i].prefix) {
            if (best == NULL || ntohl(route_table[i].mask) > ntohl(best->mask)) {
                best = &route_table[i];
            }
        }
    }
    if (!best) {
        return NULL;
    } else {
        return best;
    }
}
*/

// LPM - folosind trie
struct route_table_entry* longest_prefix_match_trie(struct ip_hdr* ip_header, struct trie *trie) {

    struct route_table_entry *best_route = NULL;
    struct trie* iter = trie;

    for (int i = 31; i >= 0; i--) {
        uint8_t current_bit = (ip_header->dest_addr >> i) & 1;

        if (current_bit == 0) {
            if (iter != NULL) {
                iter = iter->left;
                if (iter != NULL && iter->rtable_entry != NULL) {
                    best_route = iter->rtable_entry;
                }
            } else {
                break;
            }
        } else {
            if (iter != NULL) {
                iter = iter->right;
                if (iter != NULL && iter->rtable_entry != NULL) {
                    best_route = iter->rtable_entry;
                }
            } else {
                break;
            }
        }
    }
    return best_route;
}


// Get mac entry - pe protocolul ARP
struct arp_table_entry* get_mac_entry(int current_ip_address, struct arp_table_entry *arp_table, int arp_table_len) {
    for (int i = 0; i < arp_table_len; i++) {
        if (arp_table[i].ip == current_ip_address)
            return &arp_table[i];
    }
    return NULL;
}