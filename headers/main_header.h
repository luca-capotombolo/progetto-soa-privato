//main_header.h

#ifndef MAIN_HEADER_H
#define MAIN_HEADER_H

#include "./file_system.h"
#include "./data_structure_core.h"

/* Nome del modulo */
#define MOD_NAME "SOA-22-23"

/* Descrizione del modulo */
#define DESCRIPTION "Progetto Sistemi Operativi Avanzati AA 2022-2023"

/* Autore del modulo */
#define AUTHOR "Luca Capotombolo <capoluca99@gmail.com>"

/* Licenza software */
#define LICENSE "GPL"

/* Periodo per il kernel thread */
#define PERIOD 10

/* Maschera per recuperare il valore dell'epoca */
#define MASK 0X8000000000000000

/* Numero di entry della System Call table da recuperare */
#define HACKED_ENTRIES (int)(sizeof(new_sys_call_array)/sizeof(unsigned long))

/* AUDIT */
#define AUDIT if(1)

/* Debugging per le system calls */
#define LOG_SYSTEM_CALL(system_call)                                                                            \
    printk("%s: è stata richiesta l'invocazione della system call %s.\n", MOD_NAME, system_call)

/* Errore parametri system call */
#define LOG_PARAM_ERR(system_call)                                                                              \
    printk("%s: la %s è stata invocata con parametri non validi.\n", MOD_NAME, system_call)

/* Errore DEVICE system call */
#define LOG_DEV_ERR(system_call)                                                                                \
    printk("%s: [%s] il device su cui operare non è stato montato.\n", MOD_NAME, system_call)

/* BUFFER CACHE */
#define LOG_BH(system_call, operazione, offset, esito)                                                            \
    printk("%s: [%s] %s nella %s del blocco con indice %d\n", MOD_NAME, system_call, esito, operazione, offset)

/* Errore KMALLOC */
#define LOG_KMALLOC_ERR(system_call)                                                            \
    printk("%s: [%s] si è verificato un errore nell'allocazione della memoria con la kmalloc()\n", MOD_NAME, system_call)
                                                

#endif
