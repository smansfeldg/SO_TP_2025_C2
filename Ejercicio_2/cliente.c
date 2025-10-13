#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h> 
#include <arpa/inet.h>

#define SERVER_IP "127.0.0.1"
#define PORT 8080
#define BUFFER_SIZE 1024

void error(const char *msg) {
    perror(msg);
    exit(EXIT_FAILURE);
}

void mostrar_ayuda() {
    fprintf(stderr, "\nComandos disponibles:\n");
    fprintf(stderr, "  BEGIN TRANSACTION          - Iniciar transaccion\n");
    fprintf(stderr, "  COMMIT TRANSACTION         - Confirmar transaccion (aplicar cambios)\n");
    fprintf(stderr, "  SELECT <ID>                - Consultar registro por ID\n");
    fprintf(stderr, "  INSERT <ID_PROC> <TIME> <DATO> - Preparar insercion\n");
    fprintf(stderr, "  UPDATE <ID> <ID_PROC> <TIME> <DATO> - Preparar actualizacion\n");
    fprintf(stderr, "  DELETE <ID>                - Preparar eliminacion\n");
    fprintf(stderr, "  EXIT                       - Salir del cliente\n");
    fprintf(stderr, "\nNota: INSERT, UPDATE y DELETE requieren transaccion activa.\n");
    fprintf(stderr, "Los cambios NO se aplican hasta hacer COMMIT TRANSACTION.\n");
}

int main(int argc, char *argv[]) {
    int sock = 0;
    struct sockaddr_in serv_addr;
    char buffer[BUFFER_SIZE] = {0};
    char command[BUFFER_SIZE];
    char *server_ip = SERVER_IP;
    int server_port = PORT;

    // 1. Validación de parámetros (simple)
    if (argc > 3) {
        mostrar_ayuda();
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
        printf("Uso: ./cliente [IP_servidor] [Puerto]\n");
        printf("Ejemplo: ./cliente 127.0.0.1 8080\n");
        close(sock);
        return EXIT_FAILURE;
    }

    // 3. Conexión al servidor
    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        perror("Connection Failed");
        printf("Uso: ./cliente [IP_servidor] [Puerto]\n");
        printf("Ejemplo: ./cliente 127.0.0.1 8080\n");
        close(sock);
        return EXIT_FAILURE;
    }

    printf("Conectando al servidor %s:%d. Escriba 'EXIT' para salir.\n", server_ip, server_port);


    // Recibir y mostrar respuesta
    int bytes_read = recv(sock, buffer, BUFFER_SIZE - 1, 0);
    if (bytes_read <= 0) {
        printf("Error o desconexion del servidor.\n");
        return EXIT_FAILURE;
    }
    buffer[bytes_read] = '\0';
    printf("Servidor: %s", buffer);
        
    // Variable para controlar el estado del cliente
    int en_cola_espera = 0;
    
    // Verificar mensajes especiales del servidor al conectar
    if (strncmp(buffer, "ERROR: Limite de clientes concurrentes y cola.\n", 49) == 0) {
        printf("Servidor lleno (clientes activos y cola de espera). Cerrando cliente...\n");
        close(sock);
        return EXIT_FAILURE;
    } else if (strncmp(buffer, "EN_COLA_ESPERA:", 15) == 0) {
        printf("Servidor: %s", buffer);
        printf("Esperando en cola... NO puede enviar comandos hasta ser aprobado.\n");
        printf("Presione Ctrl+C para cancelar la espera.\n");
        en_cola_espera = 1;
    } else if (strncmp(buffer, "CONEXION_ESTABLECIDA:", 21) == 0) {
        printf("Servidor: %s", buffer);
        mostrar_ayuda();
        en_cola_espera = 0;
    }

    // 4. Bucle interactivo
    while (1) {
        
        // Si está en cola de espera, solo escuchar mensajes del servidor
        if (en_cola_espera) {
            printf("Esperando en cola... (Ctrl+C para cancelar)\n");
            
            // Solo recibir mensajes del servidor, no pedir input del usuario
            int bytes_read = recv(sock, buffer, BUFFER_SIZE - 1, 0);
            if (bytes_read <= 0) {
                printf("Error o desconexion del servidor.\n");
                break;
            }
            buffer[bytes_read] = '\0';
            
            // Verificar mensajes especiales del servidor
            if (strncmp(buffer, "POSICION_ACTUALIZADA:", 21) == 0) {
                printf("Servidor: %s", buffer);
                continue; // Seguir esperando
            } else if (strncmp(buffer, "CONEXION_APROBADA:", 18) == 0) {
                printf("Servidor: %s", buffer);
                printf("¡Conexion aprobada! Ya puede enviar comandos normalmente.\n");
                en_cola_espera = 0; // Salir del modo de espera
                continue;
            } else if (strncmp(buffer, "SERVIDOR_CERRANDO:", 18) == 0) {
                printf("Servidor: %s", buffer);
                break;
            } else {
                printf("Servidor: %s", buffer);
                continue;
            }
        }
        // Modo normal: pedir input del usuario
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
        
        // Verificar si el servidor nos puso en cola de espera
        if (strncmp(buffer, "EN_COLA_ESPERA:", 15) == 0) {
            printf("Servidor: %s", buffer);
            printf("Ahora está en cola de espera. NO puede enviar más comandos.\n");
            en_cola_espera = 1;
            continue;
        } else {
            printf("Servidor: %s", buffer);
        }
    }
    
    printf("Desconectando...\n");
    close(sock);
    return EXIT_SUCCESS;
}