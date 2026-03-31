#include "protocols.h"
#include "queue.h"
#include "lib.h"
#include "forward.h"
#include <arpa/inet.h> 
#include <stdbool.h>
#include <string.h>

/*
intrebari: (1), (2)
*/

struct route_table_entry *route_table;
int route_table_len;
struct arp_table_entry *arp_table;
int arp_table_len;

int main(int argc, char *argv[])
{
	char buf[MAX_PACKET_LEN];

	// Do not modify this line
	init(argv + 2, argc - 2);

	// aloc tabela de rutare
	route_table = malloc(sizeof(struct route_table_entry) * 80000);  // --> nr de linii din fisierul de intrare?? (1)
	DIE(route_table == NULL, "route_table memory");

	// aloc tabela ARP
	arp_table = malloc(sizeof(struct arp_table_entry) * 80000);
	DIE(arp_table == NULL, "arp_table memory");

	// citesc tabela de rutare si tabela ARP
	route_table_len = read_rtable(argv[1], route_table); // si trable1.txt ?? (2)
	arp_table_len = parse_arp_table("arp_table.txt", arp_table);


	while (1) {

		size_t interface;
		size_t len;

		interface = recv_from_any_link(buf, &len);
		DIE(interface < 0, "recv_from_any_links");
		printf("packet received\n");

    // TODO: Implement the router forwarding logic

    /* Note that packets received are in network order,
		any header field which has more than 1 byte will need to be conerted to
		host order. For example, ntohs(eth_hdr->ether_type). The oposite is needed when
		sending a packet on the link, */

		// extrag header-ul Ethernet (din protocoalele encapsulate)
		// prima parte a memoriei == header-ul ethernet
		// in urmatoarea parte a memoriei == header-ul IP
		struct ether_hdr *ether_header = (struct ether_hdr *) buf;
		struct ip_hdr *ip_header = (struct ip_hdr *)(buf + sizeof(struct ether_hdr));

		// verific daca am primit un pachet IP valid
		if (ether_header->ethr_type != htons(ETHERTYPE_IP)) {
			printf("not IPv4 type\n");
			continue;
		}

		// de aici: verificarile partilor de header din antetul IPv4 ---------------------------- forward process

		// TODO 1.1: verifica daca el e destinatia
		// daca e destinatia, atunci face anumite lucruri
		// daca nu e destinatia, atunci se prelucreaza toate campurile de mai jos si se trimite mai departe

		// pentru toate cele 4 interfete de retea
		bool is_dest = false;
		for (int i = 0; i < 3; i++) {
			unsigned char ip_buf[sizeof(struct in_addr)];
			inet_pton(4, get_interface_ip(i), ip_buf);  //  #include <arpa/inet.h> int inet_pton(int af, const char *restrict src, void *restrict dst);
			if (memcmp(&ip_header->dest_addr, ip_buf, 4) == 0) {
				is_dest = true;
			}
		}

		if (is_dest) {
			
			// procesez pachetul pt ca e al meu
			// "routerul va răspunde doar la mesaje de tip ICMP"

			// TODO 1.2: verifica checksum
			if (checksum((uint16_t *)ip_header, sizeof(struct ip_hdr)) != 0) {
				printf("checksum invalid, diferit de 0\n");
				continue;
			}
			
			// daca arata checksum gresit
			// uint16_t csum = checksum((uint16_t *)ip_header, sizeof(struct ip_hdr));
			// printf("checksum result: %d\n", csum);

			// raspunde si router ul cu echo reply

		} else {
			printf("nu sunt eu destinatia\n");
			// TODO 1.2: verifica checksum
			if (checksum((uint16_t *)ip_header, sizeof(struct ip_hdr)) != 0) {
				printf("checksum invalid, diferit de 0\n");
				continue;
			}
			
			// TODO 1.3: verificare si actualizare TTL + include mesaj ICMP
			if (ip_header->ttl <= 1) {
				// structura noua pe care o are pachetul ICMP:
				// [Ethernet][Ip nou][ICMP header][IP original][8 bytes din payload]

				// declar header-ul ICMP care pointeaza in "buf" la memoria corespunzatoare
				struct icmp_hdr *icmp_header = (struct icmp_hdr*)(buf + sizeof(struct ether_hdr) + sizeof(struct ip_hdr));

				// adresa eth destinatie <-> adresa eth sursa
				uint8_t eth_dest[6], eth_src[6];
				memcpy(eth_dest, ether_header->ethr_dhost, 6);
				memcpy(eth_src, ether_header->ethr_shost, 6);

				memcpy(ether_header->ethr_dhost, eth_src, 6);
				memcpy(ether_header->ethr_shost, eth_dest, 6);

				// salvez header-ul ip original
				char ip_original[sizeof(struct ip_hdr) + 8];  // modifica in atribuire de pointer gen char *ip_original = buf + sizeof(struct ip_hdr) + 8
				memcpy(ip_original, ip_header, sizeof(struct ip_hdr) + 8);

				// modific campurile din header-ul IP pentru a trimite pachetul inapoi
				ip_header->dest_addr = ip_header->source_addr;
				inet_pton(4, get_interface_ip(interface), &ip_header->source_addr);
				ip_header->proto = 1;
				ip_header->ttl = 64;
				ip_header->tot_len = htons(sizeof(struct ip_hdr) + sizeof(struct icmp_hdr) + sizeof(struct ip_hdr) + 8);
				ip_header->checksum = 0;
				ip_header->checksum = checksum((uint16_t *)ip_header, sizeof(struct ip_hdr));

				// copiez header-ul ip original in pachetul icmp
				memcpy(buf + sizeof(struct ether_hdr) + sizeof(struct ip_hdr) + sizeof(struct icmp_hdr), ip_original, sizeof(struct ip_hdr) + 8);

				// modific campurile din header-ul ICMP pentru a trimite eroarea specifica
				icmp_header->mtype = 11;
				icmp_header->mcode = 0;
				icmp_header->un_t.gateway_addr = 0;
				icmp_header->check = 0;
				icmp_header->check = checksum((uint16_t*)icmp_header, sizeof(struct icmp_hdr) + sizeof(struct ip_hdr) + 8);

				// trimit noul pachet facut
				len = sizeof(struct ether_hdr) + 2 * sizeof(struct ip_hdr) + sizeof(struct icmp_hdr) + 8;
				send_to_link(len, buf, interface);
				continue;
			}
			ip_header->ttl -= 1;

			// TODO 1.4: cautare in tabela de rutare + include mesaj ICMP (cred)
			struct route_table_entry *best_route = longest_prefix_match(ip_header, route_table, route_table_len);
			if (!best_route) {
				// printf("eroare la best route\n");
				send_to_link(len, buf, interface);
				continue;
			}
			printf("am gasit ruta, trimit pe interfata %d\n", best_route->interface);

			// TODO 1.5: actualizare checksum
			ip_header->checksum = 0;
			ip_header->checksum = checksum((uint16_t *)ip_header, sizeof(struct ip_hdr));

			// TODO 1.6: rescriere adrese L2 -> cele care tin de Ethernet !! => adresa mac
			/*
			adresa sursă va fi adresa interfeței routerului pe care pachetul e trimis mai departe, iar adresa destinație va fi adresa MAC a următorului hop. 
			Pentru a determina adresa următorului hop, routerul folosește protocolul ARP.*/

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
}
