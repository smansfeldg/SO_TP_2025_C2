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
static int restart_failed_generators = 0;  /* Opción para reiniciar generadores fallidos */
static int max_generator_failures = 3;     /* Máximo de fallos por generador */
static int *generator_failure_count = NULL; /* Contador de fallos por generador */

/*
 * show_usage - Muestra información de uso del programa
 * @program_name: Nombre del programa
 */
void show_usage(const char *program_name) {
    printf("Uso: %s <total_registros> <num_generadores> [opciones]\n", program_name);
    printf("  total_registros: Número total de registros a generar (> 0)\n");
    printf("  num_generadores: Número de procesos generadores (> 0)\n");
    printf("\nOpciones:\n");
    printf("  --restart-failed: Reiniciar automáticamente generadores fallidos\n");
    printf("  --max-failures=N: Máximo de fallos por generador (default: 3)\n");
    printf("\nEjemplos:\n");
    printf("  %s 1000 4\n", program_name);
    printf("  %s 1000 4 --restart-failed\n", program_name);
    printf("  %s 1000 4 --restart-failed --max-failures=5\n", program_name);
}

/*
 * validate_parameters - Valida los parámetros de entrada
 * @argc: Número de argumentos
 * @argv: Array de argumentos
 * @return: 0 si válidos, -1 si inválidos
 */
int validate_parameters(int argc, char *argv[]) {
    if (argc < 3) {
        fprintf(stderr, "Error: Número insuficiente de argumentos\n");
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
    
    /* Procesar opciones adicionales */
    for (int i = 3; i < argc; i++) {
        if (strcmp(argv[i], "--restart-failed") == 0) {
            restart_failed_generators = 1;
            printf("Opción habilitada: Reiniciar generadores fallidos\n");
        } else if (strncmp(argv[i], "--max-failures=", 15) == 0) {
            max_generator_failures = atoi(argv[i] + 15);
            if (max_generator_failures <= 0) {
                fprintf(stderr, "Error: max-failures debe ser mayor que 0\n");
                return -1;
            }
            printf("Configurado: Máximo %d fallos por generador\n", max_generator_failures);
        } else {
            fprintf(stderr, "Opción desconocida: %s\n", argv[i]);
            return -1;
        }
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
    
    /* Inicializar contador de fallos por generador */
    generator_failure_count = calloc(num_generators, sizeof(int));
    if (generator_failure_count == NULL) {
        perror("Error asignando memoria para contador de fallos");
        free(generator_pids);
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
 * restart_failed_generator - Reinicia un generador fallido (opcional)
 * @shm_id: ID de memoria compartida
 * @sem_id: ID de semáforos  
 * @generator_index: Índice del generador a reiniciar
 * @return: 0 si exitoso, -1 en error
 */
int restart_failed_generator(int shm_id, int sem_id, int generator_index) {
    if (generator_index < 0 || generator_index >= num_generators) {
        return -1;
    }
    
    pid_t pid = fork();
    if (pid < 0) {
        perror("Error reiniciando generador");
        return -1;
    } else if (pid == 0) {
        /* Proceso hijo - ejecutar generador */
        char shm_str[32], sem_str[32];
        snprintf(shm_str, sizeof(shm_str), "%d", shm_id);
        snprintf(sem_str, sizeof(sem_str), "%d", sem_id);
        
        execl("./generador", "generador", shm_str, sem_str, NULL);
        perror("Error ejecutando generador reiniciado");
        exit(EXIT_FAILURE);
    } else {
        /* Proceso padre - actualizar PID */
        generator_pids[generator_index] = pid;
        printf("🔄 Generador reiniciado en índice %d con PID: %d\n", generator_index, pid);
        return 0;
    }
}

/*
 * count_active_generators - Cuenta cuántos generadores siguen activos
 * @return: Número de generadores activos
 */
int count_active_generators(void) {
    int active = 0;
    if (generator_pids == NULL) return 0;
    
    for (int i = 0; i < num_generators; i++) {
        if (generator_pids[i] > 0) {
            /* Verificar si el proceso sigue activo */
            if (kill(generator_pids[i], 0) == 0) {
                active++;
            } else {
                /* El proceso ya no existe, marcarlo como terminado */
                generator_pids[i] = -1;
            }
        }
    }
    return active;
}

/*
 * coordinator_main_loop - Bucle principal del coordinador
 * @shared_mem: Puntero a memoria compartida
 * @sem_id: ID de semáforos
 */
void coordinator_main_loop(shared_memory_t *shared_mem, int sem_id) {
    printf("Coordinador iniciando bucle principal...\n");
    int failed_generators = 0;
    
    while (records_processed < total_records) {
        /* Verificar si hay generadores activos */
        int active_generators = count_active_generators();
        
        if (active_generators == 0) {
            printf("⚠️  Todos los generadores han terminado.\n");
            printf("Registros procesados: %d/%d\n", records_processed, total_records);
            if (records_processed < total_records) {
                printf("⚠️  No se alcanzó el objetivo de registros debido a fallas en generadores.\n");
            }
            break;
        }
        
        /* Reportar cambios en el número de generadores */
        int current_failed = num_generators - active_generators;
        if (current_failed > failed_generators) {
            failed_generators = current_failed;
            printf("⚠️  Generadores fallidos: %d/%d. Continuando con %d activos.\n", 
                   failed_generators, num_generators, active_generators);
        }
        
        /* Esperar solicitud de generador con timeout */
        struct timespec timeout;
        timeout.tv_sec = 5;  /* 5 segundos de timeout (más tiempo para ser tolerante) */
        timeout.tv_nsec = 0;
        
        struct sembuf sb = {SEM_COORDINATOR, -1, 0};
        if (semtimedop(sem_id, &sb, 1, &timeout) == -1) {
            if (errno == EAGAIN) {
                /* Timeout - verificar estado y continuar */
                printf("Timeout esperando generadores. Generadores activos: %d\n", active_generators);
                continue;
            } else if (errno == EINTR) {
                /* Interrupción por señal (SIGCHLD) - es normal, continuar */
                printf("Operación interrumpida por señal. Continuando...\n");
                continue;
            } else {
                perror("Error en semtimedop");
                /* No romper inmediatamente, intentar continuar */
                continue;
            }
        }
        
        sem_wait(sem_id, SEM_MUTEX);
        
        printf("DEBUG: Estado de memoria compartida: %d\n", shared_mem->status);
        
        if (shared_mem->status == REQUEST_IDS) {
            /* Asignar bloque de IDs */
            printf("DEBUG: Procesando solicitud de IDs de proceso %d\n", shared_mem->data.request.process_id);
            assign_id_block(shared_mem);
            /* Notificar al generador que la respuesta está lista */
            sem_post(sem_id, SEM_COORDINATOR);
        } else if (shared_mem->status == SEND_RECORD) {
            /* Procesar registro */
            printf("DEBUG: Procesando registro ID %d de proceso %d\n", 
                   shared_mem->data.record.id, shared_mem->data.record.process_id);
            process_record(shared_mem);
            /* Notificar al generador que el registro fue procesado */
            sem_post(sem_id, SEM_COORDINATOR);
        } else {
            printf("DEBUG: Estado de memoria compartida desconocido: %d\n", shared_mem->status);
        }
        
        sem_post(sem_id, SEM_MUTEX);
    }
    
    printf("Coordinador terminando bucle principal.\n");
    printf("Registros procesados: %d/%d\n", records_processed, total_records);
}

/*
 * sigchld_handler - Manejador para señales SIGCHLD (evitar procesos zombie)
 * @sig: Número de señal recibida
 */
void sigchld_handler(int sig) {
    (void)sig;  /* Suprimir warning de parámetro no usado */
    int status;
    pid_t pid;
    
    /* Recolectar todos los procesos hijos que hayan terminado */
    while ((pid = waitpid(-1, &status, WNOHANG)) > 0) {
        /* Determinar la causa de terminación */
        if (WIFEXITED(status)) {
            int exit_code = WEXITSTATUS(status);
            if (exit_code == 0) {
                printf("✅ Generador PID %d terminó normalmente\n", pid);
            } else {
                printf("⚠️  Generador PID %d terminó con código de error %d\n", pid, exit_code);
            }
        } else if (WIFSIGNALED(status)) {
            int signal_num = WTERMSIG(status);
            printf("⚠️  Generador PID %d terminado por señal %d (%s)\n", 
                   pid, signal_num, strsignal(signal_num));
        }
        
        /* Marcar el PID como terminado en nuestro array y manejar reinicio */
        if (generator_pids != NULL) {
            for (int i = 0; i < num_generators; i++) {
                if (generator_pids[i] == pid) {
                    generator_pids[i] = -1; /* Marcar como terminado */
                    
                    /* Si está habilitado el reinicio y no hemos excedido el límite */
                    if (restart_failed_generators && 
                        generator_failure_count[i] < max_generator_failures &&
                        records_processed < total_records) {
                        
                        generator_failure_count[i]++;
                        printf("🔄 Reiniciando generador %d (intento %d/%d)...\n", 
                               i, generator_failure_count[i], max_generator_failures);
                        
                        /* Reiniciar generador después de un breve delay */
                        sleep(1);
                        if (restart_failed_generator(g_shm_id, g_sem_id, i) != 0) {
                            printf("❌ No se pudo reiniciar generador %d\n", i);
                        }
                    } else if (restart_failed_generators && 
                              generator_failure_count[i] >= max_generator_failures) {
                        printf("❌ Generador %d ha excedido el límite de fallos (%d)\n", 
                               i, max_generator_failures);
                    }
                    break;
                }
            }
        }
    }
}

/*
 * terminate_generators - Termina todos los procesos generadores
 */
void terminate_generators(void) {
    if (generator_pids == NULL) return;
    
    printf("Terminando procesos generadores...\n");
    
    /* Enviar señal SIGTERM a todos los generadores activos */
    for (int i = 0; i < num_generators; i++) {
        if (generator_pids[i] > 0) {
            printf("Enviando SIGTERM a generador PID %d\n", generator_pids[i]);
            if (kill(generator_pids[i], SIGTERM) == -1) {
                perror("Error enviando SIGTERM");
            }
        }
    }
    
    /* Dar tiempo para terminación ordenada */
    sleep(1);
    
    /* Verificar cuáles siguen activos y forzar terminación si es necesario */
    for (int i = 0; i < num_generators; i++) {
        if (generator_pids[i] > 0) {
            /* Verificar si el proceso sigue activo */
            if (kill(generator_pids[i], 0) == 0) {
                printf("Forzando terminación del generador PID %d\n", generator_pids[i]);
                kill(generator_pids[i], SIGKILL);
            }
        }
    }
    
    /* Recolectar cualquier proceso zombie restante */
    int status;
    pid_t pid;
    while ((pid = waitpid(-1, &status, WNOHANG)) > 0) {
        printf("Recolectado proceso zombie PID %d\n", pid);
    }
    
    free(generator_pids);
    generator_pids = NULL;
    
    if (generator_failure_count != NULL) {
        free(generator_failure_count);
        generator_failure_count = NULL;
    }
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
    
    /* Configurar manejadores de señales */
    signal(SIGINT, coordinator_signal_handler);
    signal(SIGTERM, coordinator_signal_handler);
    signal(SIGCHLD, sigchld_handler);  /* Evitar procesos zombie */
    
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
    shared_mem->next_id_to_write = 1;  /* Primer ID esperado para escritura ordenada */
    
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