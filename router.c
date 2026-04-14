#include "protocols.h"
#include "queue.h"
#include "lib.h"
#include <arpa/inet.h> 
#include <stdbool.h>
#include <string.h>
#include "forward.h"
#include "trie.h"
#include "arp.h"

struct route_table_entry *route_table;
int route_table_len;

struct arp_table_entry *arp_table;
int arp_table_len;

struct trie *trie;

struct arp_table_entry *arp_cache;

int main(int argc, char *argv[])
{
	char buf[MAX_PACKET_LEN];

	// Do not modify this line
	init(argv + 2, argc - 2);

	// aloc tabela de rutare
	route_table = malloc(sizeof(struct route_table_entry) * MAX_TABLE_ENTRIES);
	DIE(route_table == NULL, "route_table memory");

	// aloc tabela statica ARP
	arp_table = malloc(sizeof(struct arp_table_entry) * MAX_TABLE_ENTRIES);
	DIE(arp_table == NULL, "arp_table memory");

	// citesc tabela de rutare (si tabela statica ARP)
	route_table_len = read_rtable(argv[1], route_table);
	// arp_table_len = parse_arp_table("arp_table.txt", arp_table);

	// aloc trie-ul
	trie = init_trie();
	DIE(trie == NULL, "route_table memory");
	trie = create_trie(trie, route_table, route_table_len);

	// aloc ARP cache
	int arp_cache_dim = ARP_CACHE_MAX_DIM;
	int arp_cache_size = 0;
	arp_cache = malloc(sizeof(struct arp_table_entry) * arp_cache_dim);  // daca e nevoie de mai mult fac realloc

	// initializez coada pentru pachete, daca e nevoie sa le salvez
	queue queue = create_queue();
	int queue_size = 0;
	int seq_nr = 0;

	while (1) {
		size_t interface;
		size_t len;

		interface = recv_from_any_link(buf, &len);
		DIE(interface < 0, "recv_from_any_links");
		printf("\nPacket received\n");

    // TODO: Implement the router forwarding logic

		struct ether_hdr *ether_header = (struct ether_hdr *) buf;
	
		// TODO 1.0: verific daca pachetul e destinat routerului meu / daca e broadcast; altfel dau drop
		uint8_t mac[6];
		get_interface_mac(interface, mac);  // adresa MAC a interfetei de pe care am primit pachetul
		int res = memcmp(ether_header->ethr_dhost, mac, 6);  // compar adresa destinatie cu adresa mea mac

		uint8_t broadcast_addr[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
		int res_broadcast = memcmp(ether_header->ethr_dhost, broadcast_addr, 6);

		if (res != 0 && res_broadcast != 0) {
			printf("Packet is not for me\n");
			continue;
		}

		// TODO 1.0: verific validitatea pachetului IP/ARP
		if (ntohs(ether_header->ethr_type) != ETHERTYPE_IP && ntohs(ether_header->ethr_type) != ETHERTYPE_ARP) {
			printf("Packet type not IPv4 or ARP\n");
			continue;
		}

		// logica se desparte in functie de ce tip de pachet am primit: IP vs ARP
		if (ntohs(ether_header->ethr_type) == ETHERTYPE_IP) {
			struct ip_hdr *ip_header = (struct ip_hdr *)(buf + sizeof(struct ether_hdr));

			// TODO IP.1: verific daca el e destinatia
			bool is_dest = false;
			char *ip_char = get_interface_ip(interface);
			struct in_addr ip_addr;
			inet_pton(AF_INET, ip_char, &ip_addr.s_addr);  //  #include <arpa/inet.h> int inet_pton(int af, const char *restrict src, void *restrict dst);
			if (ip_header->dest_addr == ip_addr.s_addr)
				is_dest = true;

			if (is_dest) {  // implementez icmp reply
				printf("My router is the destination\n");

				// verific checksum (pentru header IP)
				if (checksum((uint16_t *)ip_header, sizeof(struct ip_hdr)) != 0) {
					printf("Invalid checksum\n");
					continue;
				}

				// header icmp
				struct icmp_hdr* icmp_header = (struct icmp_hdr*)(buf + sizeof(struct ether_hdr) + sizeof(struct ip_hdr));
				icmp_header->mtype = 0;
				icmp_header->mcode = 0; 
				icmp_header->check = 0;
				icmp_header->check = htons(checksum((uint16_t*) icmp_header, ntohs(ip_header->tot_len) - sizeof(struct ip_hdr)));

				// resetez adresele ip
				ip_header->dest_addr = ip_header->source_addr;

				struct in_addr source_addr;
				char *current_interface = get_interface_ip(interface);
				inet_pton(AF_INET, current_interface, &source_addr.s_addr);
				ip_header->source_addr = source_addr.s_addr;

				// resetez ttl
				ip_header->ttl = 64;

				// recalculez checksum ip
				ip_header->checksum = 0;
				ip_header->checksum = htons(checksum((uint16_t *)ip_header, sizeof(struct ip_hdr)));

				// resetez adresele mac
				uint8_t eth_dest[6], eth_src[6];
				memcpy(eth_dest, ether_header->ethr_dhost, 6);
				memcpy(eth_src, ether_header->ethr_shost, 6);

				memcpy(ether_header->ethr_dhost, eth_src, 6);
				memcpy(ether_header->ethr_shost, eth_dest, 6);

				send_to_link(len, buf, interface);
				continue;

			} else {
				// forwardez pachetul
				printf("Forward to another router\n");

				// TODO IP.2: Verific checksum
				if (checksum((uint16_t *)ip_header, sizeof(struct ip_hdr)) != 0) {
					printf("Invalid checksum\n");
					continue;
				}

				// TODO IP.3: verific si actualizez TTL
				if (ip_header->ttl <= 1) {
					printf("Time limit exceeded\n");
					// inseamna ca pachetul a expirat si trimit ICMP (time exceeded)
					struct icmp_hdr* icmp_header = (struct icmp_hdr*)(buf + sizeof(struct ether_hdr) + sizeof(struct ip_hdr));
					icmp_header->mtype = 11;
					icmp_header->mcode = 0; 
					icmp_header->check = 0;
					icmp_header->check = htons(checksum((uint16_t*) icmp_header, sizeof(struct icmp_hdr)));

					// resetez adresele ip
					ip_header->dest_addr = ip_header->source_addr;

					struct in_addr source_addr;
					char *current_interface = get_interface_ip(interface);
					inet_pton(AF_INET, current_interface, &source_addr.s_addr);
					ip_header->source_addr = source_addr.s_addr;

					ip_header->tot_len = htons(sizeof(struct ip_hdr) + sizeof(struct icmp_hdr));

					// resetez ttl
					ip_header->ttl = 64;

					// setez protocolul
					ip_header->proto = IPPROTO_ICMP;

					// recalculez checksum ip
					ip_header->checksum = 0;
					ip_header->checksum = htons(checksum((uint16_t *)ip_header, sizeof(struct ip_hdr)));

					// resetez adresele mac
					uint8_t eth_dest[6], eth_src[6];
					memcpy(eth_dest, ether_header->ethr_dhost, 6);
					memcpy(eth_src, ether_header->ethr_shost, 6);

					memcpy(ether_header->ethr_dhost, eth_src, 6);
					memcpy(ether_header->ethr_shost, eth_dest, 6);

					len = sizeof(struct ether_hdr) + sizeof(struct ip_hdr) + sizeof(struct icmp_hdr);
					send_to_link(len, buf, interface);		
					continue;
				}

				// decrementez ttl-ul
				ip_header->ttl--;

				// actualizez checksum
				ip_header->checksum = 0;
				ip_header->checksum = htons(checksum((uint16_t *)ip_header, sizeof(struct ip_hdr)));

				// TODO IP.4: caut in tabela de rutare (LPM) + implementez ICMP de destination unreachable
				// struct route_table_entry *best_route = longest_prefix_match(ip_header, route_table, route_table_len);
				struct route_table_entry *best_route = longest_prefix_match_trie(ip_header, trie);
				if (!best_route) {
					printf("Destination unreachable\n");
					// implementeaza icmp
					struct icmp_hdr* icmp_header = (struct icmp_hdr*)(buf + sizeof(struct ether_hdr) + sizeof(struct ip_hdr));
					icmp_header->mtype = 3;
					icmp_header->mcode = 0; 
					icmp_header->check = 0;
					icmp_header->check = htons(checksum((uint16_t*) icmp_header, sizeof(struct icmp_hdr)));

					// resetez adresele ip
					ip_header->dest_addr = ip_header->source_addr;

					struct in_addr source_addr;
					char *current_interface = get_interface_ip(interface);
					inet_pton(AF_INET, current_interface, &source_addr.s_addr);
					ip_header->source_addr = source_addr.s_addr;

					ip_header->tot_len = htons(sizeof(struct ip_hdr) + sizeof(struct icmp_hdr));

					// resetez ttl
					ip_header->ttl = 64;

					// setez protocolul
					ip_header->proto = IPPROTO_ICMP;

					// recalculez checksum ip
					ip_header->checksum = 0;
					ip_header->checksum = htons(checksum((uint16_t *)ip_header, sizeof(struct ip_hdr)));

					// resetez adresele mac
					uint8_t eth_dest[6], eth_src[6];
					memcpy(eth_dest, ether_header->ethr_dhost, 6);
					memcpy(eth_src, ether_header->ethr_shost, 6);

					memcpy(ether_header->ethr_dhost, eth_src, 6);
					memcpy(ether_header->ethr_shost, eth_dest, 6);

					len = sizeof(struct ether_hdr) + sizeof(struct ip_hdr) + sizeof(struct icmp_hdr);
					send_to_link(len, buf, interface);
					continue;
				}

				// TODO IP.5: caut adresa MAC pentru next_hop in tabela ARP

				// TODO ARP.1: cauta adresa mac in arp_cache
				// struct arp_table_entry * mac_entry = get_mac_entry(best_route->next_hop, arp_table, arp_table_len);
				struct arp_table_entry *mac_entry = get_mac_entry_cache(best_route, arp_cache, arp_cache_size);

				
				if (!mac_entry) {
					// implementez arp:

					// TODO ARP.2: salvez pachetul in coada

					// char *buf_copy = malloc(sizeof(char) * MAX_PACKET_LEN);
					// memcpy(buf_copy, buf, len);

					// trebuie sa stochez (buf, len_buf) ca atunci cand prelucrez pachete, sa stiu care e dimensiunea corespunzatoare pachetului curent
					Node* q_node = malloc(sizeof(Node));
					q_node->buf = malloc(sizeof(char) * MAX_PACKET_LEN);
					memcpy(q_node->buf, buf, len);
					q_node->buf_length = len;

					q_node->seq_number = seq_nr;
					seq_nr++;

					queue_enq(queue, q_node);
					queue_size++;

					// TODO ARP.3: generez ARP request
					char arp_req[MAX_PACKET_LEN];
					create_arp_request(arp_req, best_route, interface);
					int length = sizeof(struct ether_hdr) + sizeof(struct arp_hdr);
					send_to_link(length, arp_req, best_route->interface);

					continue;
				}

				memcpy(ether_header->ethr_dhost, mac_entry->mac, 6);
				get_interface_mac(best_route->interface, ether_header->ethr_shost);

				// trimit pachetul mai departe
				send_to_link(len, buf, best_route->interface);
			}

		// logica pentru ARP request
		} else if (ntohs(ether_header->ethr_type) == ETHERTYPE_ARP) {
			struct arp_hdr *arp_header = (struct arp_hdr*) (buf + sizeof(struct ether_hdr));

			if (ntohs(arp_header->opcode) == 1) {
				// pachet ARP request
				
				// setez campurile in header-ul arp
				// transform request-ul in reply
				arp_header->opcode = htons(2);

				// swap intre ip sender si target
				uint32_t aux = arp_header->tprotoa;
				arp_header->tprotoa = arp_header->sprotoa;
				arp_header->sprotoa = aux;

				// setez adresele hw de sender si target
				memcpy(arp_header->thwa, arp_header->shwa, 6);
				get_interface_mac(interface, arp_header->shwa);
				
				// setez adresele mac: 
				memcpy(ether_header->ethr_dhost, ether_header->ethr_shost, 6);
				get_interface_mac(interface, ether_header->ethr_shost);

				// trimit pachetul
				send_to_link(len, buf, interface);

			} else if (ntohs(arp_header->opcode) == 2) {
				// pachet ARP reply
				printf("ARP reply received from %s\n", inet_ntoa(*(struct in_addr*)&arp_header->sprotoa));

				// adaug in cache adresa mac primita
				uint8_t recv_mac[6];
				memcpy(recv_mac, arp_header->shwa, 6);

				memcpy(&arp_cache[arp_cache_size].mac, recv_mac, 6);
				arp_cache[arp_cache_size].ip = arp_header->sprotoa;
				arp_cache_size++;

				// verific daca pot sa dau send vreunui pachet din coada
				// dar fac asta de maxim len(queue) ca sa nu ciclez
				int iter = 1;

				struct queue_node* q_node = malloc(sizeof(struct queue_node));
				while (iter <= queue_size) {
					q_node = queue_deq(queue);
				
					// procesez pachetul
					bool processed = false;
					struct ether_hdr* ether_header = (struct ether_hdr*)(q_node->buf);
					struct ip_hdr* ip_header = (struct ip_hdr*)(q_node->buf + sizeof(struct ether_hdr));

					// aplic LPM pe ip-ul destinatie pentru pachetul curent
					struct route_table_entry* best_route = longest_prefix_match_trie(ip_header, trie);
					if (best_route->next_hop == arp_header->sprotoa) {
						processed = true;
					}

					if(processed) {
						// daca e ce trebuie, trimit pachetul
						queue_size--;

						// actualizez adresele mac
						memcpy(ether_header->ethr_dhost, arp_header->shwa, 6);
						get_interface_mac(best_route->interface, ether_header->ethr_shost); 

						// trimit
						send_to_link(q_node->buf_length, q_node->buf, best_route->interface);

						free(q_node->buf);
						free(q_node);
						break;
					} else {
						// daca nu e ce trebuie, adaug pachetul la loc
						queue_enq(queue, q_node);
					}

					iter++;
				}
			}
			continue;
		}
	}

	free(route_table);
	free(arp_table);
	destroy_trie(&trie);
}

