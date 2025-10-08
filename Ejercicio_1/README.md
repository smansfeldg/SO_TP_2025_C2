# Sistema Generador de Datos Concurrente

## Descripción General

Sistema distribuido que genera registros de datos utilizando múltiples procesos concurrentes. El sistema está compuesto por un proceso coordinador que gestiona N procesos generadores, utilizando memoria compartida y semáforos para la comunicación y sincronización.

## Arquitectura del Sistema

### Proceso Coordinador
- **Función principal**: Gestiona el ciclo de vida del sistema y recopila datos
- **Responsabilidades**:
  - Crear y gestionar memoria compartida y semáforos
  - Lanzar y supervisar procesos generadores
  - Asignar bloques de IDs únicos a los generadores
  - Recibir registros generados y escribirlos al archivo CSV
  - Limpiar recursos al finalizar

### Procesos Generadores
- **Función principal**: Generar registros de datos con IDs únicos
- **Flujo de trabajo**:
  1. Solicitar bloque de IDs al coordinador
  2. Generar datos aleatorios para cada ID asignado
  3. Enviar registros completados al coordinador
  4. Repetir hasta agotar IDs disponibles

### Comunicación IPC (Inter-Process Communication)

#### Memoria Compartida
- **Clave**: `0x1234`
- **Estructura**: Contiene uniones para diferentes tipos de mensajes
- **Tipos de mensaje**:
  - `REQUEST_IDS`: Solicitud de bloque de IDs
  - `SEND_RECORD`: Envío de registro generado
  - `NO_MORE_IDS`: Señal de finalización
  - `SHUTDOWN`: Señal de cierre del sistema

#### Semáforos
- **Clave**: `0x5678`
- **Conjunto de 3 semáforos**:
  - `SEM_MUTEX`: Exclusión mutua para acceso a memoria compartida
  - `SEM_COORDINATOR`: Señalización para el coordinador
  - `SEM_GENERATOR`: Señalización para generadores

### Algoritmo de Sincronización

1. **Inicialización**:
   - Coordinador crea memoria compartida y semáforos
   - Lanza N procesos generadores como hijos

2. **Asignación de IDs**:
   - Generador solicita IDs (bloque de 10)
   - Coordinador asigna siguiente bloque disponible
   - Se mantiene contador global de próximo ID

3. **Generación de Datos**:
   - Generador crea registros con timestamp y datos aleatorios
   - Envía cada registro individualmente al coordinador

4. **Finalización**:
   - Cuando se agotan IDs, coordinador envía `NO_MORE_IDS`
   - Generadores terminan su ejecución
   - Coordinador espera a todos los hijos y limpia recursos

## Compilación y Ejecución

### Compilación
```bash
# Compilar todo el proyecto
make all

# Compilar componentes individuales
make coordinador    # Solo el coordinador
make generador      # Solo el generador

# Ver ayuda completa
make help
```

### Ejecución
```bash
# Sintaxis básica
./coordinador <total_registros> <num_generadores>

# Ejemplos
./coordinador 100 3     # 100 registros con 3 generadores
./coordinador 1000 5    # 1000 registros con 5 generadores
./coordinador 50 2      # Prueba rápida: 50 registros con 2 generadores
```

### Opciones Avanzadas
```bash
# Reiniciar automáticamente generadores fallidos
./coordinador 1000 4 --restart-failed

# Configurar máximo de fallos por generador
./coordinador 1000 4 --restart-failed --max-failures=5
```

## Pruebas y Verificación

### Pruebas Automáticas
```bash
# Prueba básica (100 registros, 3 generadores)
make test

# Prueba completa (1000 registros, 5 generadores)
make test-full
```

### Verificación de Resultados
```bash
# Verificar integridad del archivo CSV generado
make verify

# Script de prueba completo
chmod +x run_test.sh
./run_test.sh
```

### Archivo de Salida
- **Nombre**: `datos_generados.csv`
- **Formato**: `ID,ID_PROCESO,TIMESTAMP,DATO_ALEATORIO`
- **Ejemplo**:
  ```csv
  ID,ID_PROCESO,TIMESTAMP,DATO_ALEATORIO
  1,1234,1759609368,DATA_1_1759609368_MtkWdEiqieWk2VFz8wf8d1...
  2,1235,1759609369,DATA_2_1759609369_XbvNmQwErTyUiOpAsDfGhJ...
  ```

## Mantenimiento y Limpieza

### Limpieza de Archivos
```bash
# Limpiar archivos compilados
make clean

# Limpieza completa (incluye CSV y temporales)
make clean-all
```

### Gestión de Recursos IPC
```bash
# Ver estado de recursos IPC
make ipc-status

# Limpiar recursos IPC residuales (en caso de terminación abrupta)
make ipc-clean

# Limpiar procesos huérfanos
make process-clean
```

### Verificación Post-Ejecución
```bash
# Verificar que no quedan recursos IPC residuales
ipcs | grep $(whoami) || echo "No hay recursos IPC residuales"

# Verificar que no quedan procesos activos
ps aux | grep -E "(coordinador|generador)" | grep -v grep || echo "No hay procesos activos"
```

## Características Técnicas

- **Lenguaje**: C (estándar C99)
- **Compilador**: GCC
- **Dependencias**: Bibliotecas estándar de POSIX para IPC
- **Arquitectura**: Multiproceso con memoria compartida System V
- **Sincronización**: Semáforos System V
- **Formato de salida**: CSV
- **Compatibilidad**: Sistemas Linux/Unix
