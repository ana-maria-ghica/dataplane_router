TEMA 1 - PCOM
GHICA ANA-MARIA, 324CD

**Ideea de rezolvare:**
La inceputul programului, initializez tabela de rutare, aloc trie-ul si cache-ul pentru ARP. Codul prezinta si tabela statica ARP (initializarea ei este comentata, intrucat am implementat ulterior ARP dinamic), pentru a reprezenta parcursul pe care l-am avut in rezolvarea cerintelor.

O data ce am primit un pachet din retea, selectez headerul Ethernet, si fac urmatoarele verificari:
- verific daca pachetul e destinat routerului meu sau daca e broadcast
- verific validitatea pachetului IP/ARP

De aici, daca a trecut verificarile, logica programului se imparte in 2 (dupa tipul pachetului):
  -Daca e pachet IPv4:
    -daca destinatia e in subretea: pachetul primit e ECHO Request, deci eu construiesc si trimit pachet ECHO Reply
    -daca nu e destinatia in subretea, ii fac forward catre alt router: 
      -daca pachetul a expirat, implementez "ICMP time exceeded" (trimit pachet tip ICMP)
      -daca am cautat in tabela de rutare si nu gasesc urmatoarea destinatie, implementez "ICMP destination unreachable" (trimit pachet tip ICMP)
      -daca pachetul a trecut toate verificarile de mai sus, caut in ARP Cache adresa MAC pentru next_hop;
        -daca adresa MAC era deja in cache, setez campurile in header-ul Ethernet si forwardez pachetul
        -daca adresa MAC nu era in cache, salvez pachetul curent in coada, pentru a putea construi si trimite pachet ARP request in retea; dupa ce voi primi raspunsul (pachet ARP reply, atunci voi completa header-ul Ethernet), voi salva intrarea ARP in cache si voi forwarda pachetul

  -Daca e pachet ARP:
    -daca e pachet ARP Request:
      -interschimb adresele IP (source, target), setez adresele MAC si trimit pachetul inapoi de unde a venit
    -daca e pachet ARP Reply:
      -salvez adresa MAC primita in ARP cache, caut in fiecare pachet din coada daca MAC-ul primit corespunde cu adresa next_hop-ului la care trebuie sa mearga, daca adresele corespund atunci actualizez header-ul Ethernet si trimit pachetul (daca adresele nu corespund, doar adaug pachetul inapoi in coada si continui verificarile; am grija sa nu ciclez la infinit in coada, de asta verific doar un nr finit de pachete, egal cu dimensiunea cozii)
