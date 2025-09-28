/*
 * generador.c - Proceso generador de datos
 * 
 * Sistema Generador de Datos Concurrente con Memoria Compartida
 * Ejercicio 1 - Sistemas Operativos
 * 
 * Uso: ./generador <shm_id> <sem_id>
 * (Ejecutado automáticamente por el coordinador)
 */

#include "../include/shared.h"

/* Variables globales del generador */
static int my_shm_id = -1;
static int my_sem_id = -1;
static pid_t my_pid;

/*
 * generator_signal_handler - Manejador de señales para el generador
 * @sig: Número de señal recibida
 */
void generator_signal_handler(int sig) {
    printf("Generador PID %d: Señal %d recibida. Terminando...\n", my_pid, sig);
    exit(EXIT_SUCCESS);
}

/*
 * request_id_block - Solicita un bloque de IDs al coordinador
 * @shared_mem: Puntero a memoria compartida
 * @sem_id: ID de semáforos
 * @start_id: Puntero para almacenar el ID inicial
 * @end_id: Puntero para almacenar el ID final
 * @return: Número de IDs asignados, 0 si no hay más IDs
 */
int request_id_block(shared_memory_t *shared_mem, int sem_id, int *start_id, int *end_id) {
    /* Solicitar acceso a memoria compartida */
    sem_wait(sem_id, SEM_MUTEX);
    
    /* Preparar solicitud */
    shared_mem->status = REQUEST_IDS;
    shared_mem->data.request.action = REQUEST_IDS;
    shared_mem->data.request.process_id = my_pid;
    
    /* Liberar mutex y notificar al coordinador */
    sem_post(sem_id, SEM_MUTEX);
    sem_post(sem_id, SEM_COORDINATOR);
    
    /* Esperar respuesta del coordinador */
    sem_wait(sem_id, SEM_GENERATOR);
    sem_wait(sem_id, SEM_MUTEX);
    
    if (shared_mem->data.request.action == NO_MORE_IDS) {
        sem_post(sem_id, SEM_MUTEX);
        printf("Generador PID %d: No hay más IDs disponibles\n", my_pid);
        return 0;
    }
    
    /* Obtener IDs asignados */
    *start_id = shared_mem->data.request.start_id;
    *end_id = shared_mem->data.request.end_id;
    
    sem_post(sem_id, SEM_MUTEX);
    
    printf("Generador PID %d: Recibidos IDs %d-%d\n", my_pid, *start_id, *end_id);
    return (*end_id - *start_id + 1);
}

/*
 * send_record - Envía un registro al coordinador
 * @shared_mem: Puntero a memoria compartida
 * @sem_id: ID de semáforos
 * @record: Registro a enviar
 * @return: 0 si exitoso, -1 en error
 */
int send_record(shared_memory_t *shared_mem, int sem_id, const record_t *record) {
    /* Esperar acceso a memoria compartida */
    sem_wait(sem_id, SEM_MUTEX);
    
    /* Copiar registro a memoria compartida */
    shared_mem->status = SEND_RECORD;
    memcpy(&shared_mem->data.record, record, sizeof(record_t));
    
    /* Liberar mutex y notificar al coordinador */
    sem_post(sem_id, SEM_MUTEX);
    sem_post(sem_id, SEM_COORDINATOR);
    
    /* Esperar confirmación del coordinador antes de continuar */
    sem_wait(sem_id, SEM_GENERATOR);
    
    return 0;
}

/*
 * generate_record - Genera un registro con datos aleatorios
 * @record: Puntero al registro a generar
 * @id: ID del registro
 */
void generate_record(record_t *record, int id) {
    record->action = SEND_RECORD;
    record->id = id;
    record->process_id = my_pid;
    record->timestamp = time(NULL);
    
    /* Generar datos aleatorios iniciales */
    generate_random_data(record->random_data, MAX_DATA_SIZE / 2); /* Reservar espacio para prefijo */
    
    /* Agregar información adicional para hacer los datos más únicos */
    char temp_data[MAX_DATA_SIZE];
    int written = snprintf(temp_data, sizeof(temp_data), "DATA_%d_%ld_%s", 
                          id, record->timestamp, record->random_data);
    
    /* Verificar si hubo truncamiento y manejarlo */
    if (written >= (int)sizeof(temp_data)) {
        /* Si hay truncamiento, usar solo ID y timestamp */
        snprintf(temp_data, sizeof(temp_data), "DATA_%d_%ld", id, record->timestamp);
    }
    
    /* Copiar resultado final */
    strncpy(record->random_data, temp_data, MAX_DATA_SIZE - 1);
    record->random_data[MAX_DATA_SIZE - 1] = '\0';
}

/*
 * generator_main_loop - Bucle principal del generador
 * @shared_mem: Puntero a memoria compartida
 * @sem_id: ID de semáforos
 */
void generator_main_loop(shared_memory_t *shared_mem, int sem_id) {
    int start_id, end_id;
    int records_sent = 0;
    
    printf("Generador PID %d iniciando bucle principal...\n", my_pid);
    
    while (1) {
        /* Solicitar bloque de IDs */
        int ids_received = request_id_block(shared_mem, sem_id, &start_id, &end_id);
        
        if (ids_received == 0) {
            /* No hay más IDs, terminar */
            break;
        }
        
        /* Generar y enviar registros para cada ID */
        printf("Generador PID %d: Procesando IDs %d-%d\n", my_pid, start_id, end_id);
        for (int current_id = start_id; current_id <= end_id; current_id++) {
            record_t record;
            
            /* Generar registro */
            generate_record(&record, current_id);
            
            /* Enviar registro al coordinador */
            printf("Generador PID %d: Enviando registro ID %d\n", my_pid, current_id);
            if (send_record(shared_mem, sem_id, &record) == 0) {
                records_sent++;
                printf("Generador PID %d: Registro ID %d enviado exitosamente\n", my_pid, current_id);
                
                if (records_sent % 10 == 0) {
                    printf("Generador PID %d: Enviados %d registros\n", my_pid, records_sent);
                }
            } else {
                fprintf(stderr, "Generador PID %d: Error enviando registro ID %d\n", 
                        my_pid, current_id);
            }
            
            /* Pequeña pausa para simular trabajo */
            usleep(10000); /* 10ms para simular trabajo realista */
        }
    }
    
    printf("Generador PID %d terminado. Total enviados: %d registros\n", 
           my_pid, records_sent);
}

/*
 * main - Función principal del generador
 */
int main(int argc, char *argv[]) {
    shared_memory_t *shared_mem;
    
    my_pid = getpid();
    
    /* Validar argumentos */
    if (argc != 3) {
        fprintf(stderr, "Generador PID %d: Argumentos incorrectos\n", my_pid);
        fprintf(stderr, "Uso: %s <shm_id> <sem_id>\n", argv[0]);
        return EXIT_FAILURE;
    }
    
    my_shm_id = atoi(argv[1]);
    my_sem_id = atoi(argv[2]);
    
    printf("Generador PID %d iniciado (SHM: %d, SEM: %d)\n", 
           my_pid, my_shm_id, my_sem_id);
    
    /* Configurar manejador de señales */
    signal(SIGTERM, generator_signal_handler);
    signal(SIGINT, generator_signal_handler);
    
    /* Inicializar generador de números aleatorios */
    srand(time(NULL) + my_pid);
    
    /* Adjuntar memoria compartida */
    shared_mem = (shared_memory_t *)shmat(my_shm_id, NULL, 0);
    if (shared_mem == (void *)-1) {
        perror("Generador: Error adjuntando memoria compartida");
        return EXIT_FAILURE;
    }
    
    /* Ejecutar bucle principal */
    generator_main_loop(shared_mem, my_sem_id);
    
    /* Desadjuntar memoria compartida */
    if (shmdt(shared_mem) == -1) {
        perror("Generador: Error desadjuntando memoria compartida");
    }
    
    printf("Generador PID %d finalizado correctamente\n", my_pid);
    return EXIT_SUCCESS;
}