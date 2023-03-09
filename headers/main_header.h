//main_header.h

#ifndef MAIN_HEADER_H
#define MAIN_HEADER_H

#include "./file_system.h"

/* Nome del modulo */
#define MOD_NAME "SOA-22-23"
/* Descrizione del modulo */
#define DESCRIPTION "Progetto Sistemi Operativi Avanzati AA 2022-2023"
/* Autore del modulo */
#define AUTHOR "Luca Capotombolo <capoluca99@gmail.com>"
/* Licenza software */
#define LICENSE "GPL"
/* Numero di entry della System Call table da recuperare */
#define HACKED_ENTRIES (int)(sizeof(new_sys_call_array)/sizeof(unsigned long))
/* AUDIT */
#define AUDIT if(1)

#endif
