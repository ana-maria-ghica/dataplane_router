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
	route_table_len = read_rtable("rtable0.txt", route_table); // si trable1.txt ?? (2)
	arp_table_len = parse_arp_table("arp_table.txt", arp_table);


	while (1) {

		size_t interface;
		size_t len;

		interface = recv_from_any_link(buf, &len);
		DIE(interface < 0, "recv_from_any_links");
		printf("mi-a dat, mi-a dat pachet!\n");

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
		if (ether_header->ethr_type != ntohs(ETHERTYPE_IP)) {
			printf("nu e pachet de tip IPv4\n");
			continue;
		}

		// de aici: verificarile partilor de header din antetul IPv4 ---------------------------- forward process

		// TODO 1.1: verifica daca el e destinatia
		// daca e destinatia, atunci face anumite lucruri
		// daca nu e destinatia, atunci se prelucreaza toate campurile de mai jos si se trimite mai departe

		// pentru toate cele 4 interfete de retea
		bool is_dest = false;
		for (int i = 0; i <= 3; i++) {
			unsigned char buf[sizeof(struct in6_addr)];
			int ip_interface = inet_pton(4, get_interface_ip(i), buf);
			if (ip_header->dest_addr == ip_interface)
				is_dest = true;
		}

		if (is_dest) {
			// procesez pachetul pt ca e al meu
			// "routerul va răspunde doar la mesaje de tip ICMP"

			// TODO 1.2: verifica checksum
			ip_header->checksum = 0;
			if (checksum((uint16_t *)ip_header, sizeof(struct ip_hdr)) != 0) {
				printf("checksum invalid, diferit de 0\n");
				continue;
			}

		} else {
			// TODO 1.2: verifica checksum
			ip_header->checksum = 0;
			if (checksum((uint16_t *)ip_header, sizeof(struct ip_hdr)) != 0) {
				printf("checksum invalid, diferit de 0\n");
				continue;
			}
			
			// TODO 1.3: verificare si actualizare TTL
			ip_header->ttl -= 1;
			if (ip_header->ttl < 1) {
				printf("ttl == 0, pachetul a murit\n");
				continue;
			}

			// TODO 1.4: cautare in tabela de rutare 
			struct route_table_entry *best_route = longest_prefix_match(ip_header, route_table, route_table_len);
			if (!best_route) {
				printf("eroare la best route\n");
				continue;
			}

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


			// TODO 1.7: trimiterea noului pachet pe interfata corespunzatoare urmatorului hop
			send_to_link(best_route->interface, buf, len);
		}
	}

	free(route_table);
	free(arp_table);
}
