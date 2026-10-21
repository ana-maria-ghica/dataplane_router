# Dataplane Router

The forwarding plane of an IPv4 router, written in C. The program receives raw Ethernet frames on several interfaces and decides, packet by packet, what to do with each one: forward it towards its destination, answer it itself, or report an error back to the sender. Doing that involves three protocols working together. IPv4 decides where a packet goes, ARP finds out which physical machine is the next step, and ICMP tells the sender when something goes wrong.

I wrote it for the Network Protocols course at POLITEHNICA Bucharest (spring 2026). The course provided the low-level library for sending and receiving frames on the router's interfaces, the protocol header definitions, the routing tables and a Mininet topology for testing. The forwarding logic, the routing lookup and the ARP and ICMP handling are my implementation.

## What happens to a packet

Every frame goes through the same sequence of decisions.

First, the router checks whether the frame is meant for it at all. At the Ethernet level that means the destination MAC address is either the address of the interface it arrived on, or the broadcast address. Anything else is dropped, along with frames that are neither IPv4 nor ARP.

For an **IPv4 packet**, the router verifies the header checksum and then looks at the destination address. If the packet is addressed to the router itself, it is a ping, and the router answers with an ICMP Echo Reply. Otherwise the packet is in transit and has to be forwarded. The router decrements its TTL, recomputes the checksum (since the header has changed), finds the best route in the routing table, finds the MAC address of the next hop, rewrites the Ethernet header and sends the frame out on the right interface.

For an **ARP packet**, the router either answers a request for one of its own addresses, or, if it is a reply, learns a new IP to MAC mapping and releases the packets that were waiting for it.

## Routing: longest prefix match with a trie

The routing table holds over 64,000 entries, each a network prefix with a mask, a next hop and an outgoing interface. Several entries can match the same destination, for example `10.0.0.0/8` and `10.1.2.0/24`, and the rule is to choose the most specific one, the match with the longest prefix.

A linear scan of the table works (an earlier version of the code, `longest_prefix_match`, is still there for comparison), but it costs 64,000 comparisons for every single packet. The router instead stores the table in a **binary trie**. Each route is inserted by walking the bits of its prefix from the most significant one, going left for 0 and right for 1, and attaching the route to the node where the prefix ends. To route a packet, the router walks the bits of the destination address down the same tree and remembers the last route it passed. When the path ends, that route is the longest match.

The lookup now costs at most 32 steps, one per bit of an IPv4 address, regardless of how large the table is. This is the same basic idea that real routers build on, although they use compressed variants of the trie to save memory.

## ARP: finding the next hop

Knowing that a packet must go to next hop `192.168.1.2` is not enough to send it. The Ethernet frame needs that machine's MAC address. The router keeps an ARP cache of the mappings it has already learned. When the next hop is not in the cache yet, the router cannot simply drop the packet or block while it waits. Instead:

1. the packet is copied into a queue, together with its length;
2. the router broadcasts an ARP Request, asking who has the next hop's IP address;
3. when the ARP Reply arrives, the new mapping is added to the cache, and the router goes through the queue once, sending every packet whose next hop is the address that just answered and putting the others back.

Going through the queue exactly once, by counting its elements before starting, guarantees that the loop ends even when some packets are still waiting for other replies.

In the other direction, when another machine asks for the MAC address of one of the router's interfaces, the router turns the request into a reply in place: it swaps the sender and target fields, fills in its own MAC address and sends the frame back.

## ICMP: reporting errors

A router that silently drops packets makes networks very hard to debug, so three situations produce an ICMP message:

- **Echo Reply** (type 0), when the router itself is pinged;
- **Time Exceeded** (type 11), when a packet's TTL runs out. This prevents packets from circling forever in a routing loop, and it is also the mechanism `traceroute` relies on;
- **Destination Unreachable** (type 3), when no route matches the destination.

In each case the router builds the reply from the packet it received: it swaps the IP and MAC addresses, sets a fresh TTL, fills in the ICMP type and recomputes both checksums.

## Building and running

```
make
make run_router0      # router 0, with rtable0.txt
make run_router1      # router 1, with rtable1.txt
```

The routers are meant to run inside the course's Mininet topology (`checker/topo.py`): two routers connected to each other, each with two hosts on its own subnets. `checker/checker.sh` starts the topology and runs the automated tests.

## Testing with Wireshark

Besides the automated tests, I checked each feature by hand in the topology and captured the traffic in Wireshark:

- **forwarding**: a `ping` between hosts in different subnets, with requests and replies crossing both routers;
- **ARP**: an `arping` capture showing ARP requests being broadcast and answered on the link;
- **ICMP errors**: a ping to an address with no route, which returns Destination Unreachable, and a ping with `-t 1`, which returns Time Exceeded from the first router.

The captures are in `screenshots/`.

## Limitations

- **ICMP error messages carry no payload.** RFC 792 requires them to include the IP header and the first 8 bytes of the original packet, so that the sender can tell which of its packets caused the error. Here the original data is overwritten, which is enough for `ping` but would not let a TCP or UDP sender match the error to a connection.
- **The default route is not supported.** A `0.0.0.0/0` entry has an empty prefix, so it ends at the root of the trie, where no route is stored. The routing tables used in the tests do not contain one.
- **The ARP cache never expires.** Entries are kept forever and the cache has a fixed capacity, whereas a real router ages out old mappings so that it notices when a machine changes its MAC address.

## Project structure

```
router.c         main loop: frame validation, IPv4 forwarding, ARP and ICMP handling
trie.c, trie.h   binary trie for longest prefix match
forward.c        route lookup (trie and linear), ARP cache lookup, ARP request construction
include/, lib/   course library: link-level I/O, protocol headers, queue and list
rtable0.txt      routing table of router 0 (rtable1.txt for router 1)
checker/         Mininet topology and automated tests
```
