/*
 * shared.h - Definiciones compartidas entre coordinador y generador
 * 
 * Sistema Generador de Datos Concurrente con Memoria Compartida
 * Ejercicio 1 - Sistemas Operativos
 */

#ifndef SHARED_H
#define SHARED_H

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <sys/ipc.h>
#include <errno.h>
#include <signal.h>

/* Constantes del sistema */
#define MAX_DATA_SIZE 256
#define ID_BLOCK_SIZE 10      /* Bloque de IDs que asigna el coordinador */
#define SHM_KEY 0x1234        /* Clave para memoria compartida */
#define SEM_KEY 0x5678        /* Clave para semáforos */
#define CSV_FILENAME "datos_generados.csv"

/* Estados de comunicación */
#define REQUEST_IDS 1         /* Generador solicita IDs */
#define SEND_RECORD 2         /* Generador envía registro */
#define NO_MORE_IDS 3         /* Coordinador indica fin de IDs */
#define SHUTDOWN 4            /* Señal de cierre */

/* Estructura para solicitud de IDs */
typedef struct {
    int action;               /* Tipo de acción (REQUEST_IDS, SEND_RECORD, etc.) */
    pid_t process_id;         /* ID del proceso generador */
    int start_id;            /* ID inicial del bloque asignado */
    int end_id;              /* ID final del bloque asignado */
} id_request_t;

/* Estructura de un registro de datos */
typedef struct {
    int action;               /* SEND_RECORD */
    int id;                  /* ID único del registro */
    pid_t process_id;        /* ID del proceso que generó el registro */
    time_t timestamp;        /* Timestamp de generación */
    char random_data[MAX_DATA_SIZE]; /* Dato aleatorio */
} record_t;

/* Estructura de la memoria compartida */
typedef struct {
    int status;              /* Estado actual de la memoria compartida */
    int next_id_to_write;    /* Próximo ID que debe escribirse (para orden correlativo) */
    union {
        id_request_t request; /* Para solicitudes de ID */
        record_t record;      /* Para envío de registros */
    } data;
} shared_memory_t;

/* Índices de semáforos */
enum {
    SEM_MUTEX = 0,           /* Exclusión mutua para acceso a memoria compartida */
    SEM_COORDINATOR,         /* Señal para el coordinador */
    SEM_GENERATOR,           /* Señal para generadores */
    SEM_COUNT                /* Total de semáforos */
};

/* Declaraciones de funciones compartidas */
void cleanup_resources(int shm_id, int sem_id);
void signal_handler(int sig);
int create_semaphore(key_t key, int num_sems);
int get_semaphore(key_t key);
void sem_wait(int sem_id, int sem_num);
void sem_post(int sem_id, int sem_num);
void generate_random_data(char *buffer, size_t size);

/* Variables globales para cleanup */
extern int g_shm_id;
extern int g_sem_id;

#endif /* SHARED_H */