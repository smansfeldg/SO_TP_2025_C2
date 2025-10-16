/*
 * generador.c - Proceso generador de datos
 *
 * Sistema Generador de Datos Concurrente con Memoria Compartida
 * Ejercicio 1 - Sistemas Operativos
 */

#include "../include/shared.h"

static int my_shm_id = -1;
static int my_sem_id = -1;
static pid_t my_pid;

static void generator_signal_handler(int sig) {
    printf("Generador PID %d: Señal %d recibida. Terminando...\n", my_pid, sig);
    exit(EXIT_SUCCESS);
}

static int request_id_block(shared_memory_t *shared_mem, int sem_id, int *start_id, int *end_id) {
    sem_wait(sem_id, SEM_REQUEST_TURN);
    sem_wait(sem_id, SEM_REQUEST_MUTEX);

    shared_mem->request_slot.request.action = REQUEST_IDS;
    shared_mem->request_slot.request.process_id = my_pid;

    sem_post(sem_id, SEM_REQUEST_MUTEX);
    sem_post(sem_id, SEM_REQUEST_AVAILABLE);

    sem_wait(sem_id, SEM_REQUEST_DONE);
    sem_wait(sem_id, SEM_REQUEST_MUTEX);

    id_request_t response = shared_mem->request_slot.request;

    sem_post(sem_id, SEM_REQUEST_MUTEX);
    sem_post(sem_id, SEM_REQUEST_TURN);

    if (response.action == NO_MORE_IDS) {
        printf("Generador PID %d: No hay más IDs disponibles\n", my_pid);
        return 0;
    }

    *start_id = response.start_id;
    *end_id = response.end_id;
    printf("Generador PID %d: Recibidos IDs %d-%d\n", my_pid, *start_id, *end_id);
    return (*end_id >= *start_id) ? (*end_id - *start_id + 1) : 0;
}

static int enqueue_record(shared_memory_t *shared_mem, int sem_id, const record_t *record) {
    sem_wait(sem_id, SEM_RECORD_EMPTY);
    sem_wait(sem_id, SEM_RECORD_MUTEX);

    record_ring_t *ring = &shared_mem->record_ring;
    ring->records[ring->write_index] = *record;
    ring->write_index = (ring->write_index + 1) % RECORD_BUFFER_SIZE;
    ring->count++;

    sem_post(sem_id, SEM_RECORD_MUTEX);
    sem_post(sem_id, SEM_RECORD_AVAILABLE);
    return 0;
}

static void generate_record(record_t *record, int id) {
    record->action = SEND_RECORD;
    record->id = id;
    record->process_id = my_pid;
    record->timestamp = time(NULL);

    generate_random_data(record->random_data, MAX_DATA_SIZE / 2);

    char temp_data[MAX_DATA_SIZE];
    int written = snprintf(temp_data, sizeof(temp_data), "DATA_%d_%ld_%s",
                           id, record->timestamp, record->random_data);

    if (written >= (int)sizeof(temp_data)) {
        snprintf(temp_data, sizeof(temp_data), "DATA_%d_%ld", id, record->timestamp);
    }

    strncpy(record->random_data, temp_data, MAX_DATA_SIZE - 1);
    record->random_data[MAX_DATA_SIZE - 1] = '\0';
}

static void generator_main_loop(shared_memory_t *shared_mem, int sem_id) {
    int start_id;
    int end_id;
    int records_sent = 0;

    printf("Generador PID %d iniciando bucle principal...\n", my_pid);

    while (1) {
        int ids_received = request_id_block(shared_mem, sem_id, &start_id, &end_id);
        if (ids_received == 0) {
            break;
        }

        printf("Generador PID %d: Procesando IDs %d-%d\n", my_pid, start_id, end_id);
        for (int current_id = start_id; current_id <= end_id; current_id++) {
            record_t record;
            generate_record(&record, current_id);

            if (enqueue_record(shared_mem, sem_id, &record) == 0) {
                records_sent++;
                if (records_sent % 10 == 0) {
                    printf("Generador PID %d: Enviados %d registros\n", my_pid, records_sent);
                }
            } else {
                fprintf(stderr, "Generador PID %d: Error encolando registro ID %d\n", my_pid, current_id);
            }

            usleep(10000);
        }
    }

    printf("Generador PID %d terminado. Total enviados: %d registros\n", my_pid, records_sent);
}

int main(int argc, char *argv[]) {
    shared_memory_t *shared_mem;

    my_pid = getpid();

    if (argc != 3) {
        fprintf(stderr, "Generador PID %d: Argumentos incorrectos\n", my_pid);
        fprintf(stderr, "Uso: %s <shm_id> <sem_id>\n", argv[0]);
        return EXIT_FAILURE;
    }

    my_shm_id = atoi(argv[1]);
    my_sem_id = atoi(argv[2]);

    printf("Generador PID %d iniciado (SHM: %d, SEM: %d)\n",
           my_pid, my_shm_id, my_sem_id);

    signal(SIGTERM, generator_signal_handler);
    signal(SIGINT, generator_signal_handler);

    srand(time(NULL) + my_pid);

    shared_mem = (shared_memory_t *)shmat(my_shm_id, NULL, 0);
    if (shared_mem == (void *)-1) {
        perror("Generador: Error adjuntando memoria compartida");
        return EXIT_FAILURE;
    }

    generator_main_loop(shared_mem, my_sem_id);

    if (shmdt(shared_mem) == -1) {
        perror("Generador: Error desadjuntando memoria compartida");
    }

    printf("Generador PID %d finalizado correctamente\n", my_pid);
    return EXIT_SUCCESS;
}