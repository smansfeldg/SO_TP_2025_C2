# Trabajo Práctico - Sistemas Operativos

Este repositorio contiene la implementación de dos ejercicios de sistemas operativos que demuestran diferentes técnicas de comunicación entre procesos y gestión de recursos concurrentes.

## Estructura del Proyecto

### Ejercicio 1: Sistema Generador de Datos Concurrente
- **Ubicación**: `Ejercicio_1/`
- **Tecnología**: Memoria compartida System V + Semáforos
- **Descripción**: Sistema distribuido con proceso coordinador que gestiona múltiples generadores de datos

### Ejercicio 2: Servidor TCP con Transacciones
- **Ubicación**: `Ejercicio_2/`
- **Tecnología**: Sockets TCP + Bloqueo de archivos
- **Descripción**: Servidor concurrente que maneja múltiples clientes con soporte para transacciones

## Compilación Rápida

```bash
# Ejercicio 1
cd Ejercicio_1 && make all

# Ejercicio 2  
cd Ejercicio_2 && make all
```

## Ejecución Rápida

```bash
# Ejercicio 1: Generar 100 registros con 3 procesos
cd Ejercicio_1 && ./coordinador 100 3

# Ejercicio 2: Servidor con 5 clientes concurrentes, cola de 10
cd Ejercicio_2 && ./servidor 5 10
# En otra terminal: ./cliente
```

## Monitoreo y Análisis del Sistema

### Monitoreo de Recursos IPC (Ejercicio 1)

#### Estado de memoria compartida
```bash
# Listar memoria compartida activa
ipcs -m

# Buscar recursos del proyecto (clave 0x1234)
ipcs -m | grep 1234

# Ver detalles específicos
ipcs -m -i <shmid>
```

#### Estado de semáforos  
```bash
# Listar semáforos activos
ipcs -s

# Buscar semáforos del proyecto (clave 0x5678)
ipcs -s | grep 5678

# Ver estado completo de IPC
ipcs -a
```

### Monitoreo de Conexiones de Red (Ejercicio 2)

#### Estado del servidor TCP
```bash
# Ver puertos en escucha
netstat -tlnp | grep :8080

# Conexiones TCP activas
netstat -tnp | grep :8080

# Con ss (alternativa moderna)
ss -tlnp | grep :8080
ss -tnp | grep :8080
```

#### Archivos abiertos por el servidor
```bash
# Archivos abiertos por el proceso servidor
lsof -p $(pgrep servidor)

# Conexiones de red específicas
lsof -i :8080

# Bloqueos de archivos activos
lsof | grep FLOCK
```

### Monitoreo de Procesos (Ambos Ejercicios)

#### Procesos del sistema en tiempo real
```bash
# Ejercicio 1: Monitorear coordinador y generadores
watch -n 2 'ps aux | grep -E "(coordinador|generador)" | grep -v grep'

# Ejercicio 2: Monitorear servidor y clientes
watch -n 2 'ps aux | grep -E "(servidor|cliente)" | grep -v grep'

# Árbol de procesos
pstree -p $(pgrep coordinador)  # Ejercicio 1
pstree -p $(pgrep servidor)     # Ejercicio 2
```

#### Uso de recursos por proceso
```bash
# Información detallada de memoria y CPU
ps -o pid,ppid,cmd,%mem,%cpu -p $(pgrep coordinador)
ps -o pid,ppid,cmd,%mem,%cpu -p $(pgrep servidor)

# Top específico por proceso
top -p $(pgrep coordinador)
top -p $(pgrep servidor)
```

### Monitoreo del Sistema Durante Ejecución

#### Recursos del sistema
```bash
# Memoria total del sistema
free -h

# Uso de CPU en tiempo real
top -n 1 | grep "Cpu(s)"

# Carga del sistema
uptime

# Espacio en disco
df -h .
```

#### Actividad de I/O
```bash
# Monitor de I/O del sistema
iostat 1 5

# Procesos con mayor uso de I/O
iotop

# Actividad de red
iftop           # Tráfico de red por conexión
nethogs         # Uso de red por proceso
```

## Análisis y Debugging

### Rastreo de Llamadas del Sistema

#### Ejercicio 1 (IPC)
```bash
# Rastrear llamadas IPC del coordinador
strace -e trace=shmget,shmat,shmdt,shmctl,semget,semop,semctl ./coordinador 50 2

# Rastrear un generador específico
strace -p $(pgrep generador | head -1)

# Todas las llamadas con timestamps
strace -t -o trace_coordinador.log ./coordinador 50 2
```

#### Ejercicio 2 (Red y archivos)
```bash
# Rastrear llamadas de red y archivos del servidor
strace -e trace=socket,bind,listen,accept,read,write,open,close ./servidor 3 5

# Rastrear un cliente específico
strace -e trace=socket,connect,read,write ./cliente

# Log completo con timestamps
strace -t -o trace_servidor.log ./servidor 3 5
```

### Análisis de Memoria

#### Detección de memory leaks
```bash
# Ejercicio 1
valgrind --tool=memcheck --leak-check=full ./coordinador 50 2

# Ejercicio 2  
valgrind --tool=memcheck --leak-check=full ./servidor 3 5
```

#### Información de memoria por proceso
```bash
# Estado de memoria detallado
cat /proc/$(pgrep coordinador)/status | grep -E "(VmSize|VmRSS|VmData)"
cat /proc/$(pgrep servidor)/status | grep -E "(VmSize|VmRSS|VmData)"

# Mapas de memoria
cat /proc/$(pgrep coordinador)/maps
cat /proc/$(pgrep servidor)/maps
```

## Resolución de Problemas

### Limpieza de Recursos

#### Ejercicio 1: Recursos IPC
```bash
# Limpiar automáticamente
cd Ejercicio_1 && make ipc-clean

# Limpiar manualmente por clave
ipcrm -M 0x1234  # Memoria compartida
ipcrm -S 0x5678  # Semáforos

# Limpiar todos los recursos del usuario
ipcs -m | grep $(whoami) | awk '{print $2}' | xargs -r ipcrm -m
ipcs -s | grep $(whoami) | awk '{print $2}' | xargs -r ipcrm -s
```

#### Ejercicio 2: Conexiones y archivos
```bash
# Cerrar conexiones TCP del puerto 8080
ss -K dst :8080

# Verificar archivos bloqueados
lsof | grep FLOCK | grep servidor

# Limpiar archivos temporales
cd Ejercicio_2 && make clean-all
```

### Terminación de Procesos

#### Terminación normal
```bash
# Ejercicio 1
pkill coordinador
pkill generador

# Ejercicio 2
pkill servidor
pkill cliente
```

#### Terminación forzada (si es necesario)
```bash
# Ejercicio 1
pkill -9 coordinador
pkill -9 generador

# Ejercicio 2
pkill -9 servidor
pkill -9 cliente
```

## Pruebas de Rendimiento

### Medición de tiempo de ejecución
```bash
# Ejercicio 1: Tiempo de generación de datos
time ./coordinador 1000 4

# Ejercicio 2: Tiempo de respuesta del servidor
time echo "SELECT 1" | ./cliente

# Múltiples ejecuciones para promedios
for i in {1..5}; do 
    echo "Ejecución $i:"
    time ./coordinador 500 3
    echo "---"
done
```

### Pruebas de estrés
```bash
# Ejercicio 1: Muchos registros y generadores
./coordinador 10000 8
./coordinador 1000 20

# Ejercicio 2: Múltiples clientes concurrentes
for i in {1..10}; do ./cliente & done
```

### Monitoreo durante pruebas de carga
```bash
# Monitor completo durante ejecución
# Terminal 1: Ejecutar test
./coordinador 5000 10

# Terminal 2: Monitorear recursos
watch -n 1 'echo "=== CPU ==="; top -bn1 | grep "Cpu(s)"; echo "=== Memoria ==="; free -h; echo "=== Procesos ==="; ps aux | grep -E "(coordinador|generador)" | grep -v grep | wc -l'
```
