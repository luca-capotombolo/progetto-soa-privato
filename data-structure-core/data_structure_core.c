#include <linux/buffer_head.h>  /* sb_bread()-brelse() */
#include <linux/string.h>       /* strncpy() */
#include <linux/slab.h>         /* kmalloc() */
#include <linux/wait.h>         /* wait_event_interruptible() - wake_up_interruptible() */

#include "../headers/main_header.h"



struct block *head_sorted_list = NULL;                          /* Puntatore alla testa della lista contenente i blocchi nell'ordine di consegna. */
struct block *tail_sorted_list = NULL;                          /* Puntatore alla coda della lista contenente i blocchi nell'ordine di consegna.  */
struct block_free *head_free_block_list = NULL;                 /* Puntatore alla testa della lista contenente i blocchi liberi. */
struct ht_valid_entry *hash_table_valid = NULL;                 /* Hash table */
uint64_t num_block_free_used = 0;
uint64_t pos = 0;
uint64_t**bitmask = NULL;
int x = 0;
uint64_t empty_actual_size = 0;

/*
 * Il primo bit identificato tramite la maschera di bit
 * MASK_INVALIDATE rappresenta l'esistenza di un thread
 * che sta invalidando un blocco. I restanti bit sono il
 * numero di thread impegnati nell'operazione di insert
 * di un nuovo blocco.
 * Questa variabile consente di sincronizzare le operazioni
 * di inserimento e l'operazione di invalidazione. Gli unici
 * scenari consentiti sono i seguenti:
 * 1. Non ci sono nè inserimenti né invalidazioni.
 * 2. Ho un numero arbitrario di inserimenti e nessuna
 *    invalidazione.
 * 3. Ho un'unica invalidazione e nessun inserimento.
 */
uint64_t sync_var = 0;
static DEFINE_MUTEX(inval_insert_mutex);
static DECLARE_WAIT_QUEUE_HEAD(the_queue);



void scan_free_list(void) //
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



void scan_sorted_list(void) //
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



void scan_hash_table(void) //
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



void debugging_init(void) //
{
    /* scansione della lista contenente le informazioni dei blocchi liberi. */
    scan_free_list();

    /* scansione della lista contenente i blocchi ordinati per inserimento. */
    scan_sorted_list();

    /* scansione delle liste della hash table */
    scan_hash_table();
}



/*
 * Computa il numero di righe della tabella hash
 * applicando la formula. Si cerca di avere un numero
 * logaritmico di elementi per ogni lista della
 * hast_table_valid.
 */
void compute_num_rows(uint64_t num_data_block) //
{
    int list_len;
    
    if(num_data_block == 1)
    {
        x = 1;      /* Non devo applicare la formula e ho solamente una lista */
    }
    else
    {
 
        list_len = ilog2(num_data_block) + 1;    /* Computo il numero di elementi massimo che posso avere in una lista */

        if((num_data_block % list_len) == 0)
        {
            x = num_data_block / list_len;
        }
        else
        {
            x = (num_data_block / list_len) + 1;
        }
    }

    printk("%s: La lunghezza massima di una entry della tabella hash è pari a %d.\n", MOD_NAME, list_len);
    printk("%s: Il numero di entry nella tabella hash è pari a %d.\n", MOD_NAME, x);
}



int init_bitmask(void) //
{
    struct buffer_head *bh;
    struct soafs_sb_info *sbi;
    uint64_t num_block_state;
    uint64_t index;
    uint64_t roll_index;
    uint64_t * block_state;
    int sub_index;

    /* 
     * Recupero le informazioni specifiche 
     * necessarie per identificare dove nel
     * device sono presenti i blocchi di stato
     */

    sbi = (struct soafs_sb_info *)sb_global->s_fs_info;

    num_block_state = sbi->num_block_state;

    if(num_block_state <= 0)
    {
        printk("%s: Numero dei blocchi di stato non è valido.\n", MOD_NAME);
        return 1;
    }

    printk("%s: Inizio inizializzazione bitmask...\nNumero entry pari a %lld\n", MOD_NAME, num_block_state);

    bitmask = (uint64_t **)kzalloc(num_block_state * sizeof(uint64_t *), GFP_KERNEL);

    if(bitmask == NULL)
    {
        printk("%s: Errore durante l'allocazione della bitmask.", MOD_NAME);
        return 1;
    }


    /* [SB][Inode][SB1]...[SBn][DB]...[DB] */
    for(index=0;index<num_block_state;index++)
    {
        /* Recupero i bit di stato dal device */
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

        /* 512 * 8 = 4096 BYTE */
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

        printk("%s: Inizializzazione blocco bitmask #%lld è stata completata con successo.\n", MOD_NAME, index);
    }

    printk("%s: Inizializzazione bitmask completata con successo.\n", MOD_NAME);

    return 0;
    
}



void check_consistenza(void) //
{
    struct block_free *bf;
    int bitmask_entry;
    int array_entry;
    uint64_t offset;
    uint64_t base;

    base = 1;

    bf = head_free_block_list;

    while(bf!=NULL)
    {
        printk("%s: valore indice del blocco inserito nella lista dei blocchi liberi è pari a %lld\n", MOD_NAME, bf->block_index);
    
        // Determino l'array di uint64_t */
        bitmask_entry = bf->block_index / (SOAFS_BLOCK_SIZE << 3);

        // Determino la entry dell'array */
        array_entry = (bf->block_index  % (SOAFS_BLOCK_SIZE << 3)) / (sizeof(uint64_t) * 8);

        offset = bf->block_index % (sizeof(uint64_t) * 8);

        if(bitmask[bitmask_entry][array_entry] & (base << offset))
        {
            printk("%s: Errore inconsistenza per l'indice %lld.\n", MOD_NAME, bf->block_index);
        }
        
        bf = bf->next;
    }
}



/*
 * Restituisce l'indice del blocco libero in testa alla lista.
 * Questa funzione può essere eseguita in concorrenza.
 */
struct block_free * get_freelist_head(void) //
{
    int ret;
    int n;
    struct block_free *old_head;   

    n = 0;

retry:

    old_head = head_free_block_list;

    /* Gestione di molteplici scritture concorrenti */

    //if(head_free_block_list == NULL)
    if(old_head == NULL)
    {
        printk("%s: La lista risulta essere attualmente vuota al tentativo #%d\n", MOD_NAME, n);
        return NULL;
    }

    ret = __sync_bool_compare_and_swap(&head_free_block_list, old_head, head_free_block_list->next);

    if(!ret)
    {
        n++;

        if(n > 10)
        {
            return NULL;
        }
    
        goto retry;
    }

    return old_head;
}



int check_bit(uint64_t index) //
{
    uint64_t base;
    uint64_t offset;
    int bitmask_entry;
    int array_entry;
    int bits;

    bits = sizeof(uint64_t) * 8;
    base = 1;

    /* 
     * Determino il blocco di stato che contiene
     * l'informazione relativa al blocco che sto
     * richiedendo (i.e., l'array di uint64_t).
     */
    bitmask_entry = index / (SOAFS_BLOCK_SIZE << 3);

    /* Determino la entry dell'array */
    array_entry = (index  % (SOAFS_BLOCK_SIZE << 3)) / bits;

    /* Determino l'offset nella entry dell'array */
    offset = index % bits;

    if(bitmask[bitmask_entry][array_entry] & (base << offset))
    {
        printk("%s: Il blocco di dati ad offset %lld è valido.\n", MOD_NAME, index);
        return 1;
    }

    printk("%s: Il blocco di dati ad offset %lld non è valido.\n", MOD_NAME, index);
    
    return 0;
}



int get_bitmask_block(void) //
{
    int ret;
    uint64_t index;
    uint64_t count;
    uint64_t num_block_data;
    struct soafs_sb_info *sbi;
    struct block_free *old_head;
    struct block_free *bf;

    sbi = (struct soafs_sb_info *)sb_global->s_fs_info;

    /*
     * Devo gestire la concorrenza tra i vari scrittori. Può
     * accadere che più thread nello stesso momento
     * devono invocare questa funzione. Solamente un
     * thread alla volta può eseguirla in modo da
     * caricare nella lista le informazioni solamente
     * una volta. Dopo che il primo thread ha eseguito
     * questa funzione, i threads successivi possono
     * trovarsi la lista non vuota, poiché precedentemente
     * popolata, oppure la lista nuovamente vuota ma
     * con tutti i blocchi liberi già utilizzati.
     * In entrambi i casi, la funzione non deve essere
     * eseguita poiché o i blocchi sono stati già
     * recuperati oppure non esiste alcun blocco libero
     * da utilizzare per inserire il messaggio.
     */

    if( (head_free_block_list != NULL) || (num_block_free_used == sbi->num_block_free))
    {
        printk("%s: I blocchi sono stati già determinati\n", MOD_NAME);
        return 0;
    }

    /* Recupero il numero totale dei blocchi di dati */
    num_block_data = sbi->num_block - 2 - sbi->num_block_state;

    /* 
     * Questa variabile rappresenta il numero massimo di
     * blocchi che è possibile inserire all'interno della
     * lista. In questo modo, riesco a gestire meglio la
     * quantità di memoria utilizzata. 
     */
    count = empty_actual_size;


    /*
     * Itero sui bit di stato alla ricerca dei
     * blocchi liberi da inserire nella lista.
     */
    for(index = pos; index<num_block_data; index++)
    {

        printk("%s: Verifica della validità del blocco con indice %lld\n", MOD_NAME, index);

        ret = check_bit(index);

        if(!ret)
        {

            /* Ho trovato un nuovo blocco libero da inserire nella free list */

            bf = (struct block_free *)kmalloc(sizeof(struct block_free), GFP_KERNEL);

            if(bf == NULL)
            {
                    printk("%s: Errore kmalloc() nuovo blocco libero con indice %lld\n", MOD_NAME, index);
                    return 1;
            }

            bf -> block_index = index;
retry:

            old_head = head_free_block_list;
        
            /* Implemento un inserimento in testa alla lista */
            bf->next = head_free_block_list;

            ret = __sync_bool_compare_and_swap(&head_free_block_list, old_head, bf);

            if(!ret)
            {
                    goto retry;
            }

            pos = index + 1;

            printk("%s: Il nuovo valore di pos è pari a %lld\n", MOD_NAME, pos);

            num_block_free_used++;

            count --;

            /*
             * Verifico se ho esaurito il numero di blocchi liberi oppure
             * se ho già caricato il numero massimo di blocchi all'interno
             * della lista. In entrambi i casi, interrompo la ricerca
             * in modo da risparmiare le risorse.
             */
            if( (num_block_free_used == sbi->num_block_free)  || (count == 0) )
            {
                if(num_block_free_used == sbi->num_block_free)
                {
                    printk("%s: Ho esaurito il numero di blocchi liberi totali\n", MOD_NAME);
                }
                else
                {
                    printk("%s: Ho inserito il numero massimo di elementi all'interno della lista\n", MOD_NAME);
                }

                break;  
            }                       
        }
    
    }

    return 0;
    
}



/*
 * Inserisce un nuovo elemento all'interno della lista free_block_list.
 * L'inserimento viene fatto in testa poiché non mi importa mantenere
 * alcun ordine tra i blocchi liberi.
 */
int insert_free_list(uint64_t index) //
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
int init_free_block_list(uint64_t *index_free, uint64_t actual_size) //
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

    empty_actual_size = actual_size;

    printk("%s: Il numero di blocchi liberi utilizzati è pari a %lld.\n", MOD_NAME, num_block_free_used);

    printk("%s: Il valore di pos è pari a %lld.\n", MOD_NAME, pos);

    check_consistenza();

    return 0;
}



void set_bitmask(uint64_t index, int mode) //
{
    uint64_t base;
    uint64_t offset;
    int bitmask_entry;
    int array_entry;
    int bits;

    bits = sizeof(uint64_t) * 8;
    

    /* 
     * Determino il blocco di stato che contiene
     * l'informazione relativa al blocco che sto
     * richiedendo (i.e., l'array di uint64_t).
     */
    bitmask_entry = index / (SOAFS_BLOCK_SIZE << 3);

    /* Determino la entry dell'array */
    array_entry = (index  % (SOAFS_BLOCK_SIZE << 3)) / bits;

    /* Determino l'offset nella entry dell'array */
    offset = index % bits;

    if(mode)
    {
        base = 1;
        bitmask[bitmask_entry][array_entry] |= (base << offset);
    }
    else
    {
        base = 0;
        bitmask[bitmask_entry][array_entry] &= (base << offset);
    }
    

}


/*
 * Questa funzione è simile alla funzione 'insert_sorted_list'
 * che si trova successivamente. La differenza con l'altra funzione
 * sta nel fatto che questa gestisce la concorrenza.
 * In questo scenario, l'inserimento dei blocchi all'interno
 * della lista viene fatto in coda.
 */
int insert_sorted_list_conc(struct block *block)
{
    struct block *next;
    struct block *curr;
    int ret;
    int n;

    n = 0;

    block->sorted_list_next = NULL;

    next = NULL;

    if(head_sorted_list == NULL)
    {
        ret = __sync_bool_compare_and_swap(&head_sorted_list, next, block);

        if(!ret)
        {
            printk("%s: L'inserimento in coda non è stato eseguito poiché la lista non è più vuota\n", MOD_NAME);
            n++;
            goto no_empty;        
        }

        printk("%s: Inserimento in coda nella lista ordinata effettuato con successo per il blocco %lld\n", MOD_NAME, block->block_index);
    
        return 0;
    }

no_empty:

    /*
     * Poiché le invalidazioni dei blocchi non possono essere eseguite
     * in concorrenza con gli inserimenti, da questo momento la lista
     * non potrà essere vuota. In questo modo, posso gestire correttamente
     * il puntatore 'next'.
     */

    if(n > 10)
    {
        printk("%s: Il numero di tentativi massimo consentito %d è stato raggiunto per l'inserimento nella sorted list del blocco %lld\n", MOD_NAME, n, block->block_index);
        return 1;
    }

    curr = head_sorted_list;

    while( curr->sorted_list_next != NULL )
    {
        curr = curr->sorted_list_next;
    }

    ret = __sync_bool_compare_and_swap(&(curr->sorted_list_next), next, block);

    if(!ret)
    {
        printk("%s: Tentativo di inserimento #%d del blocco %lld terminato senza successo\n", MOD_NAME, n, block->block_index);
        n++;
        goto no_empty;
    }

    printk("%s: Inserimento in coda nella lista ordinata effettuato con successo per il blocco %lld\n", MOD_NAME, block->block_index);

    return 0;    
}


/*
 * Inserisce un nuovo elemento all'interno della
 * lista contenente i blocchi ordinati secondo
 * l'ordine di consegna. L'elemento non deve essere
 * nuovamente allocato ma si collega l'elemento che 
 * è stato precedentemente allocato tramite puntatori.
 * Il campo 'pos' rappresenta la posizione del blocco
 * nella lista ordinata e viene sfruttato per determinare
 * la corretta posizione all'interno della lista.
 * Osservo che l'esecuzione di questa funzione avviene in
 * assenza di concorrenza.
 */
void insert_sorted_list(struct block *block) //
{
    struct block *prev;
    struct block *curr;

    if(head_sorted_list == NULL)
    {
        /* La lista è vuota ed eseguo un inserimento in testa */
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

            /* L'elemento che dovrà precedere il nuovo item */
            prev = head_sorted_list;

            /* L'elemento che dovrà seguire il nuovo item */
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



void check_rollback(uint64_t index, struct ht_valid_entry * ht_entry)
{
    struct block *curr;

    curr = ht_entry->head_list;

    while(curr != NULL)
    {
        if(curr->block_index == index)
        {
            printk("%s: [ERRORE] La procedura di rollback non è stata eseguita con successo per il blocco con indice %lld\n", MOD_NAME, index);
        }
        curr = curr->hash_table_next;
    }

    printk("%s: [CHECK] La procedura di rollback è stata eseguita con successo per il blocco con indice %lld\n", MOD_NAME, index);
}



static void rollback(uint64_t index, struct ht_valid_entry * ht_entry)
{

    int ret;
    int n;
    struct block *curr;
    struct block *prev;
    

    curr = ht_entry->head_list;

    /*
     * Poiché durante la fase di rollback non è possibile
     * avere invalidazioni e gli inserimenti nelle
     * liste della HT avvengono in testa, allora non dovrebbe
     * esserci alcun nuovo blocco tra quello che si deve
     * eliminare e il successivo (se esiste).
     */



    /*
     * Verifico se il blocco corrisponde alla testa della lista.
     * In questo caso, nessun altro blocco è stato successivamente
     * inserito.
     */

    if(curr->block_index == index)
    {
        ret = __sync_bool_compare_and_swap(&(ht_entry->head_list), curr, curr->hash_table_next);

        if(!ret)
        {
            /* Sono stati inseriti nuovi blocchi in testa alla lista */
            goto no_head;
        }

        printk("%s: Rollback completato con successo: blocco %lld eliminato dalla HT\n", MOD_NAME, index);

        kfree(curr);

        check_rollback(index, ht_entry);

        return ;
    }

    n = 0;

no_head:

    prev = ht_entry->head_list;

    curr = prev->hash_table_next;

    while(curr != NULL)
    {
        if( curr->block_index == index )
        {
            break;
        }

        prev = curr;
        curr = curr->hash_table_next;
    }

    if(curr == NULL)
    {
        printk("%s: [ERRORE] Il blocco %lld da rimuovere non è presente nella lista\n", MOD_NAME, index);
        return ;
    }

    ret = __sync_bool_compare_and_swap(&(prev->hash_table_next), curr, curr->hash_table_next);

    if(!ret)
    {
        n++;

        printk("%s: Tentativo #%d fallito nell'esecuzione della procedura di rollback\n", MOD_NAME, n);

        goto no_head;
    }

    printk("%s: Rollback completato con successo: blocco %lld eliminato dalla HT\n", MOD_NAME, index);

    kfree(curr);
}



/*
 * Inserisce un nuovo elemento all'interno della lista
 * nella hash table e all'interno della lista ordinata
 * dei blocchi. Questa funzione viene eseguita in concorrenza.
 */
int insert_hash_table_valid_and_sorted_list_conc(char *data_block_msg, uint64_t pos, uint64_t index)
{
    int num_entry_ht;
    int ret;
    int n;                                      /* Contatore del numero di tentativi effettuati */
    struct block *new_item;
    struct block *old_head;
    struct ht_valid_entry *ht_entry; 

    /* Identifico la lista corretta nella hash table per effettuare l'inserimento */
    num_entry_ht = index % x;

    ht_entry = &(hash_table_valid[num_entry_ht]);

    /* Alloco il nuovo elemento da inserire nelle liste */
    new_item = (struct block *)kmalloc(sizeof(struct block), GFP_KERNEL);
    
    if(new_item == NULL)
    {
        printk("%s: Errore kmalloc() nell'allocazione del nuovo elemento da inserire nella hash table.", MOD_NAME);
        return 1;
    }

    /* Inizializzo il nuovo elemento */
    new_item->block_index = index;

    new_item->pos = pos;

    new_item->msg = data_block_msg;

    /* Gestisco la concorrenza con le invalidazioni */

    n = 0;

retry_mutex_inval_insert:

    if(n > 10)
    {
        printk("%s: [CONFLITTO] Il numero massimo di tentativi %d per l'inserimento del blocco con indice %lld è stato raggiunto\n", MOD_NAME, n, index);
        return 1;
    }

    mutex_lock(&inval_insert_mutex);    

    if( sync_var & MASK_INVALIDATE )
    {
        printk("%s: E' in corso un'invalidazione, è necessario attendere che l'invalidazione termini\n", MOD_NAME);

        mutex_unlock(&inval_insert_mutex);

        wait_event_interruptible(the_queue, (sync_var & MASK_INVALIDATE) == 0 );

        n++;

        goto retry_mutex_inval_insert;
    }

    /* 
     * Comunico la presenza del thread che effettuerà
     * l'inserimento di un nuovo blocco.
     */
    __sync_fetch_and_add(&sync_var,1);

    //asm(memory)

    printk("%s: Comunicazione per l'inserimento del blocco %lld avvenuta con successo\n", MOD_NAME, index);

    mutex_unlock(&inval_insert_mutex);

    /* Inserimento in testa nella lista della HT */

    n = 0;

retry_insert_ht:

    if(n > 10)
    {
        printk("%s: Il numero di tentativi massimo consentito %d è stato raggiunto per l'inserimento nella HT\n", MOD_NAME, n);

        __sync_fetch_and_sub(&sync_var,1);              /* Segnalo che l'operazione di inserimento si è conclusa con successo. */

        wake_up_interruptible(&the_queue);

        return 1;
    }

    old_head = ht_entry->head_list;

    new_item->hash_table_next = old_head;

    ret = __sync_bool_compare_and_swap(&(ht_entry->head_list), old_head, new_item);

    if(!ret)
    {
        printk("%s: Conflitto nell'inserimento in testa nella lista #%d della HT tentativo #%d\n", MOD_NAME, num_entry_ht, n);
        n++;
        goto retry_insert_ht;
    }

    printk("%s: Inserimento blocco %lld nella entry #%d della HT completato con successo.\n", MOD_NAME, index, num_entry_ht);

    ret = insert_sorted_list_conc(new_item);            /* Inserimento del blocco nella lista ordinata */

    if(ret)
    {
        printk("%s: Il numero di tentativi massimo consentito %d è stato raggiunto per l'inserimento nella sorted list\n", MOD_NAME, 10);

        rollback(new_item->block_index, ht_entry);

        __sync_fetch_and_sub(&sync_var,1);              /* Segnalo che l'operazione di inserimento si è conclusa con successo. */

        wake_up_interruptible(&the_queue);

        return 1;        
    }

    __sync_fetch_and_sub(&sync_var,1);                  /* Segnalo che l'operazione di inserimento si è conclusa con successo. */

    wake_up_interruptible(&the_queue);

    printk("%s: Inserimento blocco %lld nella sorted list avvenuto con successo\n", MOD_NAME, index);

    asm volatile("mfence");

    return 0;   
    
}




/*
 * Inserisce un nuovo elemento all'interno della lista
 * identificata dal parametro 'x'.
 */
static int insert_hash_table_valid_and_sorted_list(char *data_block_msg, uint64_t pos, uint64_t index) //
{
    int num_entry_ht;
    size_t len;
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

    len = strlen(data_block_msg) + 1;

    new_item->msg = (char *)kmalloc(len, GFP_KERNEL);

    if(new_item->msg == NULL)
    {
        printk("%s: Errore malloc() nell'allocazione della memoria per il messaggio dell'elemento da inserire nella hash table.", MOD_NAME);
        return 1;
    }

    printk("%s: Stringa da copiare per il blocco con indice %lld - %s.\n", MOD_NAME, index, data_block_msg);

    strncpy(new_item->msg, data_block_msg, len);

    printk("Lunghezza della stringa copiata %ld - Dimensione del buffer allocato %ld.\n", strlen(data_block_msg) + 1, strlen(new_item->msg) + 1);

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




int init_ht_valid_and_sorted_list(uint64_t num_data_block) //
{
    uint64_t index;
    int isValid;
    int ret;
    struct buffer_head *bh;
    struct soafs_block *data_block;
    struct soafs_sb_info *sbi;

    sbi = (struct soafs_sb_info *)sb_global->s_fs_info;

    if(sbi->num_block_free > sbi->num_block)
    {
        printk("%s: Numero di blocchi liberi maggiore del numero dei blocchi totale\n", MOD_NAME);
        return 1;
    }
    
    if(sbi->num_block_free == sbi->num_block)
    {
        printk("%s: Non ci sono blocchi validi\n", MOD_NAME);
        tail_sorted_list = NULL;
        head_sorted_list = NULL;
        return 0;
    }

    bh = NULL;
    data_block = NULL;

    for(index=0; index<num_data_block; index++)
    {
        /* Verifico se il blocco è valido */
        isValid = check_bit(index);

        if(!isValid)
        {
            continue;
        }

        bh = sb_bread(sb_global, 2 + sbi->num_block_state + index);                   

        if(bh == NULL)
        {
            printk("%s: Errore esecuzione della sb_bread() per la lettura del blocco di dati con indice %lld...\n", MOD_NAME, index);
            return 1;
        }

        data_block = (struct soafs_block *)bh->b_data;

        ret = insert_hash_table_valid_and_sorted_list(data_block->msg, data_block->pos, index);

        if(ret)
        {
            printk("%s: Errore inserimento nella HT del blocco con indice %lld.\n", MOD_NAME, index);
            return 1;
        }

        printk("%s: Il blocco di dati con indice %lld è valido e nella lista ordinata si trova in posizione %lld.\n", MOD_NAME, index, data_block->pos);

        brelse(bh);        
        
    }

    return 0;
}



int init_data_structure_core(uint64_t num_data_block, uint64_t *index_free, uint64_t actual_size) //
{
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

    /* Inizializzo la bitmask */
    ret = init_bitmask();

    if(ret)
    {
        printk("%s: Errore inizializzazione bitmask, non è possibile completare l'inizializzazione core.\n", MOD_NAME);
        // kfree(hash_table_valid);
        return 1;
    }

    sbi = (struct soafs_sb_info *)sb_global->s_fs_info;

    if( (actual_size < 0) || (sbi->num_block_free < 0) )
    {
        printk("%s: Le informazioni sui blocchi liberi non sono valide.\n", MOD_NAME);
        return 1;
    }

    if( (actual_size > 0) && (sbi->num_block_free == 0) )
    {
        printk("%s: Problema di inconsistenza tra il numero dei blocchi liberi e la dimensione dell'array.\n", MOD_NAME);
        return 1;
    }


    /* Inizializzo la free_block_list */

    if(actual_size > 0)
    {
        ret = init_free_block_list(index_free, actual_size);
        if(ret)
        {
            printk("%s: Errore inizializzazione free_block_list, non è possibile completare l'inizializzazione core.\n", MOD_NAME);
            // kfree(hash_table_valid);
            // kfree(bitmask);
            return 1;
        }

    }else
    {
        head_free_block_list = NULL;
    }


    /* Inizializzazione HT e sorted_list */

    compute_num_rows(num_data_block);

    size_ht = x * sizeof(struct ht_valid_entry);

    hash_table_valid = (struct ht_valid_entry *)kmalloc(size_ht, GFP_KERNEL);

    if(hash_table_valid == NULL)
    {
        printk("%s: Errore esecuzione kmalloc() nell'allocazione della memoria per la tabella hash.\n", MOD_NAME);
        return 1;
    }

    printk("%s: Il numero di liste nella tabella hash è pari a %d\n", MOD_NAME, x);

    for(index=0;index<x;index++)
    {
        (&hash_table_valid[index])->head_list = NULL;
    }
    
    ret = init_ht_valid_and_sorted_list(num_data_block);

    if(ret)
    {
        printk("%s: Errore inizializzazione HT e sorted_list\n", MOD_NAME);
        // fai le kfree
        return 1;
    }

    debugging_init();

    return 0;
}
