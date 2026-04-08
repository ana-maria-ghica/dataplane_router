#ifndef _TRIE_H
#define _TRIE_H

#include <unistd.h>
#include <stdint.h>
#include "lib.h"
#include <arpa/inet.h> 

struct trie {
  uint8_t bit;
  struct trie* left;
  struct trie* right;
  struct route_table_entry* rtable_entry;
};

struct trie* init_trie();
struct trie* create_node(int bit_value);
int count_bits(uint32_t mask);
struct trie* add_node(struct trie* root, struct route_table_entry* route_table_entry);
struct trie* create_trie(struct trie* root, struct route_table_entry* route_table, int route_table_len);
void destroy_trie(struct trie** trie);

#endif