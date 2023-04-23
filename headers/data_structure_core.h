//data_structure_core.h

#ifndef _DATA_STRUCTURE_CORE_H
#define _DATA_STRUCTURE_CORE_H

#include <linux/types.h>        /* uint64_t */

#define MASK_INVALIDATE 0X8000000000000000
#define MASK_NUMINSERT 0X7FFFFFFFFFFFFFFF


/*
 * La struttura dati 'block' rappresenta un singolo
 * elemento che viene inserito all'interno di una delle
 * liste nella hash_table_valid e all'interno della lista
 * sorted_list. Rappresenta un blocco del device che
 * mantiene un messaggio valido.
 */
struct block {
    uint64_t block_index;
    uint64_t pos;      
    char *msg;
    struct block* hash_table_next;
    struct block* sorted_list_next;
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
    struct block *head_list;
};



struct grace_period {
    unsigned long standing_ht[EPOCHS];
    unsigned long epoch_ht;
    unsigned long standing_sorted[EPOCHS];
    unsigned long epoch_sorted;
    int next_epoch_index_ht;
    int next_epoch_index_sorted;
};



extern struct block *head_sorted_list;                          /* Puntatore alla testa della lista contenente i blocchi nell'ordine di consegna. */
extern struct block_free *head_free_block_list;                 /* Puntatore alla testa della lista contenente i blocchi liberi. */
extern struct ht_valid_entry *hash_table_valid;                 /* Hash table */
extern struct grace_period *gp;

extern uint64_t **bitmask;
extern uint64_t num_block_free_used;                            /* Numero di blocchi liberi (istante t = 0) utilizzati. */
extern uint64_t empty_actual_size;                              /* Il numero massimo di blocchi che carico quando la free list si svuota. */
extern int x;                                                   /* Numero di entry della hash table. */

extern void compute_num_rows(uint64_t num_data_block);
extern int init_data_structure_core(uint64_t num_data_block, uint64_t *index_free, uint64_t actual_size);
extern int check_bit(uint64_t index);
extern struct block_free * get_freelist_head(void);
extern void set_bitmask(uint64_t index, int mode);
extern int get_bitmask_block(void);
extern int insert_hash_table_valid_and_sorted_list_conc(char *data_block_msg, uint64_t pos, uint64_t index, struct block_free *bf);
extern int invalidate_block(uint64_t index);

#endif //data_structure_core.h
