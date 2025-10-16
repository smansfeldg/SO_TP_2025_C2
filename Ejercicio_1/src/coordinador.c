/*
 * coordinador.c - Proceso coordinador del sistema de generación de datos
 *
 * Sistema Generador de Datos Concurrente con Memoria Compartida
 * Ejercicio 1 - Sistemas Operativos
 */

#include "../include/shared.h"

static int total_records = 0;
static int num_generators = 0;
static int ids_assigned = 0;
static int records_written = 0;
static int next_assign_id = 1;
static int next_expected_id = 1;
static FILE *csv_file = NULL;
static pid_t *generator_pids = NULL;
static record_t *records_by_id = NULL;
static unsigned char *record_flags = NULL;
static shared_memory_t *shared_mem_ptr = NULL;

static void show_usage(const char *program_name) {
	fprintf(stderr, "Uso: %s <total_registros> <num_generadores>\n", program_name);
}

static int validate_parameters(int argc, char *argv[]) {
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

	return 0;
}

static int initialize_runtime_buffers(void) {
	records_by_id = calloc((size_t)(total_records + 1), sizeof(record_t));
	if (records_by_id == NULL) {
		perror("Error reservando memoria para registros");
		return -1;
	}

	record_flags = calloc((size_t)(total_records + 1), sizeof(unsigned char));
	if (record_flags == NULL) {
		perror("Error reservando memoria para banderas de registros");
		free(records_by_id);
		records_by_id = NULL;
		return -1;
	}

	return 0;
}

static void free_runtime_buffers(void) {
	if (records_by_id != NULL) {
		free(records_by_id);
		records_by_id = NULL;
	}
	if (record_flags != NULL) {
		free(record_flags);
		record_flags = NULL;
	}
}

static int initialize_csv_file(void) {
	csv_file = fopen(CSV_FILENAME, "w");
	if (csv_file == NULL) {
		perror("Error creando archivo CSV");
		return -1;
	}

	fprintf(csv_file, "ID,ID_PROCESO,TIMESTAMP,DATO_ALEATORIO\n");
	fflush(csv_file);
	return 0;
}

static void close_csv(void) {
	if (csv_file != NULL) {
		fclose(csv_file);
		csv_file = NULL;
	}
}

static void write_record_to_csv(const record_t *record) {
	fprintf(csv_file, "%d,%d,%ld,%s\n",
			record->id,
			record->process_id,
			(long)record->timestamp,
			record->random_data);
	fflush(csv_file);
	records_written++;
}

static void flush_pending_records(void) {
	while (next_expected_id <= total_records && record_flags[next_expected_id]) {
		write_record_to_csv(&records_by_id[next_expected_id]);
		record_flags[next_expected_id] = 0;
		next_expected_id++;
	}
}

static void handle_incoming_record(const record_t *record) {
	if (record->id <= 0 || record->id > total_records) {
		fprintf(stderr, "Registro con ID inválido recibido: %d\n", record->id);
		return;
	}

	if (record->id == next_expected_id) {
		write_record_to_csv(record);
		next_expected_id++;
		flush_pending_records();
	} else if (record->id > next_expected_id) {
		records_by_id[record->id] = *record;
		record_flags[record->id] = 1;
	}
}

static void serve_request(shared_memory_t *shared_mem, int sem_id) {
	sem_wait(sem_id, SEM_REQUEST_MUTEX);

	id_request_t *request = &shared_mem->request_slot.request;
	pid_t requester = request->process_id;

	if (ids_assigned >= total_records) {
		request->action = NO_MORE_IDS;
		request->start_id = 0;
		request->end_id = 0;
	} else {
		int remaining = total_records - ids_assigned;
		int batch = remaining < ID_BLOCK_SIZE ? remaining : ID_BLOCK_SIZE;
		request->start_id = next_assign_id;
		request->end_id = next_assign_id + batch - 1;
		request->action = ASSIGN_IDS;
		ids_assigned += batch;
		next_assign_id += batch;
		printf("Asignados IDs %d-%d al PID %d\n", request->start_id, request->end_id, requester);
	}

	sem_post(sem_id, SEM_REQUEST_MUTEX);
	sem_post(sem_id, SEM_REQUEST_DONE);
}

static int try_serve_request(shared_memory_t *shared_mem, int sem_id) {
	if (sem_trywait(sem_id, SEM_REQUEST_AVAILABLE) == -1) {
		return 0;
	}
	serve_request(shared_mem, sem_id);
	return 1;
}

static int wait_and_serve_request(shared_memory_t *shared_mem, int sem_id, const struct timespec *timeout) {
	if (sem_wait_timed(sem_id, SEM_REQUEST_AVAILABLE, timeout) == -1) {
		return 0;
	}
	serve_request(shared_mem, sem_id);
	return 1;
}

static void extract_record(shared_memory_t *shared_mem, int sem_id, record_t *out) {
	sem_wait(sem_id, SEM_RECORD_MUTEX);
	record_ring_t *ring = &shared_mem->record_ring;
	*out = ring->records[ring->read_index];
	ring->read_index = (ring->read_index + 1) % RECORD_BUFFER_SIZE;
	if (ring->count > 0) {
		ring->count--;
	}
	sem_post(sem_id, SEM_RECORD_MUTEX);
	sem_post(sem_id, SEM_RECORD_EMPTY);
}

static int try_extract_record(shared_memory_t *shared_mem, int sem_id, record_t *out) {
	if (sem_trywait(sem_id, SEM_RECORD_AVAILABLE) == -1) {
		return 0;
	}
	extract_record(shared_mem, sem_id, out);
	return 1;
}

static int wait_and_extract_record(shared_memory_t *shared_mem, int sem_id, record_t *out, const struct timespec *timeout) {
	if (sem_wait_timed(sem_id, SEM_RECORD_AVAILABLE, timeout) == -1) {
		return 0;
	}
	extract_record(shared_mem, sem_id, out);
	return 1;
}

static int count_active_generators(void) {
	int active = 0;

	if (generator_pids == NULL) {
		return 0;
	}

	for (int i = 0; i < num_generators; i++) {
		if (generator_pids[i] > 0 && kill(generator_pids[i], 0) == 0) {
			active++;
		}
	}

	return active;
}

static void sigchld_handler(int sig) {
	(void)sig;
	int status;
	pid_t pid;

	while ((pid = waitpid(-1, &status, WNOHANG)) > 0) {
		if (generator_pids != NULL) {
			for (int i = 0; i < num_generators; i++) {
				if (generator_pids[i] == pid) {
					generator_pids[i] = -1;
					break;
				}
			}
		}
	}
}

static void terminate_generators(void) {
	if (generator_pids == NULL) {
		return;
	}

	for (int i = 0; i < num_generators; i++) {
		if (generator_pids[i] > 0) {
			kill(generator_pids[i], SIGTERM);
		}
	}

	int status;
	while (waitpid(-1, &status, WNOHANG) > 0) {
		/* Descarta procesos zombies */
	}

	free(generator_pids);
	generator_pids = NULL;
}

static void cleanup_coordinator(void) {
	close_csv();
	terminate_generators();

	if (shared_mem_ptr != NULL) {
		shmdt(shared_mem_ptr);
		shared_mem_ptr = NULL;
	}

	cleanup_resources(g_shm_id, g_sem_id);
	g_shm_id = -1;
	g_sem_id = -1;

	free_runtime_buffers();
}

static void coordinator_signal_handler(int sig) {
	printf("\nCoordinador: Señal %d recibida. Limpiando recursos...\n", sig);
	cleanup_coordinator();
	exit(EXIT_SUCCESS);
}

static int spawn_generators(int shm_id, int sem_id) {
	generator_pids = malloc((size_t)num_generators * sizeof(pid_t));
	if (generator_pids == NULL) {
		perror("Error reservando memoria para PIDs");
		return -1;
	}

	for (int i = 0; i < num_generators; i++) {
		pid_t pid = fork();
		if (pid < 0) {
			perror("Error en fork()");
			for (int j = 0; j < i; j++) {
				if (generator_pids[j] > 0) {
					kill(generator_pids[j], SIGTERM);
				}
			}
			int status;
			while (waitpid(-1, &status, WNOHANG) > 0) {
				/* Limpia procesos lanzados antes del fallo */
			}
			free(generator_pids);
			generator_pids = NULL;
			return -1;
		} else if (pid == 0) {
			char shm_str[32];
			char sem_str[32];
			snprintf(shm_str, sizeof(shm_str), "%d", shm_id);
			snprintf(sem_str, sizeof(sem_str), "%d", sem_id);
			execl("./generador", "generador", shm_str, sem_str, NULL);
			perror("Error ejecutando generador");
			_exit(EXIT_FAILURE);
		} else {
			generator_pids[i] = pid;
			printf("Generador %d lanzado con PID %d\n", i + 1, pid);
		}
	}

	return 0;
}

static void coordinator_main_loop(shared_memory_t *shared_mem, int sem_id) {
	printf("Coordinador iniciando bucle principal...\n");
	const struct timespec wait_slice = {0, 200000000};
	record_t record;

	while (1) {
		int did_work = 0;

		while (try_extract_record(shared_mem, sem_id, &record)) {
			handle_incoming_record(&record);
			did_work = 1;
		}

		if (try_serve_request(shared_mem, sem_id)) {
			did_work = 1;
		}

		if (records_written >= total_records && ids_assigned >= total_records) {
			if (count_active_generators() == 0) {
				break;
			}
		}

		if (did_work) {
			continue;
		}

		if (wait_and_extract_record(shared_mem, sem_id, &record, &wait_slice)) {
			handle_incoming_record(&record);
			continue;
		}

		if (wait_and_serve_request(shared_mem, sem_id, &wait_slice)) {
			continue;
		}

		if (records_written >= total_records && ids_assigned >= total_records && count_active_generators() == 0) {
			break;
		}
	}

	printf("Coordinador finalizado. Registros escritos: %d/%d\n", records_written, total_records);
}

int main(int argc, char *argv[]) {
	shared_memory_t *shared_mem;
	int shm_id;
	int sem_id;

	printf("=== Sistema Generador de Datos Concurrente ===\n");
	printf("Coordinador iniciando...\n");

	if (validate_parameters(argc, argv) == -1) {
		show_usage(argv[0]);
		return EXIT_FAILURE;
	}

	if (initialize_runtime_buffers() == -1) {
		return EXIT_FAILURE;
	}

	signal(SIGINT, coordinator_signal_handler);
	signal(SIGTERM, coordinator_signal_handler);
	signal(SIGCHLD, sigchld_handler);

	shm_id = shmget(SHM_KEY, sizeof(shared_memory_t), IPC_CREAT | IPC_EXCL | 0666);
	if (shm_id == -1) {
		perror("Error creando memoria compartida");
		free_runtime_buffers();
		return EXIT_FAILURE;
	}
	g_shm_id = shm_id;

	shared_mem = (shared_memory_t *)shmat(shm_id, NULL, 0);
	if (shared_mem == (void *)-1) {
		perror("Error adjuntando memoria compartida");
		cleanup_resources(shm_id, -1);
		free_runtime_buffers();
		return EXIT_FAILURE;
	}
	shared_mem_ptr = shared_mem;
	memset(shared_mem, 0, sizeof(shared_memory_t));

	sem_id = create_semaphore(SEM_KEY, SEM_COUNT);
	if (sem_id == -1) {
		shmdt(shared_mem_ptr);
		shared_mem_ptr = NULL;
		cleanup_resources(shm_id, -1);
		free_runtime_buffers();
		return EXIT_FAILURE;
	}
	g_sem_id = sem_id;

	if (initialize_csv_file() == -1) {
		cleanup_coordinator();
		return EXIT_FAILURE;
	}

	if (spawn_generators(shm_id, sem_id) == -1) {
		cleanup_coordinator();
		return EXIT_FAILURE;
	}

	coordinator_main_loop(shared_mem, sem_id);

	cleanup_coordinator();
	return EXIT_SUCCESS;
}
