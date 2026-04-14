#include "forward.h"
#include "protocols.h"
#include "queue.h"
#include "lib.h"
#include "trie.h"

// implementarea functiilor corespunzatoare Protocolului IPv4

// LPM - cautare liniara
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


// LPM - implementat folosind trie
struct route_table_entry* longest_prefix_match_trie(struct ip_hdr* ip_header, struct trie *trie) {

    struct route_table_entry *best_route = NULL;
    struct trie* iter = trie;
    uint32_t dest = ntohl(ip_header->dest_addr);

    for (int i = 31; i >= 0; i--) {
        uint8_t current_bit = (dest >> i) & 1;

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


// implementarea functiilor corespunzatoare Protocolului ARP

// Get mac entry - pe protocolul ARP
struct arp_table_entry* get_mac_entry(uint32_t current_ip_address, struct arp_table_entry *arp_table, int arp_table_len) {
    for (int i = 0; i < arp_table_len; i++) {
        if (arp_table[i].ip == current_ip_address)
            return &arp_table[i];
    }
    return NULL;
}

// protocolul ARP - tabela dinamica
struct arp_table_entry* get_mac_entry_cache(struct route_table_entry* best_route, struct arp_table_entry *arp_cache, int arp_cache_size) {
    uint32_t current_ip_address = best_route->next_hop;
    // caut in cache
    for (int i = 0; i < arp_cache_size; i++) {
        if (arp_cache[i].ip == current_ip_address) {
            return &arp_cache[i];
        }
    }

    return NULL;
}

void create_arp_request(char* buf, struct route_table_entry* best_route, int interface) {
    // setez headerul ethernet corespunzator
    struct ether_hdr* ether_header = (struct ether_hdr*)(buf);

    // adresa mea mac
    get_interface_mac(best_route->interface, ether_header->ethr_shost);

    // adresa mac destinatie: broadcast
    uint8_t broadcast_addr[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
    memcpy(ether_header->ethr_dhost, broadcast_addr, 6);

    ether_header->ethr_type = htons(ETHERTYPE_ARP);

    // setez headerul ARP
    struct arp_hdr* arp_header = (struct arp_hdr*) (buf + sizeof(struct ether_hdr));

    // setez campurile in ordine - cum sunt date in structura:
    // 1 pentru Ethernet
    arp_header->hw_type = htons(1);

    // protocolul de retea IPv4
    arp_header->proto_type = htons(ETHERTYPE_IP);

    // lungimea adresei mac
    arp_header->hw_len = 6;

    // lungimea adresei IP
    arp_header->proto_len = 4;

    // 1 pt ARP request
    arp_header->opcode = htons(1);

    // adresa hw pentru sender
    get_interface_mac(best_route->interface, arp_header->shwa);

    // adresa ip pentru sender
    arp_header->sprotoa = inet_addr(get_interface_ip(best_route->interface));

    // adresa hw target
    memset(arp_header->thwa, 0, 6);

    // adresa ip target
    arp_header->tprotoa = best_route->next_hop;
}
