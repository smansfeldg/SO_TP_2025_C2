#ifndef SOCKET_H_INCLUDED
#define SOCKET_H_INCLUDED

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

#define PORT 8080
#define BACKLOG 5       // M clientes en espera (5 en este ejemplo)
#define MAX_CLIENTES 10 // N clientes concurrentes (solo controla fork, no es estricto)
#define BUFFER_SIZE 1024
#define CSV_FILE "datos.csv"
#define LOG_FILE "server.log"

#endif // SOCKET_H_INCLUDED
