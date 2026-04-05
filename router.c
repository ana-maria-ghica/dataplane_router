#include "protocols.h"
#include "queue.h"
#include "lib.h"
#include "forward.h"
#include <arpa/inet.h> 
#include <stdbool.h>
#include <string.h>
#include "trie.h"

struct route_table_entry *route_table;
int route_table_len;

struct arp_table_entry *arp_table;
int arp_table_len;

struct trie *trie;

int main(int argc, char *argv[])
{
	char buf[MAX_PACKET_LEN];

	// Do not modify this line
	init(argv + 2, argc - 2);

	// aloc tabela de rutare
	route_table = malloc(sizeof(struct route_table_entry) * 80000);
	DIE(route_table == NULL, "route_table memory");

	// aloc tabela ARP
	arp_table = malloc(sizeof(struct arp_table_entry) * 80000);
	DIE(arp_table == NULL, "arp_table memory");

	// citesc tabela de rutare si tabela ARP
	route_table_len = read_rtable(argv[1], route_table);
	arp_table_len = parse_arp_table("arp_table.txt", arp_table);

	// aloc trie-ul
	trie = init_trie();
	DIE(trie == NULL, "route_table memory");
	trie = create_trie(trie, route_table, route_table_len);

	while (1) {
		size_t interface;
		size_t len;

		interface = recv_from_any_link(buf, &len);
		DIE(interface < 0, "recv_from_any_links");
		printf("Packet received\n");

    // TODO: Implement the router forwarding logic

    /* Note that packets received are in network order,
		any header field which has more than 1 byte will need to be conerted to
		host order. For example, ntohs(eth_hdr->ether_type). The oposite is needed when
		sending a packet on the link, */

		struct ether_hdr *ether_header = (struct ether_hdr *) buf;
		struct ip_hdr *ip_header = (struct ip_hdr *)(buf + sizeof(struct ether_hdr));

		// verific validitatea pachetului IP
		if (ether_header->ethr_type != htons(ETHERTYPE_IP)) {
			printf("Packet type not IPv4\n");
			continue;
		}

		// TODO 1.1: verifica daca el e destinatia
		bool is_dest = false;
		for (int i = 0; i < ROUTER_NUM_INTERFACES; i++) {
			char *ip_char = get_interface_ip(i);
			struct in_addr ip_addr;
			inet_pton(AF_INET, ip_char, &ip_addr.s_addr);  //  #include <arpa/inet.h> int inet_pton(int af, const char *restrict src, void *restrict dst);

			if (ip_header->dest_addr == ip_addr.s_addr) {
				is_dest = true;
			}
		}

		if (is_dest) {
			// pachetul e destinat routerului meu
			printf("mi-a dat, mi-a dat pachet!\n");

			// TODO 1.2: verifica checksum
			int recv_checksum = ip_header->checksum;
			ip_header->checksum = 0;
			int calc_checksum = htons(checksum((uint16_t *)ip_header, sizeof(struct ip_hdr)));
			if (recv_checksum != calc_checksum) {
				printf("Invalid checksum\n");
				printf("recv_checksum: %x, calc_checksum: %x\n", recv_checksum, calc_checksum);
				continue;
			}
			
			// raspund cu echo-reply + implementez icmp


			// implementeaza icmp
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
			printf("-- nu sunt eu destinatia --\n");

			// TODO 1.2: verifica checksum
			int recv_checksum = ip_header->checksum;
			ip_header->checksum = 0;
			int calc_checksum = htons(checksum((uint16_t *)ip_header, sizeof(struct ip_hdr)));
			if (recv_checksum != calc_checksum) {
				printf("Invalid checksum\n");
				printf("recv_checksum: %x, calc_checksum: %x\n", recv_checksum, calc_checksum);
				continue;
			}
			
			// TODO 1.3: verificare si actualizare TTL + implementare ICMP [time limit exceeded]
			ip_header->ttl -= 1;
			if (ip_header->ttl == 0) {
				char original_datagram[sizeof(struct ip_hdr) + 8];
				memcpy(original_datagram, buf + sizeof(struct ether_hdr), sizeof(struct ip_hdr) + 8); 

				// implementeaza icmp
				struct icmp_hdr* icmp_header = (struct icmp_hdr*)(buf + sizeof(struct ether_hdr) + sizeof(struct ip_hdr));
				icmp_header->mtype = 11;
				icmp_header->mcode = 0; 
				icmp_header->un_t.gateway_addr = 0;

				// pun in buf ceea ce aveam din pachetul ip original:
				memcpy(buf + sizeof(struct ether_hdr) + sizeof(struct ip_hdr) + sizeof(struct icmp_hdr), original_datagram, sizeof(struct ip_hdr) + 8);

				icmp_header->check = 0;
				icmp_header->check = htons(checksum((uint16_t*) icmp_header, sizeof(struct icmp_hdr) + sizeof(struct ip_hdr) + 8));

				// resetez adresele ip
				ip_header->dest_addr = ip_header->source_addr;

				struct in_addr source_addr;
				char *current_interface = get_interface_ip(interface);
				inet_pton(AF_INET, current_interface, &source_addr.s_addr);
				ip_header->source_addr = source_addr.s_addr;

				ip_header->tot_len = htons(sizeof(struct ip_hdr) + sizeof(struct icmp_hdr) + sizeof(struct ip_hdr) + 8);

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


				len = sizeof(struct ether_hdr) + 2 * sizeof(struct ip_hdr) + sizeof(struct icmp_hdr) + 8;
				send_to_link(len, buf, interface);
				continue;
			}
		
			// TODO 1.4: LPM pe tabela de rutare + implementare ICMP [destination unreachable]
			// struct route_table_entry *best_route = longest_prefix_match(ip_header, route_table, route_table_len);
			struct route_table_entry *best_route = longest_prefix_match_trie(ip_header, trie);
			if (!best_route) {
				// implementeaza icmp
				char original_datagram[sizeof(struct ip_hdr) + 8];
				memcpy(original_datagram, buf + sizeof(struct ether_hdr), sizeof(struct ip_hdr) + 8); 

				// implementeaza icmp
				struct icmp_hdr* icmp_header = (struct icmp_hdr*)(buf + sizeof(struct ether_hdr) + sizeof(struct ip_hdr));
				icmp_header->mtype = 3;
				icmp_header->mcode = 0; 
				icmp_header->un_t.gateway_addr = 0;

				// pun in buf ceea ce aveam din pachetul ip original:
				memcpy(buf + sizeof(struct ether_hdr) + sizeof(struct ip_hdr) + sizeof(struct icmp_hdr), original_datagram, sizeof(struct ip_hdr) + 8);

				icmp_header->check = 0;
				icmp_header->check = htons(checksum((uint16_t*) icmp_header, sizeof(struct icmp_hdr) + sizeof(struct ip_hdr) + 8));

				// resetez adresele ip
				ip_header->dest_addr = ip_header->source_addr;

				struct in_addr source_addr;
				char *current_interface = get_interface_ip(interface);
				inet_pton(AF_INET, current_interface, &source_addr.s_addr);
				ip_header->source_addr = source_addr.s_addr;

				ip_header->tot_len = htons(sizeof(struct ip_hdr) + sizeof(struct icmp_hdr) + sizeof(struct ip_hdr) + 8);

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


				len = sizeof(struct ether_hdr) + 2 * sizeof(struct ip_hdr) + sizeof(struct icmp_hdr) + 8;
				send_to_link(len, buf, interface);
				continue;
			}

			printf("Route found, send to interface:%d\n", best_route->interface);

			// TODO 1.5: actualizare checksum
			ip_header->checksum = 0;
			ip_header->checksum = htons(checksum((uint16_t *)ip_header, sizeof(struct ip_hdr)));

			// TODO 1.6: rescriere adrese ethernet
			// adresa sursa
			get_interface_mac(best_route->interface, ether_header->ethr_shost);

			// adresa destinatie
			struct arp_table_entry * mac_entry = get_mac_entry(best_route->next_hop, arp_table, arp_table_len);
			if (!mac_entry) {
				printf("eroare la gasirea adresei MAC\n");
				continue;
			}
			
			memcpy(ether_header->ethr_dhost, mac_entry->mac, 6);
			printf("am gasit MAC, trimit pachetul\n");

			// TODO 1.7: trimiterea noului pachet pe interfata corespunzatoare urmatorului hop
			printf("trimit pe interfata %d, next hop din tabela\n", best_route->interface);
			send_to_link(len, buf, best_route->interface);

		}
	}

	free(route_table);
	free(arp_table);
	destroy_trie(&trie);
}

/*

    printf("ICMP type primit: %d\n", icmp_header->mtype);
    printf("ICMP code primit: %d\n", icmp_header->mcode);
    printf("IP src: %s\n", inet_ntoa(*(struct in_addr*)&ip_header->source_addr));
    printf("IP dst: %s\n", inet_ntoa(*(struct in_addr*)&ip_header->dest_addr));
    printf("MAC src primit: %02x:%02x:%02x:%02x:%02x:%02x\n",
        ether_header->ethr_shost[0], ether_header->ethr_shost[1],
        ether_header->ethr_shost[2], ether_header->ethr_shost[3],
        ether_header->ethr_shost[4], ether_header->ethr_shost[5]);
    printf("Interface primit: %zu\n", interface);



	printf("=== REPLY ===\n");
    printf("IP src reply: %s\n", inet_ntoa(*(struct in_addr*)&ip_header->source_addr));
    printf("IP dst reply: %s\n", inet_ntoa(*(struct in_addr*)&ip_header->dest_addr));
    printf("MAC dst reply: %02x:%02x:%02x:%02x:%02x:%02x\n",
        ether_header->ethr_dhost[0], ether_header->ethr_dhost[1],
        ether_header->ethr_dhost[2], ether_header->ethr_dhost[3],
        ether_header->ethr_dhost[4], ether_header->ethr_dhost[5]);
    printf("Trimit pe interface: %zu\n", interface);
    printf("len: %zu\n", len);

*/