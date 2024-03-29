//main_header.h

#ifndef MAIN_HEADER_H
#define MAIN_HEADER_H

#include "./file_system.h"
#include "./data_structure_core.h"

//#define NOT_CRITICAL_BUT_INVAL
//#define NOT_CRITICAL_INVAL
//#define NOT_CRITICAL_BUT_GET
//#define NOT_CRITICAL_GET
//#define NOT_CRITICAL_BUT_PUT
//#define NOT_CRITICAL_PUT

/* Nome del modulo */
#define MOD_NAME "SOA-22-23"

/* Descrizione del modulo */
#define DESCRIPTION "Progetto Sistemi Operativi Avanzati AA 2022-2023"

/* Autore del modulo */
#define AUTHOR "Luca Capotombolo <capoluca99@gmail.com>"

/* Licenza software */
#define LICENSE "GPL"

/* Periodo per il kernel thread */
#define PERIOD 30

/* Maschera per recuperare il valore dell'epoca */
#define MASK 0X8000000000000000

/* Numero di entry della System Call table da recuperare */
#define HACKED_ENTRIES (int)(sizeof(new_sys_call_array)/sizeof(unsigned long))

/* AUDIT */
#define AUDIT if(1)

/* Debugging per le system calls */
#define LOG_SYSTEM_CALL(system_call_m, system_call)                                                              \
    printk("%s: [%s] è stata richiesta l'invocazione della system call %s.\n", MOD_NAME, system_call_m, system_call)

/* Errore parametri system call */
#define LOG_PARAM_ERR(system_call_m, system_call)                                                                              \
    printk("%s: [%s] la %s è stata invocata con parametri non validi.\n", MOD_NAME, system_call_m, system_call)

/* Errore DEVICE system call */
#define LOG_DEV_ERR(system_call_m, system_call)                                                                                \
    printk("%s: [%s] il device su cui operare non è stato montato.\n", MOD_NAME, system_call_m)

/* BUFFER CACHE */
#define LOG_BH(system_call, operazione, offset, esito)                                                            \
    printk("%s: [%s] %s nella %s del blocco con indice %d\n", MOD_NAME, system_call, esito, operazione, offset)

/* Errore KMALLOC */
#define LOG_KMALLOC_ERR(system_call_m)                                                            \
    printk("%s: [%s] si è verificato un errore nell'allocazione della memoria con la kmalloc()\n", MOD_NAME, system_call_m)



/**
 * @code: Codice numerico per l'esecuzione dell'invalidazione
 * @bh: Puntatore al buffer head che dovrà essere modificato al termine dell'esecuzione dell'invalidazione
 *
 */
struct result_inval {
    int code;
    struct buffer_head *bh;
};




/*
 * Il primo bit, identificato tramite la maschera di bit MASK_INVALIDATE,
 * codifica l'esistenza di un thread che sta invalidando un blocco. I
 * restanti bit sono il numero di thread impegnati nell'operazione di
 * inserimento di un nuovo blocco.
 * Questa variabile consente di sincronizzare le operazioni di inserimento
 * e l'operazione di invalidazione. Gli unici scenari consentiti sono i
 * seguenti:
 * 1. Non ci sono nè inserimenti nè invalidazioni.
 * 2. Ho un numero arbitrario di inserimenti e nessuna
 *    invalidazione.
 * 3. Ho un'unica invalidazione e nessun inserimento.
 */
extern uint64_t sync_var;




/* 
 * Rappresenta il numero di threads che correntemente sono in esecuzione e
 * che potrebbero lavorare sul FS. E' necessario tenere conto di questi
 * thread in modo da eseguire correttamente lo smontaggio. Infatti, questa
 * variabile viene incrementata e decrementata all'interno delle varie
 * system call e all'interno del driver per poi essere utilizzata nella
 * funzione di smontaggio come check per poter iniziare lo smontaggio del FS.
 */
extern uint64_t num_threads_run;




/*
 * Permette di implementare una 'barriera' che blocca i threads in modo
 * da evitare che essi operino sul FS. Se il valore della variabile è
 * pari ad 1 allora il thread non è autorizzato ad accedere al device; 
 * altrimenti, è possibile usufruire dei servizi messi a disposizione. 
 */
extern int stop;





/* Mi consente di gestire la concorrenza tra un'invalidazione e gli inserimenti */
static DEFINE_MUTEX(inval_insert_mutex);




/* Mi consente di avere una sola invalidazione alla volta */
static DEFINE_MUTEX(invalidate_mutex);




static DECLARE_WAIT_QUEUE_HEAD(the_queue);




static DECLARE_WAIT_QUEUE_HEAD(umount_queue);
                                                

#endif
