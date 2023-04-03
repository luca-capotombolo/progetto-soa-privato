#include <linux/buffer_head.h>  /* sb_bread()-brelse() */
#include <linux/string.h>       /* strncpy() */
#include <linux/slab.h>         /* kmalloc() */

#include "../headers/main_header.h"



struct block *head_sorted_list = NULL;                          /* Puntatore alla testa della lista contenente i blocchi nell'ordine di consegna. */
struct block *tail_sorted_list = NULL;                          /* Puntatore alla coda della lista contenente i blocchi nell'ordine di consegna.  */
struct block_free *head_free_block_list = NULL;                 /* Puntatore alla testa della lista contenente i blocchi liberi. */
struct ht_valid_entry *hash_table_valid = NULL;                 /* Hash table */
uint64_t num_block_free_used = 0;
uint64_t pos = 0;
uint64_t**bitmask = NULL; 





void scan_free_list(void)
{
    struct block_free *curr;

    curr = head_free_block_list;

    printk("%s: ------------------------------INIZIO FREE LIST------------------------------------------", MOD_NAME);

    while(curr!=NULL)
    {
        printk("Blocco #%lld\n", curr->block_index);
        curr = curr->next;
    }

    printk("%s: ----------------------------FINE FREE LIST-------------------------------------------", MOD_NAME);
}



void scan_sorted_list(void)
{
    struct block *curr;

    curr = head_sorted_list;
    
    printk("%s: ----------------------------------INIZIO SORTED LIST  ---------------------------------------------", MOD_NAME);

    while(curr!=NULL)
    {
        printk("Blocco #%lld - Messaggio %s\n", curr->block_index, curr->msg);
        curr = curr->sorted_list_next;
    }

    printk("%s: --------------------------------FINE SORTED LIST -----------------------------------------", MOD_NAME);
    
}



void scan_hash_table(int x)
{
    int entry_num;
    struct ht_valid_entry entry;
    struct block *item;
    
    printk("%s:-------------------------- INIZIO HASH TABLE --------------------------------------------------\n", MOD_NAME);

    for(entry_num=0; entry_num<x; entry_num++)
    {
        printk("%s: ---------------------------------------------------------------------------------", MOD_NAME);
        entry = hash_table_valid[entry_num];
        item = entry.head_list;

        while(item!=NULL)
        {
            printk("%s: Blocco #%lld\n", MOD_NAME, item->block_index);
            item = item ->hash_table_next;
        }

        printk("%s: ---------------------------------------------------------------------------------", MOD_NAME);
        
    }

    printk("%s: -------------------------- FINE HASH TABLE --------------------------------------------------\n", MOD_NAME);
}



void debugging_init(int x)
{
    /* scansione della lista contenente le informazioni dei blocchi liberi. */
    scan_free_list();

    /* scansione della lista contenente i blocchi ordinati per inserimento. */
    scan_sorted_list();

    /* scansione delle liste della hash table */
    scan_hash_table(x);
}



/*
 * Inserisce un nuovo elemento all'interno della
 * lista contenente i blocchi ordinati secondo
 * l'ordine di consegna. L'elemento non deve essere
 * nuovamente allocato ma si collega l'elemento che 
 * è stato precedentemente allocato.
 * Il campo 'pos' rappresenta la posizione del blocco
 * nella lista ordinata e viene sfruttato per determinare
 * la corretta posizione all'interno della lista.
 */
void insert_sorted_list(struct block *block)
{
    struct block *prev;
    struct block *curr;

    if(head_sorted_list == NULL)
    {
        /* La lista è vuota */
        head_sorted_list = block;
        block->sorted_list_next = NULL;
    }
    else
    {
        if( (head_sorted_list->pos) > (block->pos) )
        {
            /* Inserimento in testa */
            block->sorted_list_next = head_sorted_list;
            head_sorted_list = block;
        }
        else
        {
            /* Cerco la posizione per inserire il nuovo elemento */
            prev = head_sorted_list;
            curr = head_sorted_list->sorted_list_next;

            while(curr!=NULL)
            {
                if((curr->pos) > (block->pos))
                {
                    break;
                }
                prev = curr;
                curr = curr->sorted_list_next;
            }
            
            block->sorted_list_next = curr;
            prev -> sorted_list_next = block;        
        }   
    }
}




/*
 * Inserisce un nuovo elemento all'interno della lista
 * identificata dal parametro 'x'.
 */
int insert_hash_table_valid(struct soafs_block *data_block, uint64_t pos, uint64_t index, int x)
{
    int num_entry_ht;
    struct block *new_item;
    struct block *old_head;
    struct ht_valid_entry *ht_entry; 

    /* Identifico la lista corretta nella hash table per effettuare l'inserimento */
    num_entry_ht = index % x;
    ht_entry = &(hash_table_valid[num_entry_ht]);

    /* Alloco il nuovo elemento da inserire nella lista */
    new_item = (struct block *)kmalloc(sizeof(struct block), GFP_KERNEL);
    
    if(new_item == NULL)
    {
        printk("%s: Errore malloc() nell'allocazione del nuovo elemento da inserire nella hash table.", MOD_NAME);
        return 1;
    }

    /* Inizializzo il nuovo elemento */
    new_item->block_index = index;

    new_item->pos = pos;

    new_item->msg = (char *)kmalloc(strlen(data_block->msg) + 1, GFP_KERNEL);

    if(new_item->msg == NULL)
    {
        printk("%s: Errore malloc() nell'allocazione della memoria per il messaggio dell'elemento da inserire nella hash table.", MOD_NAME);
        return 1;
    }

    printk("%s: Stringa da copiare per il blocco con indice %lld - %s.\n", MOD_NAME, index, data_block->msg);

    strncpy(new_item->msg, data_block->msg, strlen(data_block->msg) + 1);

    printk("Lunghezza della stringa copiata %ld - Dimensione del buffer allocato %ld.\n", strlen(data_block->msg) + 1, strlen(new_item->msg) + 1);

    /* Inserimento in testa */
    if(ht_entry->head_list == NULL)
    {
        /* La lista è vuota */
        ht_entry->head_list = new_item;
        ht_entry->head_list->hash_table_next = NULL;
    }
    else
    {
        old_head = ht_entry->head_list;
        ht_entry->head_list = new_item;
        new_item->hash_table_next = old_head;
    }

    printk("%s: Inserimento blocco %lld nella entry #%d completato con successo.\n", MOD_NAME, index, num_entry_ht);

    /* Inserimento del blocco nella lista ordinata */
    insert_sorted_list(new_item);

    asm volatile("mfence");

    return 0;   
    
}



/*
 * Computa il numero di righe della tabella hash
 * applicando la formula. Si cerca di avere un numero
 * logaritmico di elementi per ogni lista della
 * hast_table_valid.
 */
int compute_num_rows(uint64_t num_data_block)
{
    int x;
    int list_len;
    
    if(num_data_block == 1)
    {
        return 1;      /* Non devo applicare la formula e ho solamente una lista */
    }
 
    list_len = ilog2(num_data_block) + 1;    /* Computo il numero di elementi massimo che posso avere in una lista */

    if((num_data_block % list_len) == 0)
    {
        x = num_data_block / list_len;
    }
    else
    {
        x = (num_data_block / list_len) + 1;
    }

    printk("%s: La lunghezza massima di una entry della tabella hash è pari a %d.\n", MOD_NAME, list_len);
    printk("%s: Il numero di entry nella tabella hash è pari a %d.\n", MOD_NAME, x);
    
    return x;
}



int init_bitmask(void)
{
    struct buffer_head *bh;
    struct soafs_sb_info *sbi;
    uint64_t num_block_state;
    uint64_t index;
    uint64_t roll_index;
    uint64_t * block_state;
    int sub_index;

    /* Recupero le informazioni specifiche */
    sbi = (struct soafs_sb_info *)sb_global->s_fs_info;

    num_block_state = sbi->num_block_state;

    printk("%s: Inizio inizializzazione bitmask...\nNumero entry pari a %lld\n", MOD_NAME, num_block_state);

    bitmask = (uint64_t **)kzalloc(num_block_state * sizeof(uint64_t *), GFP_KERNEL);

    if(bitmask == NULL)
    {
        printk("%s: Errore durante l'allocazione della bitmask.", MOD_NAME);
        return 1;
    }

    for(index=0;index<num_block_state;index++)
    {

        /* [SB][Inode][SB1]...[SBn][DB]...[DB] */
        bh = sb_bread(sb_global, index + 2);

        if(bh == NULL)
        {
            printk("%s: Errore nella lettura del blocco di stato con indice %lld.\n", MOD_NAME, index);
            for(roll_index=0; roll_index<index; roll_index++)
            {
                kfree(bitmask[roll_index]);    
            }
            printk("%s: deallocazioni eseguite con successo.\n", MOD_NAME);
	        return 1;
        }

        bitmask[index] = (uint64_t *)kzalloc(sizeof(uint64_t) * 512, GFP_KERNEL);

        if(bitmask[index] == NULL)
        {
            printk("%s: Errore nella lettura del blocco di stato con indice %lld.\n", MOD_NAME, index);
            for(roll_index=0; roll_index<index; roll_index++)
            {
                kfree(bitmask[roll_index]);    
            }
            printk("%s: deallocazioni eseguite con successo.\n", MOD_NAME);
	        return 1;
        }

        block_state = (uint64_t *)bh->b_data;

        for(sub_index=0;sub_index<512;sub_index++)
        {
            bitmask[index][sub_index]= block_state[sub_index]; 
        }

        brelse(bh);

        printk("%s: Inizializzazione blocco bitmask #%lld completata con successo.\n", MOD_NAME, index);
    }

    printk("%s: Inizializzazione bitmask completata con successo.\n", MOD_NAME);

    return 0;
    
}



void check_consistenza(void)
{
    struct block_free *bf;
    int bitmask_entry;
    int array_entry;
    int offset;

    bf = head_free_block_list;

    while(bf!=NULL)
    {
        printk("%s: valore indice del blocco inserito nella lista dei blocchi liberi è pari a %lld\n", MOD_NAME, bf->block_index);
    
        // Determino l'array di uint64_t */
        bitmask_entry = bf->block_index / (SOAFS_BLOCK_SIZE << 3);

        // Determino la entry dell'array */
        array_entry = (bf->block_index  % (SOAFS_BLOCK_SIZE << 3)) / sizeof(uint64_t);

        offset = bf->block_index % sizeof(uint64_t);

        if(bitmask[bitmask_entry][array_entry] & (1 << offset))
        {
            printk("%s: Errore inconsistenza per l'indice %lld.\n", MOD_NAME, bf->block_index);
        }
        
        bf = bf->next;
    }
}



/*
 * Inserisce un nuovo elemento all'interno della lista free_block_list.
 * L'inserimento viene fatto in testa poiché non mi importa mantenere
 * alcun ordine tra i blocchi liberi.
 */
int insert_free_list(uint64_t index)
{
    struct block_free *new_item;
    struct block_free *old_head;

    new_item = (struct block_free *)kmalloc(sizeof(struct block_free), GFP_KERNEL);

    if(new_item==NULL)
    {
        printk("%s: Errore malloc() sorted_list.", MOD_NAME);
        return 1;
    }

    new_item->block_index = index;

    if(head_free_block_list == NULL)
    {
        head_free_block_list = new_item;
        new_item -> next = NULL;
    }
    else
    {
        old_head = head_free_block_list;
        head_free_block_list = new_item;
        new_item -> next = old_head;
    }

    printk("%s: Inserito il blocco %lld nella lista dei blocchi liberi.\n", MOD_NAME, index);

    asm volatile("mfence");

    return 0;
}


/*
 * index_free: array con indici dei blocchi liberi
 * actual_size: dimensione effettiva dell'array relativa al FS che ho montato
 */
int init_free_block_list(uint64_t *index_free, uint64_t actual_size)
{
    uint64_t index;
    uint64_t roll_index;
    int ret;
    struct block_free *roll_bf;

    if(SIZE_INIT < actual_size)
    {
        printk("%s: Errore nella dimensione dell'array.\nACTUAL_SIZE = %lld\tSIZE_INIT = %d\n", MOD_NAME, actual_size, SIZE_INIT);
        return 1;
    }

    for(index=0; index<actual_size;index++)
    {

        ret = insert_free_list(index_free[index]);
        if(ret)
        {
            printk("%s: Errore kzalloc() free_list indice %lld.\n", MOD_NAME, index);
            for(roll_index=0; roll_index<index;roll_index++)
            {
                roll_bf = head_free_block_list->next;
                kfree(head_free_block_list);
                head_free_block_list = roll_bf;
            }
            return 1;
        }        
    }

    num_block_free_used = actual_size;

    pos = index_free[actual_size - 1] + 1;

    printk("%s: Il numero di blocchi liberi utilizzati è pari a %lld.\n", MOD_NAME, num_block_free_used);

    printk("%s: Il valore di pos è pari a %lld.\n", MOD_NAME, pos);

    check_consistenza();

    return 0;
}



int init_data_structure_core(uint64_t num_data_block, uint64_t *index_free, uint64_t actual_size)
{
    int x;                              // Numero di entry della tabella hash
    int ret;
    uint64_t index;
    size_t size_ht;
    struct soafs_sb_info *sbi;

    if(sb_global == NULL)
    {
        printk("%s: Il contenuto del superblocco non è valido. Impossibile inizializzare le strutture dati core.\n", MOD_NAME);
        return 1;
    }

    if(num_data_block <= 0)
    {
        printk("%s: Il numero di blocchi del device non è valido. Impossibile inizializzare le strutture dati core.\n", MOD_NAME);
        return 1;
    }

    x = compute_num_rows(num_data_block);

    size_ht = x * sizeof(struct ht_valid_entry);

    hash_table_valid = (struct ht_valid_entry *)kmalloc(size_ht, GFP_KERNEL);

    if(hash_table_valid == NULL)
    {
        printk("%s: Errore esecuzione kmalloc() nell'allocazione della memoria per la tabella hash.\n", MOD_NAME);
        return 1;
    } 

    /* Inizialmente le liste della tabella hash sono vuote. */
    for(index=0;index<x;index++)
    {
        (&hash_table_valid[index])->head_list = NULL;
    }

    /* Inizialmente la lista ordinata è vuota */
    head_sorted_list = NULL;
    tail_sorted_list = NULL;

    /* Inizializzo la bitmask */
    ret = init_bitmask();

    if(ret)
    {
        printk("%s: Errore inizializzazione bitmask, non è possibile completare l'inizializzazione core.\n", MOD_NAME);
        kfree(hash_table_valid);
        return 1;
    }

    sbi = (struct soafs_sb_info *)sb_global->s_fs_info;

    /* Inizializzo la free_block_list */
    if(sbi->num_block_free > 0)
    {
        ret = init_free_block_list(index_free, actual_size);
        if(ret)
        {
            printk("%s: Errore inizializzazione free_block_list, non è possibile completare l'inizializzazione core.\n", MOD_NAME);
            kfree(hash_table_valid);
            return 1;
        }
    }else
    {
        head_free_block_list = NULL;
    }

    debugging_init(x);

    return 0;
}
