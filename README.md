Repository for the first homework of the Communication Networks class. In this homework the students will implement the dataplane of a router.

pentru forwardarea in cazul in care routerul trimite direct la mine in retea (adica nu mai departe in alta retea)
aka adresa ip a pachetului corespunde cu adresa ip a retelei/subretelei mele

1. Primești pachetul — MAC destinație = MAC-ul tău ✅ → îl accepți
2. Dezambalezi — scoți cadrul Ethernet, rămâi cu pachetul IP în mână
3. Te uiți în tabela de rutare — vezi adresa_retea din IP-ul destinației
4. Constați că destinația e în subretea ta → nu mai forwardezi la alt router
5. ARP — "Cine are IP-ul 192.168.1.x? Dă-mi MAC-ul tău"
6. Construiești un cadru Ethernet nou cu MAC-ul destinației și trimiți direct

Un detaliu important pe care l-ai prins intuitiv
Observă că la pasul 6 construiești un cadru Ethernet nou — nu îl trimiți pe cel vechi. Cadrul Ethernet original a "murit" la tine. Tu ca router creezi unul proaspăt cu:
MAC sursă = MAC-ul tău
MAC destinație = MAC-ul gazdei finale
Pachetul IP din interior rămâne neschimbat (același IP sursă și destinație), doar învelișul L2 se schimbă.

[Faza L3 - IP]   → tabela de rutare → destinația e locală
[Faza L2 - ETH]  → ARP → cadru nou → livrare fizică directă



-----------------------------------------------------------------------arp
Format mesaje ARP – conceput pentru diverse categorii de
adrese
· Hardware type: 1 pentru Ethernet
· Protocol type: 0x0800 pentru IP (0000.1000.0000.0000)
· HLEN - Hardware len: 6 octeti pentru Ethernet
· PLEN - Protocol len: 4 octeti pentru IP
· Operation: 1=cerere, 2=raspuns


Tema implementare:
- prima oara verific daca pachetul primit este malformat (daca este il arunc)
- verific daca adresa mac destinatie a pachetului corespunde adresei interfetei pe care a venit pachetul
sau daca e arp request iar adresa mac destinatie e de tip broadcast
- dupa inspectez fiecare header din pachet
- daca este pachet de tip arp avem 2 cazuri:
          - fie este un arp request -> trimit un arp reply inapoi, completand adresa mac pe care nu o stiam
          - fie este un arp reply -> adaug adresa mac pe care nu o stiam in tabela arp si trimit pachetul din coada
- daca este pachet de tip ip:
          - verific daca contine un mesaj de tip icmp echo request -> trimit inapoi un icmp echo reply
          - verific checksum-ul pachetului
          - verific ttl-ul pachetului si daca e prea mic, creez un pachet de tip icmp time exceeded
          - verific daca gasesc o ruta pentru pachetul primit in tabela de rutare
                    - daca nu gasesc o ruta, creez un pachet de tip icmp destination unreachable
          - verific daca gasesc adresa mac a urmatorului hop (gasit la pasul anterior) in tabela arp
                    - daca nu gasesc, creez un arp request si pun pachetul in coada
                    - in coada de pachete retin continutul pachetului, lungimea lui si informatiile despre urmatorul hop
                      pentru a nu mai apela din nou mai tarziu functia de gasire a urmatorului hop
          - daca am gasit adresa mac a urmatorului hop in tabela arp
                    - nu mai e nevoie de coada de pachete, trimit pachetul direct catre urmatorul hop
- algoritmul lpm pentru gasirea celei mai bune rute am facut-o cu cautare binara
      - mai intai am sortat cu quicksort tabela de rutare (crescator dupa masca si daca sunt egale
      mastile, crescator dupa prefix) (noua ne trebuie ruta cu masca cea mai mare)
      - apoi am cautat cu cautare binara in tabela de rutare
              - cand dau de un prefix si o masca care sa respecte conditia din if: prefix == dest_ip & mask
                caut si liniar intre left si right pentru a ma asigura ca gasesc prefixul corect cu cea mai mare masca