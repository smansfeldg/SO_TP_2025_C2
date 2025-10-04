#include "servidor.h"

int clientes_activos = 0;

void log_message(const char *message) {
    FILE *log_fp = fopen(LOG_FILE, "a");
    if (log_fp) {
        fprintf(log_fp, "[LOG] %s\n", message);
        fclose(log_fp);
    }
}

void sigchld_handler(int s) {
    // waitpid() puede devolver -1 si no hay hijos, por eso el bucle
    while (waitpid(-1, NULL, WNOHANG) > 0) {
        clientes_activos--;
    }
}

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

void liberar_bloqueo(int fd) {
    flock(fd, LOCK_UN); // LOCK_UN: Desbloqueo
    log_message("Bloqueo liberado.");
}

void manejar_cliente(int client_socket) {
    char buffer[BUFFER_SIZE];
    int bytes_read;
    char response[BUFFER_SIZE];

    // Abrir el archivo CSV. Mantener el descriptor abierto para usar flock.
    int csv_fd = open(CSV_FILE, O_RDWR | O_CREAT, 0666);
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
        else if (strncmp(buffer, "SELECT", 6) == 0 || strncmp(buffer, "CONSULTA", 8) == 0) {
            // **IMPORTANTE:** Las consultas no requieren bloqueo exclusivo,
            // pero si hay una transacción activa (flock tomado), fallarán
            // si intentamos obtener un bloqueo compartido (LOCK_SH).

            // Opción simple: Permitir consultas SELECT incluso con bloqueo EXCLUSIVO.
            // Esto implica "Lecturas Sucias" (Dirty Reads), pero simplifica la lógica.
            // Si el requisito es NO permitir ninguna operación:
            if (flock(csv_fd, LOCK_EX | LOCK_NB) == 0) { // Si podemos tomar EXCLUSIVO, está libre
                 liberar_bloqueo(csv_fd); // Lo liberamos inmediatamente
                 strcpy(response, "OK: Consulta realizada (Lectura).\n");
                 // Implementación real: Abrir CSV en modo lectura y enviar datos al cliente
            } else {
                strcpy(response, "ERROR: Transaccion activa. Reintente luego (Consulta bloqueada).\n");
            }
        }
        // 4. MODIFICACIÓN (DML)
        else if (strncmp(buffer, "UPDATE", 6) == 0 || strncmp(buffer, "ALTA", 4) == 0) {
            if (transaccion_activa) {
                // Implementación real: Modificar el archivo CSV (solo si transaccion_activa = 1)
                strcpy(response, "OK: Modificacion aplicada. Pendiente de COMMIT.\n");
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
    close(csv_fd);
    close(client_socket);
    exit(EXIT_SUCCESS); // ¡MUY IMPORTANTE! El hijo debe terminar
}

int main(int argc, char *argv[]) {
    int server_fd, new_socket;
    struct sockaddr_in address;
    int addrlen = sizeof(address);
    struct sigaction sa;

    // 1. Manejo de señales para hijos (SIGCHLD)
    sa.sa_handler = sigchld_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    if (sigaction(SIGCHLD, &sa, NULL) == -1) {
        perror("sigaction");
        exit(EXIT_FAILURE);
    }

    // 2. Creación del socket
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }

    // 3. Configuración del socket (reuso de dirección/puerto)
    int opt = 1;
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt))) {
        perror("setsockopt");
        exit(EXIT_FAILURE);
    }

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY; // Escucha en todas las interfaces (0.0.0.0)
    address.sin_port = htons(PORT);

    // 4. Binding
    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("bind failed");
        exit(EXIT_FAILURE);
    }

    // 5. Escucha (Listen) - M clientes en espera (BACKLOG)
    if (listen(server_fd, BACKLOG) < 0) {
        perror("listen");
        exit(EXIT_FAILURE);
    }

    printf("Servidor iniciado. Escuchando en el puerto %d...\n", PORT);
    log_message("Servidor iniciado.");

    // 6. Bucle de aceptación de clientes
    while (1) {
        printf("Esperando nueva conexión...\n");

        if ((new_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t *)&addrlen)) < 0) {
            perror("accept");
            continue;
        }

        // Validación de N clientes concurrentes
        if (clientes_activos >= MAX_CLIENTES) {
             // Devolver un error al cliente y continuar (o cerrar socket)
             const char *err_msg = "ERROR: Limite de clientes concurrentes alcanzado. Reintente luego.\n";
             send(new_socket, err_msg, strlen(err_msg), 0);
             close(new_socket);
             log_message("Conexion rechazada: Limite de clientes alcanzado.");
             continue;
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
