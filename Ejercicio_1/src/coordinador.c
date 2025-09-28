/*
 * coordinador.c - Proceso coordinador del sistema de generación de datos
 * 
 * Sistema Generador de Datos Concurrente con Memoria Compartida
 * Ejercicio 1 - Sistemas Operativos
 * 
 * Uso: ./coordinador <total_registros> <num_generadores>
 */

#include "../include/shared.h"

/* Variables globales */
static int total_records = 0;
static int num_generators = 0;
static int records_processed = 0;
static int next_id = 1;
static FILE *csv_file = NULL;
static pid_t *generator_pids = NULL;

/*
 * show_usage - Muestra información de uso del programa
 * @program_name: Nombre del programa
 */
void show_usage(const char *program_name) {
    printf("Uso: %s <total_registros> <num_generadores>\n", program_name);
    printf("  total_registros: Número total de registros a generar (> 0)\n");
    printf("  num_generadores: Número de procesos generadores (> 0)\n");
    printf("\nEjemplo: %s 1000 4\n", program_name);
}

/*
 * validate_parameters - Valida los parámetros de entrada
 * @argc: Número de argumentos
 * @argv: Array de argumentos
 * @return: 0 si válidos, -1 si inválidos
 */
int validate_parameters(int argc, char *argv[]) {
    if (argc != 3) {
        fprintf(stderr, "Error: Número incorrecto de argumentos\n");
        return -1;
    }
    
    total_records = atoi(argv[1]);
    num_generators = atoi(argv[2]);
    
    if (total_records <= 0) {
        fprintf(stderr, "Error: El total de registros debe ser mayor que 0\n");
        return -1;
    }
    
    if (num_generators <= 0) {
        fprintf(stderr, "Error: El número de generadores debe ser mayor que 0\n");
        return -1;
    }
    
    if (num_generators > total_records) {
        fprintf(stderr, "Error: El número de generadores no puede ser mayor al total de registros\n");
        return -1;
    }
    
    return 0;
}

/*
 * initialize_csv_file - Inicializa el archivo CSV con cabeceras
 * @return: 0 si exitoso, -1 en error
 */
int initialize_csv_file(void) {
    csv_file = fopen(CSV_FILENAME, "w");
    if (csv_file == NULL) {
        perror("Error creando archivo CSV");
        return -1;
    }
    
    /* Escribir cabecera */
    fprintf(csv_file, "ID,ID_PROCESO,TIMESTAMP,DATO_ALEATORIO\n");
    fflush(csv_file);
    
    printf("Archivo CSV '%s' creado con cabecera\n", CSV_FILENAME);
    return 0;
}

/*
 * create_generators - Crea los procesos generadores
 * @shm_id: ID de memoria compartida
 * @sem_id: ID de semáforos
 * @return: 0 si exitoso, -1 en error
 */
int create_generators(int shm_id, int sem_id) {
    generator_pids = malloc(num_generators * sizeof(pid_t));
    if (generator_pids == NULL) {
        perror("Error asignando memoria para PIDs");
        return -1;
    }
    
    for (int i = 0; i < num_generators; i++) {
        pid_t pid = fork();
        
        if (pid < 0) {
            perror("Error en fork()");
            return -1;
        } else if (pid == 0) {
            /* Proceso hijo - ejecutar generador */
            char shm_str[32], sem_str[32];
            snprintf(shm_str, sizeof(shm_str), "%d", shm_id);
            snprintf(sem_str, sizeof(sem_str), "%d", sem_id);
            
            execl("./generador", "generador", shm_str, sem_str, NULL);
            perror("Error ejecutando generador");
            exit(EXIT_FAILURE);
        } else {
            /* Proceso padre - almacenar PID */
            generator_pids[i] = pid;
            printf("Generador %d creado con PID: %d\n", i + 1, pid);
        }
    }
    
    return 0;
}

/*
 * assign_id_block - Asigna un bloque de IDs a un generador
 * @shared_mem: Puntero a memoria compartida
 * @return: Número de IDs asignados
 */
int assign_id_block(shared_memory_t *shared_mem) {
    int remaining_ids = total_records - next_id + 1;
    int ids_to_assign = (remaining_ids > ID_BLOCK_SIZE) ? ID_BLOCK_SIZE : remaining_ids;
    
    if (ids_to_assign <= 0) {
        shared_mem->data.request.action = NO_MORE_IDS;
        return 0;
    }
    
    shared_mem->data.request.start_id = next_id;
    shared_mem->data.request.end_id = next_id + ids_to_assign - 1;
    next_id += ids_to_assign;
    
    printf("Asignados IDs %d-%d al proceso %d\n", 
           shared_mem->data.request.start_id, 
           shared_mem->data.request.end_id,
           shared_mem->data.request.process_id);
    
    return ids_to_assign;
}

/*
 * process_record - Procesa un registro recibido de un generador
 * @shared_mem: Puntero a memoria compartida
 * @return: 0 si exitoso, -1 en error
 */
int process_record(shared_memory_t *shared_mem) {
    record_t *record = &shared_mem->data.record;
    
    /* Escribir registro al CSV */
    fprintf(csv_file, "%d,%d,%ld,%s\n",
            record->id,
            record->process_id,
            record->timestamp,
            record->random_data);
    fflush(csv_file);
    
    records_processed++;
    
    if (records_processed % 100 == 0) {
        printf("Procesados %d/%d registros\n", records_processed, total_records);
    }
    
    return 0;
}

/*
 * coordinator_main_loop - Bucle principal del coordinador
 * @shared_mem: Puntero a memoria compartida
 * @sem_id: ID de semáforos
 */
void coordinator_main_loop(shared_memory_t *shared_mem, int sem_id) {
    printf("Coordinador iniciando bucle principal...\n");
    
    while (records_processed < total_records) {
        /* Esperar solicitud de generador */
        sem_wait(sem_id, SEM_COORDINATOR);
        sem_wait(sem_id, SEM_MUTEX);
        
        if (shared_mem->status == REQUEST_IDS) {
            /* Asignar bloque de IDs */
            assign_id_block(shared_mem);
        } else if (shared_mem->status == SEND_RECORD) {
            /* Procesar registro */
            process_record(shared_mem);
        }
        
        sem_post(sem_id, SEM_MUTEX);
        sem_post(sem_id, SEM_GENERATOR);
    }
    
    printf("Todos los registros han sido procesados\n");
}

/*
 * terminate_generators - Termina todos los procesos generadores
 */
void terminate_generators(void) {
    if (generator_pids == NULL) return;
    
    printf("Terminando procesos generadores...\n");
    
    for (int i = 0; i < num_generators; i++) {
        if (generator_pids[i] > 0) {
            kill(generator_pids[i], SIGTERM);
        }
    }
    
    /* Esperar que terminen */
    for (int i = 0; i < num_generators; i++) {
        if (generator_pids[i] > 0) {
            int status;
            waitpid(generator_pids[i], &status, 0);
            printf("Generador PID %d terminado\n", generator_pids[i]);
        }
    }
    
    free(generator_pids);
}

/*
 * cleanup_coordinator - Limpia recursos del coordinador
 */
void cleanup_coordinator(void) {
    if (csv_file != NULL) {
        fclose(csv_file);
        printf("Archivo CSV cerrado\n");
    }
    
    terminate_generators();
    cleanup_resources(g_shm_id, g_sem_id);
}

/*
 * coordinator_signal_handler - Manejador de señales específico del coordinador
 */
void coordinator_signal_handler(int sig) {
    printf("\nCoordinador: Señal %d recibida. Limpiando recursos...\n", sig);
    cleanup_coordinator();
    exit(EXIT_SUCCESS);
}

/*
 * main - Función principal del coordinador
 */
int main(int argc, char *argv[]) {
    shared_memory_t *shared_mem;
    int shm_id, sem_id;
    
    printf("=== Sistema Generador de Datos Concurrente ===\n");
    printf("Coordinador iniciando...\n");
    
    /* Validar parámetros */
    if (validate_parameters(argc, argv) == -1) {
        show_usage(argv[0]);
        return EXIT_FAILURE;
    }
    
    printf("Configuración: %d registros, %d generadores\n", total_records, num_generators);
    
    /* Configurar manejador de señales */
    signal(SIGINT, coordinator_signal_handler);
    signal(SIGTERM, coordinator_signal_handler);
    
    /* Crear memoria compartida */
    shm_id = shmget(SHM_KEY, sizeof(shared_memory_t), IPC_CREAT | IPC_EXCL | 0666);
    if (shm_id == -1) {
        perror("Error creando memoria compartida");
        return EXIT_FAILURE;
    }
    g_shm_id = shm_id;
    
    /* Adjuntar memoria compartida */
    shared_mem = (shared_memory_t *)shmat(shm_id, NULL, 0);
    if (shared_mem == (void *)-1) {
        perror("Error adjuntando memoria compartida");
        cleanup_resources(shm_id, -1);
        return EXIT_FAILURE;
    }
    
    /* Inicializar memoria compartida */
    memset(shared_mem, 0, sizeof(shared_memory_t));
    
    /* Crear semáforos */
    sem_id = create_semaphore(SEM_KEY, SEM_COUNT);
    if (sem_id == -1) {
        cleanup_resources(shm_id, -1);
        return EXIT_FAILURE;
    }
    g_sem_id = sem_id;
    
    /* Inicializar archivo CSV */
    if (initialize_csv_file() == -1) {
        cleanup_resources(shm_id, sem_id);
        return EXIT_FAILURE;
    }
    
    /* Crear procesos generadores */
    if (create_generators(shm_id, sem_id) == -1) {
        cleanup_coordinator();
        return EXIT_FAILURE;
    }
    
    /* Ejecutar bucle principal */
    coordinator_main_loop(shared_mem, sem_id);
    
    /* Limpieza final */
    printf("Generación completada. Total de registros: %d\n", records_processed);
    cleanup_coordinator();
    
    return EXIT_SUCCESS;
}