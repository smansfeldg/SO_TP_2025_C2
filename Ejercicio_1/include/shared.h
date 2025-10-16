/*
 * shared.h - Definiciones compartidas entre coordinador y generadores
 *
 * Sistema Generador de Datos Concurrente con Memoria Compartida
 * Ejercicio 1 - Sistemas Operativos
 */

#ifndef SHARED_H
#define SHARED_H

#include <errno.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ipc.h>
#include <sys/sem.h>
#include <sys/shm.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

/* Constantes del sistema */
#define MAX_DATA_SIZE 256
#define ID_BLOCK_SIZE 10
#define RECORD_BUFFER_SIZE 32
#define SHM_KEY 0x1234
#define SEM_KEY 0x5678
#define CSV_FILENAME "datos_generados.csv"

/* Estados de comunicación */
#define REQUEST_IDS 1
#define SEND_RECORD 2
#define NO_MORE_IDS 3
#define SHUTDOWN 4
#define ASSIGN_IDS 5

/* Solicitud y asignación de IDs */
typedef struct {
    int action;
    pid_t process_id;
    int start_id;
    int end_id;
} id_request_t;

/* Registro generado por un proceso */
typedef struct {
    int action;
    int id;
    pid_t process_id;
    time_t timestamp;
    char random_data[MAX_DATA_SIZE];
} record_t;

typedef struct {
    id_request_t request;
} request_shared_t;

typedef struct {
    record_t records[RECORD_BUFFER_SIZE];
    int read_index;
    int write_index;
    int count;
} record_ring_t;

typedef struct {
    request_shared_t request_slot;
    record_ring_t record_ring;
} shared_memory_t;

/* Índices de semáforos */
enum {
    SEM_REQUEST_TURN = 0,
    SEM_REQUEST_MUTEX,
    SEM_REQUEST_AVAILABLE,
    SEM_REQUEST_DONE,
    SEM_RECORD_MUTEX,
    SEM_RECORD_AVAILABLE,
    SEM_RECORD_EMPTY,
    SEM_COUNT
};

/* Declaraciones de funciones compartidas */
void cleanup_resources(int shm_id, int sem_id);
void signal_handler(int sig);
int create_semaphore(key_t key, int num_sems);
int get_semaphore(key_t key);
void sem_wait(int sem_id, int sem_num);
void sem_post(int sem_id, int sem_num);
int sem_trywait(int sem_id, int sem_num);
int sem_wait_timed(int sem_id, int sem_num, const struct timespec *timeout);
void generate_random_data(char *buffer, size_t size);

extern int g_shm_id;
extern int g_sem_id;

#endif /* SHARED_H */