//data_structure_core.h

#ifndef _DATA_STRUCTURE_CORE_H
#define _DATA_STRUCTURE_CORE_H

#include <linux/types.h>        /* uint64_t */

#define MASK_INVALIDATE 0X8000000000000000
#define MASK_NUMINSERT 0X7FFFFFFFFFFFFFFF



/**
 * @block_index: Indice del blocco libero
 * @next: Puntatore all'elemento successivo nella lista dei blocchi liberi
 *
 * Rappresenta un singolo elemento che viene inserito all'interno della lista
 * free_block_list e mantiene le informazioni su un blocco libero.
 */
struct block_free {
    uint64_t block_index;
    struct block_free *next;
};



/* Questa struttura dati viene utilizzata per la gestione del Grace Period */
struct grace_period {
    unsigned long standing_sorted[EPOCHS];
    unsigned long epoch_sorted;
    int next_epoch_index_sorted;
};



extern int is_free;
extern uint64_t num_block_free_used;
extern struct block_free *head_free_block_list;                 /* Puntatore alla testa della lista contenente i blocchi liberi. */
extern struct grace_period *gp;



extern int init_data_structure_core(uint64_t num_data_block, uint64_t *index_free, uint64_t actual_size);
extern int check_bit(uint64_t index);
extern int set_bitmask(uint64_t index, int mode);
extern int get_bitmask_block(void);
extern int insert_new_data_block(uint64_t index, char * source, size_t msg_size);
extern int invalidate_data_block(uint64_t index);
extern void insert_free_list_conc(struct block_free *item);
extern struct block_free * get_freelist_head(void);
extern struct buffer_head *get_block(uint64_t index);
extern struct buffer_head *get_sb_block(void);

#endif //data_structure_core.h
