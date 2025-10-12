#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <sys/file.h> // Para la función flock
#include <fcntl.h>    // Para las banderas de flock
#include <signal.h>    // Para manejo de señales
#include <time.h>     // Para time_t

#define DEFAULT_PORT 8080
#define BUFFER_SIZE 1024
#define DEFAULT_CSV_FILE "../Ejercicio_1/datos_generados.csv"
#define DEFAULT_LOG_FILE "server.log"
#define CONFIG_FILE "servidor.conf"
#define MAX_CONFIG_LINE 256

// Estructura para manejar clientes en cola de espera
typedef struct {
    int socket;
    int posicion;
    time_t tiempo_ingreso;
} cliente_en_espera_t;

// Variables globales para ser accesibles en el handler
int server_fd_global = -1; // Socket de escucha
int clientes_activos = 0;
int max_clientes_concurrentes = 0; // Se establece en main
int max_clientes_espera = 0; // Se establece en main

// Cola de espera
cliente_en_espera_t cola_espera[100]; // Máximo 100 clientes en espera
int clientes_en_cola = 0;

// Variables de configuración
char config_host[256] = "0.0.0.0";
int config_port = DEFAULT_PORT;
char config_csv_file[512] = DEFAULT_CSV_FILE;
char config_log_file[256] = DEFAULT_LOG_FILE;

// Declaraciones forward de funciones
void leer_configuracion(void);
void actualizar_posiciones_cola(void);
void manejar_cliente(int client_socket);
void cleanup_child_handler(int s);
void limpiar_cola_espera(void);
void limpiar_archivos_temporales(void);
int obtener_siguiente_de_cola(void);

// --- Funciones de Utilidad ---

void log_message(const char *message) {
    FILE *log_fp = fopen(config_log_file, "a");
    if (log_fp) {
        fprintf(log_fp, "[LOG] %s\n", message);
        fclose(log_fp);
    }
}

// Función para limpiar todos los archivos temporales
void limpiar_archivos_temporales(void) {
    // Lista de archivos temporales que podrían quedar
    const char *temp_files[] = {
        "temp.csv",
        "temp_backup.csv",
        ".temp_lock",
        NULL
    };
    
    for (int i = 0; temp_files[i] != NULL; i++) {
        if (access(temp_files[i], F_OK) != -1) {
            if (remove(temp_files[i]) == 0) {
                char msg[256];
                snprintf(msg, sizeof(msg), "Archivo temporal eliminado: %s", temp_files[i]);
                log_message(msg);
            }
        }
    }
}

// Función para agregar cliente a la cola de espera
int agregar_a_cola_espera(int client_socket) {
    if (clientes_en_cola >= max_clientes_espera) {
        return -1; // Cola llena
    }
    
    cola_espera[clientes_en_cola].socket = client_socket;
    cola_espera[clientes_en_cola].posicion = clientes_en_cola + 1;
    cola_espera[clientes_en_cola].tiempo_ingreso = time(NULL);
    clientes_en_cola++;
    
    char msg[256];
    snprintf(msg, sizeof(msg), "Cliente agregado a cola de espera. Posicion: %d/%d", 
             clientes_en_cola, max_clientes_espera);
    log_message(msg);
    
    return clientes_en_cola; // Retorna la posición en la cola
}

// Función para obtener el siguiente cliente de la cola
int obtener_siguiente_de_cola(void) {
    if (clientes_en_cola == 0) {
        return -1; // Cola vacía
    }
    
    int client_socket = cola_espera[0].socket;
    
    // Mover todos los elementos hacia adelante
    for (int i = 0; i < clientes_en_cola - 1; i++) {
        cola_espera[i] = cola_espera[i + 1];
        cola_espera[i].posicion = i + 1; // Actualizar posición
    }
    
    clientes_en_cola--;
    
    // Notificar a los clientes restantes su nueva posición
    actualizar_posiciones_cola();
    
    log_message("Cliente removido de cola de espera y promovido.");
    return client_socket;
}

// Función para actualizar las posiciones en la cola
void actualizar_posiciones_cola(void) {
    for (int i = 0; i < clientes_en_cola; i++) {
        char msg[256];
        snprintf(msg, sizeof(msg), "POSICION_ACTUALIZADA: %d/%d\n", i + 1, max_clientes_espera);
        send(cola_espera[i].socket, msg, strlen(msg), 0);
    }
}

// Función para limpiar la cola de espera (cerrar sockets)
void limpiar_cola_espera(void) {
    for (int i = 0; i < clientes_en_cola; i++) {
        const char *msg = "SERVIDOR_CERRANDO: El servidor se está cerrando.\n";
        send(cola_espera[i].socket, msg, strlen(msg), 0);
        close(cola_espera[i].socket);
    }
    clientes_en_cola = 0;
    log_message("Cola de espera limpiada.");
}

// Función para leer el archivo de configuración
void leer_configuracion(void) {
    FILE *config_fp = fopen(CONFIG_FILE, "r");
    if (!config_fp) {
        printf("Archivo de configuracion '%s' no encontrado. Usando valores por defecto.\n", CONFIG_FILE);
        return;
    }
    
    char line[MAX_CONFIG_LINE];
    while (fgets(line, sizeof(line), config_fp)) {
        // Ignorar comentarios y líneas vacías
        if (line[0] == '#' || line[0] == '\n' || line[0] == '\r') {
            continue;
        }
        
        // Eliminar salto de línea
        line[strcspn(line, "\n\r")] = 0;
        
        // Buscar el separador '='
        char *equals = strchr(line, '=');
        if (!equals) continue;
        
        *equals = '\0'; // Dividir la línea
        char *key = line;
        char *value = equals + 1;
        
        // Procesar cada parámetro
        if (strcmp(key, "HOST") == 0) {
            strncpy(config_host, value, sizeof(config_host) - 1);
            config_host[sizeof(config_host) - 1] = '\0';
        } else if (strcmp(key, "PORT") == 0) {
            config_port = atoi(value);
            if (config_port <= 0 || config_port > 65535) {
                printf("Puerto invalido en configuracion: %d. Usando puerto por defecto: %d\n", 
                       config_port, DEFAULT_PORT);
                config_port = DEFAULT_PORT;
            }
        } else if (strcmp(key, "CSV_FILE") == 0) {
            strncpy(config_csv_file, value, sizeof(config_csv_file) - 1);
            config_csv_file[sizeof(config_csv_file) - 1] = '\0';
        } else if (strcmp(key, "LOG_FILE") == 0) {
            strncpy(config_log_file, value, sizeof(config_log_file) - 1);
            config_log_file[sizeof(config_log_file) - 1] = '\0';
        }
    }
    
    fclose(config_fp);
    printf("Configuracion cargada desde '%s':\n", CONFIG_FILE);
    printf("  HOST: %s\n", config_host);
    printf("  PORT: %d\n", config_port);
    printf("  CSV_FILE: %s\n", config_csv_file);
    printf("  LOG_FILE: %s\n", config_log_file);
}

// Handler para recolectar procesos hijos terminados
void sigchld_handler(int s) {
    // waitpid() para evitar zombies
    while (waitpid(-1, NULL, WNOHANG) > 0) {
        clientes_activos--;
        
        // Si hay clientes en cola de espera, promover uno
        if (clientes_en_cola > 0 && clientes_activos < max_clientes_concurrentes) {
            int client_socket = obtener_siguiente_de_cola();
            if (client_socket != -1) {
                // Notificar al cliente que puede proceder
                const char *msg = "CONEXION_APROBADA: Puede proceder con sus comandos.\n";
                send(client_socket, msg, strlen(msg), 0);
                
                // Crear proceso hijo para manejar este cliente
                pid_t pid = fork();
                if (pid == 0) {
                    // Proceso Hijo
                    close(server_fd_global);
                    manejar_cliente(client_socket);
                    exit(EXIT_SUCCESS);
                } else if (pid > 0) {
                    // Proceso Padre
                    close(client_socket);
                    clientes_activos++;
                    printf("Cliente promovido de cola de espera (PID %d). Clientes activos: %d\n", 
                           pid, clientes_activos);
                } else {
                    perror("fork failed para cliente en cola");
                    close(client_socket);
                }
            }
        }
    }
    (void)s; // Silenciar warning de parámetro no usado
}

// --- Bloqueo Transaccional ---

// Función para intentar obtener el bloqueo exclusivo
int obtener_bloqueo_exclusivo(int fd) {
    // LOCK_EX: Bloqueo exclusivo
    // LOCK_NB: No bloqueante (devolvemos error si ya está bloqueado)
    if (flock(fd, LOCK_EX | LOCK_NB) == 0) {
        log_message("Bloqueo exclusivo OBTENIDO.");
        return 1; // Éxito
    }
    log_message("Bloqueo exclusivo FALLIDO (ya activo).");
    return 0; // Fallo
}

// Función para liberar el bloqueo
void liberar_bloqueo(int fd) {
    flock(fd, LOCK_UN); // LOCK_UN: Desbloqueo
    log_message("Bloqueo liberado.");
}

void cleanup_handler(int s) {
    log_message("Señal de terminacion recibida. Iniciando liberacion de recursos.");
    
    // 1. Cerrar el socket de escucha
    if (server_fd_global != -1) {
        close(server_fd_global);
        server_fd_global = -1;
        log_message("Socket de escucha cerrado.");
    }
    
    // 2. Enviar señal de terminación (SIGTERM) a todos los procesos hijos activos
    if (clientes_activos > 0) {
        kill(0, SIGTERM); 
        log_message("SIGTERM enviado a procesos hijos activos.");
        
        // Esperar a que terminen los procesos hijos
        int status;
        while (clientes_activos > 0 && waitpid(-1, &status, WNOHANG) > 0) {
            clientes_activos--;
        }
        log_message("Procesos hijos terminados.");
    }
    
    // 3. Limpiar cola de espera
    limpiar_cola_espera();
    
    // 4. Eliminar archivos temporales
    limpiar_archivos_temporales();
    
    // 5. Cerrar y eliminar archivo de log (opcional, comentar si se quiere mantener)
    if (access(config_log_file, F_OK) != -1) {
        log_message("Servidor finalizado correctamente.");
        // remove(config_log_file); // Comentado para mantener el log
        printf("Log mantenido en: %s\n", config_log_file);
    }
    
    printf("\nServidor finalizado. Todos los recursos liberados.\n");
    (void)s;
    exit(EXIT_SUCCESS);
}


// Handler para limpieza en procesos hijo
void cleanup_child_handler(int s) {
    (void)s; // Silenciar warning de parámetro no usado
    log_message("Proceso hijo recibio señal de terminacion. Limpiando recursos.");
    limpiar_archivos_temporales();
    exit(EXIT_SUCCESS);
}

// --- Lógica del Cliente ---

void manejar_cliente(int client_socket) {
    char buffer[BUFFER_SIZE];
    int bytes_read;
    char response[BUFFER_SIZE];
    
    // Configurar handler de limpieza para el proceso hijo
    struct sigaction sa_child;
    sa_child.sa_handler = cleanup_child_handler;
    sigemptyset(&sa_child.sa_mask);
    sa_child.sa_flags = 0;
    sigaction(SIGTERM, &sa_child, NULL);
    sigaction(SIGINT, &sa_child, NULL);
    
    // Abrir el archivo CSV. Mantener el descriptor abierto para usar flock.
    int csv_fd = open(config_csv_file, O_RDWR | O_CREAT, 0666); 
    if (csv_fd < 0) {
        perror("open CSV_FILE");
        close(client_socket);
        exit(EXIT_FAILURE);
    }

    int transaccion_activa = 0; // Estado de la transacción para ESTE cliente

    log_message("Cliente conectado. Iniciando manejo.");

    // Bucle principal de comunicación con el cliente
    while ((bytes_read = recv(client_socket, buffer, BUFFER_SIZE - 1, 0)) > 0) {
        buffer[bytes_read] = '\0'; // Asegurar terminación nula
        
        // 1. BEGIN TRANSACTION
        if (strncmp(buffer, "BEGIN TRANSACTION", 17) == 0) {
            if (obtener_bloqueo_exclusivo(csv_fd)) {
                transaccion_activa = 1;
                strcpy(response, "OK: Transaccion iniciada. Archivo bloqueado exclusivamente.\n");
            } else {
                strcpy(response, "ERROR: Transaccion activa en otro cliente. Reintente luego.\n");
            }
        } 
        // 2. COMMIT TRANSACTION
        else if (strncmp(buffer, "COMMIT TRANSACTION", 18) == 0) {
            if (transaccion_activa) {
                liberar_bloqueo(csv_fd);
                transaccion_activa = 0;
                strcpy(response, "OK: Transaccion confirmada y bloqueo liberado.\n");
            } else {
                strcpy(response, "ERROR: No hay transaccion activa para hacer COMMIT.\n");
            }
        }
        // 3. CONSULTA (SELECT)
        else if (strncmp(buffer, "SELECT", 6) == 0) {
            // Extraer el ID del comando SELECT ID
            char *token = strtok(buffer, " ");
            token = strtok(NULL, " "); // Obtener el ID
            
            if (token == NULL) {
                strcpy(response, "ERROR: Formato incorrecto. Use: SELECT <ID>\n");
            } else {
                int id_buscado = atoi(token);
                
                // Verificar si hay transacción activa
                if (flock(csv_fd, LOCK_EX | LOCK_NB) == 0) { // Si podemos tomar EXCLUSIVO, está libre
                    liberar_bloqueo(csv_fd); // Lo liberamos inmediatamente
                    
                    // Buscar el registro por ID
                    FILE *csv_file = fopen(config_csv_file, "r");
                    if (csv_file == NULL) {
                        strcpy(response, "ERROR: No se pudo abrir el archivo CSV.\n");
                    } else {
                        char line[BUFFER_SIZE];
                        int found = 0;
                        
                        // Leer encabezado
                        if (fgets(line, sizeof(line), csv_file)) {
                            int remaining = BUFFER_SIZE - 1;
                            strncpy(response, "RESULTADO:\n", remaining);
                            remaining -= strlen("RESULTADO:\n");
                            if (remaining > 0) {
                                strncat(response, line, remaining - 1);
                            }
                        }
                        
                        // Buscar el ID
                        while (fgets(line, sizeof(line), csv_file)) {
                            int current_id;
                            if (sscanf(line, "%d,", &current_id) == 1 && current_id == id_buscado) {
                                int remaining = BUFFER_SIZE - strlen(response) - 1;
                                if (remaining > 0) {
                                    strncat(response, line, remaining);
                                }
                                found = 1;
                                break;
                            }
                        }
                        
                        if (!found) {
                            snprintf(response, BUFFER_SIZE - 1, "ERROR: No se encontro registro con ID %d.\n", id_buscado);
                            response[BUFFER_SIZE - 1] = '\0';
                        }
                        
                        fclose(csv_file);
                    }
                } else {
                    strcpy(response, "ERROR: Transaccion activa. Reintente luego (Consulta bloqueada).\n");
                }
            }
        }
        // 4. MODIFICACIÓN (DML)
        else if (strncmp(buffer, "UPDATE", 6) == 0 || strncmp(buffer, "INSERT", 6) == 0 || strncmp(buffer, "DELETE", 6) == 0) {
            if (transaccion_activa) {
                if (strncmp(buffer, "INSERT", 6) == 0) {
                    // INSERT ID_PROCESO TIMESTAMP DATO_ALEATORIO
                    char *tokens[4];
                    char *token = strtok(buffer, " ");
                    int i = 0;
                    while (token != NULL && i < 4) {
                        tokens[i++] = token;
                        token = strtok(NULL, " ");
                    }
                    
                    if (i < 4) {
                        strcpy(response, "ERROR: Formato incorrecto. Use: INSERT <ID_PROCESO> <TIMESTAMP> <DATO_ALEATORIO>\n");
                    } else {
                        // Encontrar el próximo ID disponible
                        FILE *csv_file = fopen(config_csv_file, "r");
                        int max_id = -1;
                        char line[BUFFER_SIZE];
                        
                        if (csv_file) {
                            fgets(line, sizeof(line), csv_file); // Skip header
                            while (fgets(line, sizeof(line), csv_file)) {
                                int current_id;
                                if (sscanf(line, "%d,", &current_id) == 1 && current_id > max_id) {
                                    max_id = current_id;
                                }
                            }
                            fclose(csv_file);
                        }
                        
                        // Añadir nuevo registro
                        csv_file = fopen(config_csv_file, "a");
                        if (csv_file) {
                            fprintf(csv_file, "%d,%s,%s,%s\n", max_id + 1, tokens[1], tokens[2], tokens[3]);
                            fclose(csv_file);
                            snprintf(response, BUFFER_SIZE - 1, "OK: Registro insertado con ID %d. Pendiente de COMMIT.\n", max_id + 1);
                            response[BUFFER_SIZE - 1] = '\0';
                        } else {
                            strcpy(response, "ERROR: No se pudo escribir en el archivo CSV.\n");
                        }
                    }
                }
                else if (strncmp(buffer, "DELETE", 6) == 0) {
                    // DELETE ID
                    char *token = strtok(buffer, " ");
                    token = strtok(NULL, " ");
                    
                    if (token == NULL) {
                        strcpy(response, "ERROR: Formato incorrecto. Use: DELETE <ID>\n");
                    } else {
                        int id_eliminar = atoi(token);
                        
                        // Crear archivo temporal
                        FILE *csv_file = fopen(config_csv_file, "r");
                        FILE *temp_file = fopen("temp.csv", "w");
                        
                        if (csv_file && temp_file) {
                            char line[BUFFER_SIZE];
                            int found = 0;
                            
                            // Copiar encabezado
                            if (fgets(line, sizeof(line), csv_file)) {
                                fputs(line, temp_file);
                            }
                            
                            // Copiar todas las líneas excepto la que tiene el ID a eliminar
                            while (fgets(line, sizeof(line), csv_file)) {
                                int current_id;
                                if (sscanf(line, "%d,", &current_id) == 1 && current_id == id_eliminar) {
                                    found = 1;
                                    continue; // No copiar esta línea
                                }
                                fputs(line, temp_file);
                            }
                            
                            fclose(csv_file);
                            fclose(temp_file);
                            
                            if (found) {
                                rename("temp.csv", config_csv_file);
                                snprintf(response, BUFFER_SIZE - 1, "OK: Registro con ID %d eliminado. Pendiente de COMMIT.\n", id_eliminar);
                                response[BUFFER_SIZE - 1] = '\0';
                            } else {
                                remove("temp.csv");
                                snprintf(response, BUFFER_SIZE - 1, "ERROR: No se encontro registro con ID %d.\n", id_eliminar);
                                response[BUFFER_SIZE - 1] = '\0';
                            }
                        } else {
                            strcpy(response, "ERROR: No se pudo acceder al archivo CSV.\n");
                            if (csv_file) fclose(csv_file);
                            if (temp_file) fclose(temp_file);
                        }
                    }
                }
                else if (strncmp(buffer, "UPDATE", 6) == 0) {
                    // UPDATE ID ID_PROCESO TIMESTAMP DATO_ALEATORIO
                    char *tokens[5];
                    char *token = strtok(buffer, " ");
                    int i = 0;
                    while (token != NULL && i < 5) {
                        tokens[i++] = token;
                        token = strtok(NULL, " ");
                    }
                    
                    if (i < 5) {
                        strcpy(response, "ERROR: Formato incorrecto. Use: UPDATE <ID> <ID_PROCESO> <TIMESTAMP> <DATO_ALEATORIO>\n");
                    } else {
                        int id_actualizar = atoi(tokens[1]);
                        
                        // Crear archivo temporal
                        FILE *csv_file = fopen(config_csv_file, "r");
                        FILE *temp_file = fopen("temp.csv", "w");
                        
                        if (csv_file && temp_file) {
                            char line[BUFFER_SIZE];
                            int found = 0;
                            
                            // Copiar encabezado
                            if (fgets(line, sizeof(line), csv_file)) {
                                fputs(line, temp_file);
                            }
                            
                            // Procesar cada línea
                            while (fgets(line, sizeof(line), csv_file)) {
                                int current_id;
                                if (sscanf(line, "%d,", &current_id) == 1 && current_id == id_actualizar) {
                                    // Reemplazar con los nuevos datos
                                    fprintf(temp_file, "%d,%s,%s,%s\n", id_actualizar, tokens[2], tokens[3], tokens[4]);
                                    found = 1;
                                } else {
                                    fputs(line, temp_file);
                                }
                            }
                            
                            fclose(csv_file);
                            fclose(temp_file);
                            
                            if (found) {
                                rename("temp.csv", config_csv_file);
                                snprintf(response, BUFFER_SIZE - 1, "OK: Registro con ID %d actualizado. Pendiente de COMMIT.\n", id_actualizar);
                                response[BUFFER_SIZE - 1] = '\0';
                            } else {
                                remove("temp.csv");
                                snprintf(response, BUFFER_SIZE - 1, "ERROR: No se encontro registro con ID %d.\n", id_actualizar);
                                response[BUFFER_SIZE - 1] = '\0';
                            }
                        } else {
                            strcpy(response, "ERROR: No se pudo acceder al archivo CSV.\n");
                            if (csv_file) fclose(csv_file);
                            if (temp_file) fclose(temp_file);
                        }
                    }
                }
            } else {
                strcpy(response, "ERROR: La modificacion requiere iniciar una transaccion (BEGIN TRANSACTION).\n");
            }
        }
        // 5. SALIR
        else if (strncmp(buffer, "EXIT", 4) == 0) {
            if (transaccion_activa) {
                liberar_bloqueo(csv_fd); // Liberar antes de salir
                log_message("Cliente salio con transaccion activa. ROLLBACK implicito.");
            }
            break; // Salir del bucle while
        }
        // 6. COMANDO NO RECONOCIDO
        else {
            strcpy(response, "ERROR: Comando no reconocido o protocolo invalido.\n");
        }
        
        // Enviar respuesta al cliente
        send(client_socket, response, strlen(response), 0);
    }
    
    // Cierre y limpieza
    log_message("Cliente desconectado. Finalizando proceso hijo.");
    printf("Cliente desconectado. Clientes activos restantes: %d\n", clientes_activos);
    
    // Liberar bloqueo si estaba activo
    if (transaccion_activa) {
        liberar_bloqueo(csv_fd);
        log_message("Bloqueo liberado por desconexion de cliente.");
    }
    
    // Cerrar descriptores de archivo
    if (csv_fd >= 0) {
        close(csv_fd);
    }
    if (client_socket >= 0) {
        close(client_socket);
    }
    
    // Limpiar archivos temporales que pudiera haber dejado este proceso
    limpiar_archivos_temporales();
    
    log_message("Proceso hijo terminado correctamente.");
    exit(EXIT_SUCCESS); // ¡MUY IMPORTANTE! El hijo debe terminar
}

// --- Función Principal del Servidor ---

int main(int argc, char *argv[]) {
    int server_fd, new_socket;
    struct sockaddr_in address;
    int addrlen = sizeof(address);
    struct sigaction sa_chld, sa_term;

    if (argc != 3) {
        fprintf(stderr, "Uso: %s <clientes_concurrentes> <clientes_en_espera>\n", argv[0]);
        fprintf(stderr, "Ejemplo: %s 10 5\n", argv[0]);
        return EXIT_FAILURE;
    }
    
    // Leer configuración del archivo
    leer_configuracion();
    
    // Asignar N y M
    max_clientes_concurrentes = atoi(argv[1]);
    max_clientes_espera = atoi(argv[2]);
    if (max_clientes_concurrentes <= 0 || max_clientes_espera <= 0) {
        fprintf(stderr, "Error: los argumentos deben ser números enteros positivos.\n");
        return EXIT_FAILURE;
    }
    
    printf("Configuracion del servidor:\n");
    printf("  Clientes concurrentes maximos: %d\n", max_clientes_concurrentes);
    printf("  Clientes en cola de espera maximos: %d\n", max_clientes_espera);

    // 1. Manejo de señales para hijos (SIGCHLD)
    sa_chld.sa_handler = sigchld_handler; 
    sigemptyset(&sa_chld.sa_mask);
    sa_chld.sa_flags = SA_RESTART;
    if (sigaction(SIGCHLD, &sa_chld, NULL) == -1) {
        perror("sigaction SIGCHLD");
        exit(EXIT_FAILURE);
    }
    //Configurar el Manejador de Señales de Terminación (Para limpieza)
    sa_term.sa_handler = cleanup_handler;
    sigemptyset(&sa_term.sa_mask);
    sa_term.sa_flags = 0;
    if (sigaction(SIGINT, &sa_term, NULL) == -1 || // Ctrl+C
        sigaction(SIGTERM, &sa_term, NULL) == -1) { // Señal de terminación
        perror("sigaction SIGINT/SIGTERM");
        exit(EXIT_FAILURE);
    }

    // 2. Creación del socket
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }
    
    // Asignar a variable global para cleanup
    server_fd_global = server_fd;

    // 3. Configuración del socket (reuso de dirección/puerto)
    int opt = 1;
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt))) {
        perror("setsockopt");
        exit(EXIT_FAILURE);
    }

    address.sin_family = AF_INET;
    
    // Configurar la dirección IP
    if (strcmp(config_host, "0.0.0.0") == 0) {
        address.sin_addr.s_addr = INADDR_ANY; // Escucha en todas las interfaces
    } else {
        if (inet_pton(AF_INET, config_host, &address.sin_addr) <= 0) {
            fprintf(stderr, "Error: Direccion IP invalida en configuracion: %s\n", config_host);
            return EXIT_FAILURE;
        }
    }
    
    address.sin_port = htons(config_port);

    // 4. Binding
    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("bind failed");
        exit(EXIT_FAILURE);
    }
    
    printf("Servidor iniciado. Escuchando en %s:%d...\n", config_host, config_port);
    log_message("Servidor iniciado.");

    // 5. Escucha (Listen) - Cola del sistema operativo (diferente a nuestra cola de aplicación)
    if (listen(server_fd, max_clientes_espera + max_clientes_concurrentes) < 0) {
        perror("listen");
        exit(EXIT_FAILURE);
    }

    // 6. Bucle de aceptación de clientes
    while (1) {
        printf("Esperando nueva conexión...\n");

        if ((new_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t *)&addrlen)) < 0) {
            perror("accept");
            continue;
        }

        // Validación de N clientes concurrentes
        if (clientes_activos >= max_clientes_concurrentes) {
            // Intentar agregar a cola de espera
            int posicion = agregar_a_cola_espera(new_socket);
            if (posicion == -1) {
                // Cola de espera llena
                const char *err_msg = "ERROR: Limite de clientes concurrentes y cola.\n";
                send(new_socket, err_msg, strlen(err_msg), 0);
                close(new_socket);
                log_message("Conexion rechazada: Limite de clientes y cola alcanzados.");
            } else {
                // Cliente agregado a cola de espera
                char wait_msg[256];
                snprintf(wait_msg, sizeof(wait_msg), 
                        "EN_COLA_ESPERA: Posicion %d de %d. Esperando que se libere un slot...\n", 
                        posicion, max_clientes_espera);
                send(new_socket, wait_msg, strlen(wait_msg), 0);
                printf("Cliente agregado a cola de espera. Posicion: %d/%d\n", posicion, max_clientes_espera);
                // No cerrar el socket, se mantiene en la cola
            }
            continue;
        } else {
            // Hay espacio disponible
            const char *accp_msg = "CONEXION_ESTABLECIDA: Puede proceder con sus comandos.\n";
            send(new_socket, accp_msg, strlen(accp_msg), 0);
        }

        // Manejar el cliente usando un nuevo proceso (fork)
        pid_t pid = fork();
        if (pid == 0) { 
            // Proceso Hijo (Maneja el Cliente)
            close(server_fd); // El hijo no necesita el socket de escucha
            manejar_cliente(new_socket);
            // El proceso hijo termina dentro de manejar_cliente o aquí si falla
            exit(EXIT_SUCCESS); 
        } else if (pid > 0) { 
            // Proceso Padre (Servidor Principal)
            close(new_socket); // El padre no necesita el socket del cliente
            clientes_activos++;
            printf("Nuevo cliente aceptado (PID %d). Clientes activos: %d\n", pid, clientes_activos);
        } else {
            perror("fork failed");
            // Manejo de error de fork (cerrar new_socket y continuar)
            close(new_socket);
        }
    }
    
    // 7. Limpieza final (Este código es difícil de alcanzar, pero es buena práctica)
    close(server_fd);
    log_message("Servidor finalizado.");
    return EXIT_SUCCESS;
}