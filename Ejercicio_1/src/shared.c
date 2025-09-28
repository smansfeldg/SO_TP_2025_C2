/*
 * shared.c - Implementación de funciones compartidas
 * 
 * Sistema Generador de Datos Concurrente con Memoria Compartida
 * Ejercicio 1 - Sistemas Operativos
 */

#include "../include/shared.h"

/* Variables globales para cleanup */
int g_shm_id = -1;
int g_sem_id = -1;

/*
 * cleanup_resources - Libera todos los recursos IPC
 * @shm_id: ID de memoria compartida
 * @sem_id: ID del conjunto de semáforos
 */
void cleanup_resources(int shm_id, int sem_id) {
    if (shm_id != -1) {
        if (shmctl(shm_id, IPC_RMID, NULL) == -1) {
            perror("Error liberando memoria compartida");
        } else {
            printf("Memoria compartida liberada exitosamente\n");
        }
    }
    
    if (sem_id != -1) {
        if (semctl(sem_id, 0, IPC_RMID) == -1) {
            perror("Error liberando semáforos");
        } else {
            printf("Semáforos liberados exitosamente\n");
        }
    }
}

/*
 * signal_handler - Manejador de señales para cleanup
 * @sig: Número de señal recibida
 */
void signal_handler(int sig) {
    printf("\nSeñal %d recibida. Limpiando recursos...\n", sig);
    cleanup_resources(g_shm_id, g_sem_id);
    exit(EXIT_SUCCESS);
}

/*
 * create_semaphore - Crea un conjunto de semáforos
 * @key: Clave IPC
 * @num_sems: Número de semáforos a crear
 * @return: ID del conjunto de semáforos, -1 en error
 */
int create_semaphore(key_t key, int num_sems) {
    int sem_id;
    
    /* Crear conjunto de semáforos */
    sem_id = semget(key, num_sems, IPC_CREAT | IPC_EXCL | 0666);
    if (sem_id == -1) {
        perror("Error creando semáforos");
        return -1;
    }
    
    /* Inicializar semáforos */
    if (semctl(sem_id, SEM_MUTEX, SETVAL, 1) == -1 ||
        semctl(sem_id, SEM_COORDINATOR, SETVAL, 0) == -1 ||
        semctl(sem_id, SEM_GENERATOR, SETVAL, 1) == -1) {
        perror("Error inicializando semáforos");
        semctl(sem_id, 0, IPC_RMID);
        return -1;
    }
    
    return sem_id;
}

/*
 * get_semaphore - Obtiene un conjunto de semáforos existente
 * @key: Clave IPC
 * @return: ID del conjunto de semáforos, -1 en error
 */
int get_semaphore(key_t key) {
    int sem_id = semget(key, 0, 0);
    if (sem_id == -1) {
        perror("Error obteniendo semáforos");
    }
    return sem_id;
}

/*
 * sem_wait - Operación wait (P) en semáforo
 * @sem_id: ID del conjunto de semáforos
 * @sem_num: Número del semáforo en el conjunto
 */
void sem_wait(int sem_id, int sem_num) {
    struct sembuf sb = {sem_num, -1, 0};
    while (semop(sem_id, &sb, 1) == -1) {
        if (errno == EINTR) {
            /* Interrupción por señal, reintentar */
            continue;
        }
        perror("Error en sem_wait");
        exit(EXIT_FAILURE);
    }
}

/*
 * sem_post - Operación signal (V) en semáforo
 * @sem_id: ID del conjunto de semáforos
 * @sem_num: Número del semáforo en el conjunto
 */
void sem_post(int sem_id, int sem_num) {
    struct sembuf sb = {sem_num, 1, 0};
    while (semop(sem_id, &sb, 1) == -1) {
        if (errno == EINTR) {
            /* Interrupción por señal, reintentar */
            continue;
        }
        perror("Error en sem_post");
        exit(EXIT_FAILURE);
    }
}

/*
 * generate_random_data - Genera datos aleatorios
 * @buffer: Buffer donde escribir los datos
 * @size: Tamaño máximo del buffer
 */
void generate_random_data(char *buffer, size_t size) {
    if (size <= 1) {
        buffer[0] = '\0';
        return;
    }
    
    const char charset[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789_-";
    size_t charset_len = sizeof(charset) - 1;
    
    /* Generar longitud apropiada (entre 8 y size-1 para dejar espacio) */
    int min_len = (size > 16) ? 8 : 1;
    int max_len = (int)(size - 1);
    int len = min_len + (rand() % (max_len - min_len + 1));
    
    for (int i = 0; i < len; i++) {
        buffer[i] = charset[rand() % charset_len];
    }
    buffer[len] = '\0';
}