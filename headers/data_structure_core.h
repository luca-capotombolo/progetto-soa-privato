//data_structure_core.h

#ifndef _DATA_STRUCTURE_CORE_H
#define _DATA_STRUCTURE_CORE_H

#include <linux/types.h>        /* uint64_t */


/*
 * La struttura dati 'block' rappresenta un singolo
 * elemento che viene inserito all'interno di una delle
 * liste nella hash_table_valid e all'interno della lista
 * sorted_list. Rappresenta un blocco del device che
 * mantiene un messaggio valido.
 */
struct block {
    uint64_t block_index;   //
    uint64_t pos;           
    char *msg;      //
    struct block* hash_table_next;  //
    struct block* sorted_list_next; //
};



/*
 * La struttura dati 'block_free' rappresenta un singolo
 * elemento che viene inserito all'interno della lista
 * free_block_list che mantiene le informazioni su un
 * blocco libero.
 */
struct block_free {
    uint64_t block_index;
    struct block_free *next;
};



/*
 * La struttura dati 'ht_valid_entry' rappresenta una
 * entry della tabella hash 'hash_table_valid'.
 */
struct ht_valid_entry {
    unsigned long standing[EPOCHS];
    unsigned long epoch;
//    struct mutex entry_mutex;
    struct block *head_list;
    int next_epoch_index;
};



extern struct block *head_sorted_list;                          /* Puntatore alla testa della lista contenente i blocchi nell'ordine di consegna. */
extern struct block *tail_sorted_list;                          /* Puntatore alla coda della lista contenente i blocchi nell'ordine di consegna. */
extern struct block_free *head_free_block_list;                 /* Puntatore alla testa della lista contenente i blocchi liberi. */
extern struct ht_valid_entry *hash_table_valid;                 /* Hash table */
extern uint64_t **bitmask;
extern int x;

extern void compute_num_rows(uint64_t num_data_block);
extern int init_data_structure_core(uint64_t num_data_block, uint64_t *index_free, uint64_t actual_size);
extern int check_bit(uint64_t index);

#endif //data_structure_core.h
