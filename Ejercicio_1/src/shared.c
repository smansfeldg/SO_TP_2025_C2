/*
 * shared.c - Implementación de utilidades compartidas
 *
 * Sistema Generador de Datos Concurrente con Memoria Compartida
 * Ejercicio 1 - Sistemas Operativos
 */

#include "../include/shared.h"

/* Variables globales para cleanup */
int g_shm_id = -1;
int g_sem_id = -1;

union semun {
    int val;
    struct semid_ds *buf;
    unsigned short *array;
#if defined(__linux__)
    struct seminfo *__buf;
#endif
};

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

void signal_handler(int sig) {
    printf("\nSeñal %d recibida. Limpiando recursos...\n", sig);
    cleanup_resources(g_shm_id, g_sem_id);
    exit(EXIT_SUCCESS);
}

int create_semaphore(key_t key, int num_sems) {
    int sem_id = semget(key, num_sems, IPC_CREAT | IPC_EXCL | 0666);
    if (sem_id == -1) {
        perror("Error creando semáforos");
        return -1;
    }

    unsigned short init_values[SEM_COUNT] = {
        1,  /* SEM_REQUEST_TURN */
        1,  /* SEM_REQUEST_MUTEX */
        0,  /* SEM_REQUEST_AVAILABLE */
        0,  /* SEM_REQUEST_DONE */
        1,  /* SEM_RECORD_MUTEX */
        0,  /* SEM_RECORD_AVAILABLE */
        RECORD_BUFFER_SIZE /* SEM_RECORD_EMPTY */
    };

    union semun arg;
    arg.array = init_values;

    if (semctl(sem_id, 0, SETALL, arg) == -1) {
        perror("Error inicializando semáforos");
        semctl(sem_id, 0, IPC_RMID);
        return -1;
    }

    return sem_id;
}

int get_semaphore(key_t key) {
    int sem_id = semget(key, 0, 0);
    if (sem_id == -1) {
        perror("Error obteniendo semáforos");
    }
    return sem_id;
}

void sem_wait(int sem_id, int sem_num) {
    struct sembuf sb = {sem_num, -1, 0};
    while (semop(sem_id, &sb, 1) == -1) {
        if (errno == EINTR) {
            continue;
        }
        perror("Error en sem_wait");
        exit(EXIT_FAILURE);
    }
}

int sem_wait_timed(int sem_id, int sem_num, const struct timespec *timeout) {
    if (timeout == NULL) {
        sem_wait(sem_id, sem_num);
        return 0;
    }

    struct sembuf sb = {sem_num, -1, 0};
    struct timespec ts = *timeout;

    while (semtimedop(sem_id, &sb, 1, &ts) == -1) {
        if (errno == EINTR) {
            ts = *timeout;
            continue;
        }
        return -1;
    }

    return 0;
}

int sem_trywait(int sem_id, int sem_num) {
    struct sembuf sb = {sem_num, -1, IPC_NOWAIT};
    if (semop(sem_id, &sb, 1) == -1) {
        return -1;
    }
    return 0;
}

void sem_post(int sem_id, int sem_num) {
    struct sembuf sb = {sem_num, 1, 0};
    while (semop(sem_id, &sb, 1) == -1) {
        if (errno == EINTR) {
            continue;
        }
        perror("Error en sem_post");
        exit(EXIT_FAILURE);
    }
}

void generate_random_data(char *buffer, size_t size) {
    if (size <= 1) {
        buffer[0] = '\0';
        return;
    }

    const char charset[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789_-";
    size_t charset_len = sizeof(charset) - 1;

    int min_len = (size > 16) ? 8 : 1;
    int max_len = (int)(size - 1);
    int len = min_len + (rand() % (max_len - min_len + 1));

    for (int i = 0; i < len; i++) {
        buffer[i] = charset[rand() % charset_len];
    }
    buffer[len] = '\0';
}