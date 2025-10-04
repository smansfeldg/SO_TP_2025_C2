#include "cliente.h"

void mostrar_ayuda(const char *progname) {
    fprintf(stderr, "Uso: %s [IP_servidor] [Puerto]\n", progname);
    fprintf(stderr, "Ejemplo: %s 127.0.0.1 8080\n", progname);
    fprintf(stderr, "Comandos disponibles: BEGIN TRANSACTION, COMMIT TRANSACTION, SELECT, UPDATE/ALTA, EXIT\n");
}

int main(int argc, char *argv[]) {
    int sock = 0;
    struct sockaddr_in serv_addr;
    char buffer[BUFFER_SIZE] = {0};
    char *server_ip = SERVER_IP;
    int server_port = PORT;

    // 1. Validación de parámetros (simple)
    if (argc > 3) {
        mostrar_ayuda(argv[0]);
        return EXIT_FAILURE;
    }
    if (argc >= 2) server_ip = argv[1];
    if (argc == 3) server_port = atoi(argv[2]);

    // 2. Creación del socket
    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("Socket creation error");
        return EXIT_FAILURE;
    }

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(server_port);

    // Convertir IP de texto a binario
    if (inet_pton(AF_INET, server_ip, &serv_addr.sin_addr) <= 0) {
        perror("Invalid address/ Address not supported");
        close(sock);
        return EXIT_FAILURE;
    }

    // 3. Conexión al servidor
    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        perror("Connection Failed");
        close(sock);
        return EXIT_FAILURE;
    }

    printf("Conectado al servidor %s:%d. Escriba 'EXIT' para salir.\n", server_ip, server_port);
    mostrar_ayuda(argv[0]);

    // 4. Bucle interactivo
    while (1) {
        char command[BUFFER_SIZE];
        printf("> ");

        if (fgets(command, BUFFER_SIZE, stdin) == NULL) {
            break; // EOF (Ctrl+D)
        }

        // Eliminar el salto de línea
        command[strcspn(command, "\n")] = 0;

        if (strlen(command) == 0) continue;

        // 5. Enviar comando
        send(sock, command, strlen(command), 0);

        // 6. Si es EXIT, salir inmediatamente
        if (strcmp(command, "EXIT") == 0) {
            break;
        }

        // 7. Recibir y mostrar respuesta
        int bytes_read = recv(sock, buffer, BUFFER_SIZE - 1, 0);
        if (bytes_read <= 0) {
            printf("Error o desconexion del servidor.\n");
            break;
        }
        buffer[bytes_read] = '\0';
        printf("Servidor: %s", buffer);
    }

    printf("Desconectando...\n");
    close(sock);
    return EXIT_SUCCESS;
}
