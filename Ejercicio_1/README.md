# Sistema Generador de Datos Concurrente

## 📋 Descripción

Sistema de generación de datos concurrente que utiliza memoria compartida y semáforos para coordinar múltiples procesos generadores. El coordinador gestiona la asignación de IDs y recopila los datos generados en un archivo CSV.

## 🏗️ Arquitectura

- **Coordinador**: Proceso principal que gestiona memoria compartida, semáforos y procesos hijos
- **Generador**: Procesos hijos que solicitan IDs, generan datos aleatorios y los envían al coordinador
- **Memoria Compartida**: Comunicación entre coordinador y generadores (clave: `0x1234`)
- **Semáforos**: Sincronización de acceso a recursos compartidos (clave: `0x5678`)

## 🚀 Compilación

### Compilar todo el proyecto
```bash
make all
```

### Compilar componentes individuales
```bash
make coordinador    # Solo el coordinador
make generador      # Solo el generador
make clean          # Limpiar archivos compilados
```

### Ver ayuda del Makefile
```bash
make help
```

## 🧪 Ejecución y Pruebas

### Ejecución Manual
```bash
# Sintaxis: ./coordinador <total_registros> <num_generadores>
./coordinador 100 3     # 100 registros con 3 generadores
./coordinador 1000 5    # 1000 registros con 5 generadores
./coordinador 50 2      # 50 registros con 2 generadores (prueba rápida)
```

### Pruebas Automatizadas
```bash
# Prueba básica (100 registros, 3 generadores)
make test

# Prueba completa (1000 registros, 5 generadores)  
make test-full

# Script completo de pruebas
chmod +x run_test.sh
./run_test.sh
```

### Verificación de Resultados
```bash
# Verificar integridad del CSV
make verify

# O usar directamente el script AWK
chmod +x verify.awk
./verify.awk datos_generados.csv
```

## 📊 Análisis y Monitoreo

### Monitoreo de Recursos IPC

#### Ver estado de memoria compartida
```bash
# Listar toda la memoria compartida del sistema
ipcs -m

# Ver detalles específicos
ipcs -m -i <shmid>

# Buscar por clave específica
ipcs -m | grep 1234
```

#### Ver estado de semáforos
```bash
# Listar todos los semáforos
ipcs -s

# Ver detalles específicos
ipcs -s -i <semid>

# Buscar por clave específica
ipcs -s | grep 5678
```

#### Estado completo de recursos IPC
```bash
# Ver todo junto
ipcs -a

# Con información adicional
ipcs -u    # Resumen de uso
ipcs -l    # Límites del sistema
ipcs -t    # Timestamps
```

### Monitoreo de Procesos

#### Monitorear procesos en tiempo real
```bash
# Ver procesos relacionados
ps aux | grep -E "(coordinador|generador)" | grep -v grep

# Monitoreo continuo (actualización cada 2 segundos)
watch -n 2 'ps aux | grep -E "(coordinador|generador)" | grep -v grep'

# Con información detallada de recursos
top -p $(pgrep coordinador)

# Monitorear árbol de procesos
pstree -p $(pgrep coordinador)
```

#### Información detallada de procesos
```bash
# PIDs de todos los procesos relacionados
pgrep coordinador
pgrep generador

# Información completa de un proceso específico
ps -f -p $(pgrep coordinador)

# Uso de memoria de los procesos
ps -o pid,ppid,cmd,%mem,%cpu -p $(pgrep coordinador)
```

### Análisis del Archivo CSV

#### Estadísticas básicas
```bash
# Contar líneas totales
wc -l datos_generados.csv

# Ver cabecera
head -1 datos_generados.csv

# Primeros y últimos registros
head -10 datos_generados.csv    # Primeros 10
tail -10 datos_generados.csv    # Últimos 10

# Tamaño del archivo
ls -lh datos_generados.csv
du -h datos_generados.csv
```

#### Análisis avanzado con AWK
```bash
# Contar registros por proceso generador
awk -F, 'NR>1 {count[$2]++} END {for(pid in count) print "PID " pid ": " count[pid] " registros"}' datos_generados.csv

# Verificar secuencia de IDs
awk -F, 'NR>1 {print $1}' datos_generados.csv | sort -n | head -5  # Primeros IDs
awk -F, 'NR>1 {print $1}' datos_generados.csv | sort -n | tail -5  # Últimos IDs

# Timestamps únicos (detectar concurrencia)
awk -F, 'NR>1 {timestamps[$3]++} END {print "Timestamps únicos: " length(timestamps)}' datos_generados.csv

# Detectar IDs duplicados
awk -F, 'NR>1 {if(seen[$1]++) print "ID duplicado: " $1}' datos_generados.csv

# Estadísticas de longitud de datos aleatorios
awk -F, 'NR>1 {sum+=length($4); count++} END {print "Longitud promedio datos: " sum/count}' datos_generados.csv
```

### Monitoreo del Sistema Durante Ejecución

#### Uso de recursos del sistema
```bash
# Memoria total del sistema
free -h

# Uso de CPU
top -n 1 | grep "Cpu(s)"

# Cargar información del sistema
uptime

# Espacio en disco
df -h .
```

#### Monitoreo en tiempo real con herramientas avanzadas
```bash
# htop (si está disponible) - más visual que top
htop

# Monitorear I/O del sistema
iostat 1

# Actividad de red (si aplicable)
netstat -i

# Procesos con más uso de CPU/memoria
ps aux --sort=-%cpu | head -10
ps aux --sort=-%mem | head -10
```

## 🔧 Debugging y Troubleshooting

### Debugging con herramientas del sistema

#### Rastreo de llamadas del sistema
```bash
# Rastrear llamadas IPC
strace -e trace=shmget,shmat,shmdt,shmctl,semget,semop,semctl ./coordinador 50 2

# Rastrear todas las llamadas del sistema
strace -o trace.log ./coordinador 50 2
less trace.log

# Rastrear solo un proceso generador
strace -p $(pgrep generador | head -1)
```

#### Análisis de memoria
```bash
# Verificar memory leaks (si valgrind está disponible)
valgrind --tool=memcheck --leak-check=full ./coordinador 50 2

# Información de memoria del proceso
cat /proc/$(pgrep coordinador)/status | grep -E "(VmSize|VmRSS|VmData)"

# Mapas de memoria
cat /proc/$(pgrep coordinador)/maps
```

### Resolución de problemas comunes

#### Procesos colgados
```bash
# Listar procesos zombie
ps aux | grep -E "<defunct>|zombie"

# Matar procesos específicos
pkill coordinador
pkill generador

# Forzar terminación si es necesario
pkill -9 coordinador
pkill -9 generador
```

#### Limpieza de recursos IPC
```bash
# Limpiar recursos automáticamente
make ipc-clean

# Limpiar manualmente
ipcrm -M 0x1234  # Memoria compartida
ipcrm -S 0x5678  # Semáforos

# Limpiar todos los recursos del usuario actual
ipcs -m | grep $(whoami) | awk '{print $2}' | xargs -r ipcrm -m
ipcs -s | grep $(whoami) | awk '{print $2}' | xargs -r ipcrm -s
```

## 📈 Pruebas de Rendimiento

### Medición de tiempo de ejecución
```bash
# Tiempo simple
time ./coordinador 1000 4

# Tiempo detallado
/usr/bin/time -v ./coordinador 1000 4

# Múltiples ejecuciones para promedios
for i in {1..5}; do 
    echo "Ejecución $i:"
    time ./coordinador 500 3
    echo "---"
done
```

### Pruebas de estrés
```bash
# Muchos registros
./coordinador 10000 8

# Muchos generadores
./coordinador 1000 20

# Caso límite: 1 generador
./coordinador 1000 1

# Pocos registros, muchos generadores
./coordinador 50 10
```

## 🛠️ Mantenimiento

### Limpieza completa
```bash
# Limpiar todo
make clean-all

# Verificar que no quedan recursos
ipcs | grep $(whoami) || echo "No hay recursos IPC residuales"

# Verificar procesos
ps aux | grep -E "(coordinador|generador)" | grep -v grep || echo "No hay procesos activos"
```

### Reinicio completo del entorno
```bash
# Script de reinicio completo
pkill coordinador 2>/dev/null || true
pkill generador 2>/dev/null || true
make ipc-clean
make clean-all
make all
echo "Sistema reiniciado y listo para usar"
```

## 📋 Checklist de Verificación

Antes de cada prueba, verificar:

- [ ] No hay procesos coordinador/generador activos: `ps aux | grep -E "(coordinador|generador)"`
- [ ] No hay recursos IPC residuales: `ipcs | grep $(whoami)`
- [ ] El proyecto está compilado: `ls -la coordinador generador`
- [ ] Hay espacio en disco suficiente: `df -h .`

Después de cada prueba, verificar:

- [ ] El archivo CSV fue generado: `ls -la datos_generados.csv`
- [ ] La verificación AWK pasa: `./verify.awk datos_generados.csv`
- [ ] No quedan procesos zombie: `ps aux | grep defunct`
- [ ] Los recursos IPC fueron liberados correctamente

## 📞 Ayuda Adicional

```bash
# Ver ayuda del Makefile
make help

# Información sobre comandos IPC
man ipcs
man ipcrm

# Información sobre semáforos y memoria compartida
man shmget
man semget
```
* **Lenguaje de Programación:** C
* **Compilador:** GCC
* **Sistema de Control de Versiones:** Git
* **Herramientas de Monitoreo:** ps, htop, vmstat, ipcs
* **Entorno de Desarrollo:** Linux (preferentemente Ubuntu 22.04)
* **Comando de compilación:** `make`
