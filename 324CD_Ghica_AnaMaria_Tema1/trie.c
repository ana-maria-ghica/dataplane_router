#include "protocols.h"
#include "trie.h"
#include "queue.h"
#include "lib.h"
#include <arpa/inet.h> 


struct trie* init_trie() {
    struct trie* root = malloc(sizeof(struct trie));
    if (!root) {
        printf("Eroare la alocarea trie\n");
        return NULL;
    }
    root->bit = 0;
    root->left = NULL;
    root->right = NULL;
    root->rtable_entry = NULL;

    return root;
}

struct trie* create_node(int bit_value) {
    struct trie* node = malloc(sizeof(struct trie));
    if (!node) {
        printf("Eroare la alocarea node\n");
        return NULL;
    }
    node->bit = bit_value;
    node->left = NULL;
    node->right = NULL;
    node->rtable_entry = NULL;

    return node;
}

int count_bits(uint32_t mask) {
    int count = 0;
    for (int i = 0; i < 32; i++) {
        int current_bit = (mask >> i) & 1;
        if (current_bit == 1) {
            count++;
        }
    }
    return count;
}

struct trie* add_node(struct trie* root, struct route_table_entry* route_table_entry) {
    
    uint32_t prefix = ntohl(route_table_entry->prefix);
    uint32_t mask = ntohl(route_table_entry->mask);
    int num_bits = 32 - count_bits(mask); // ca sa vad cati biti are rost sa parcurg din prefix

    if (root == NULL) {
        root = init_trie();
    }

    struct trie* iter = root;
    for (int i = 31; i >= num_bits; i--) {
        // trebuie sa pornesc de la MSB
        uint8_t bit = (prefix >> i) & 1;

        if (bit == 0) {

            // merg pe stanga
            if (iter != NULL && iter->left != NULL) {
                // nodul exista deja in trie, parcurg in continuare
                iter = iter->left;

            } else {
                // nodul nu exista in trie, asa ca trebuie sa il creez
                iter->left = create_node(0);
                iter = iter->left;
            }

        } else {

            // merg pe dreapta
            if (iter != NULL && iter->right != NULL) {
                // nodul exista deja in trie, parcurg in continuare
                iter = iter->right;

            } else {
                // nodul nu exista in trie, asa ca trebuie sa il creez
                iter->right = create_node(1);
                iter = iter->right;
            }
        }

        // setez ca am creat un prefix valid
        // la final de prefix, salvez datele din tabela de rutare, care ma ajuta la LPM
        if (i == num_bits) {
            iter->valid_prefix = true;
            iter->rtable_entry = route_table_entry;
        }
    }

    return root;
}

struct trie* create_trie(struct trie* root, struct route_table_entry* route_table, int route_table_len) {
    if (root == NULL) {
        root = init_trie();
    }

    for (int i = 0; i < route_table_len; i++) {
        add_node(root, &route_table[i]);
    }
    return root;
}

void destroy_trie(struct trie** trie) {
    struct trie *iter = *trie;
    if (iter == NULL) {
        return;
    }

    destroy_trie(&iter->left);
    destroy_trie(&iter->right);

    free(iter);
    *trie = NULL;
}
