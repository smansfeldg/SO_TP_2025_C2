# Servidor y Cliente TCP con Transacciones

## Compilación

```bash
make all
```

## Limpieza

```bash
# Limpiar ejecutables y archivos temporales
make clean

# Limpieza completa (incluye logs, cores, etc.)
make clean-all

# Verificar que no queden recursos abiertos
make check-resources
```

## Configuración del Servidor

El servidor lee su configuración desde el archivo `servidor.conf`. Puedes modificar:

- **HOST**: Dirección IP o nombre del host (usar 0.0.0.0 para todas las interfaces)
- **PORT**: Puerto de escucha
- **CSV_FILE**: Ruta al archivo CSV de datos
- **LOG_FILE**: Archivo de log del servidor

## Uso del Servidor

```bash
./servidor <clientes_concurrentes> <clientes_en_cola_espera>
```

Parámetros:

- `clientes_concurrentes`: Número máximo de clientes que pueden estar activos simultáneamente
- `clientes_en_cola_espera`: Número máximo de clientes que pueden esperar en cola cuando se alcance el límite

Ejemplo:

```bash
./servidor 5 10
```

Esto permite 5 clientes activos y hasta 10 clientes esperando en cola.

## Uso del Cliente

```bash
./cliente [IP_servidor] [Puerto]
```

Ejemplo:

```bash
./cliente 127.0.0.1 8080
```

## Comandos Disponibles

### Transacciones

- `BEGIN TRANSACTION` - Iniciar transacción
- `COMMIT TRANSACTION` - Confirmar transacción

### Consultas

- `SELECT <ID>` - Consultar registro por ID

### Modificaciones (requieren transacción activa)

- `INSERT <ID_PROCESO> <TIMESTAMP> <DATO_ALEATORIO>` - Insertar nuevo registro
- `UPDATE <ID> <ID_PROCESO> <TIMESTAMP> <DATO_ALEATORIO>` - Actualizar registro existente
- `DELETE <ID>` - Eliminar registro

### Control

- `EXIT` - Salir del cliente

## Ejemplo de Sesión

```
> BEGIN TRANSACTION
Servidor: OK: Transaccion iniciada. Archivo bloqueado exclusivamente.

> SELECT 0
Servidor: RESULTADO:
ID,ID_PROCESO,TIMESTAMP,DATO_ALEATORIO
0,3646,1759609368,DATA_0_1759609368_MtkWdEiqieWk2VFz8wf8d1...

> INSERT 1234 1759609999 DATA_TEST_123
Servidor: OK: Registro insertado con ID 3648. Pendiente de COMMIT.

> COMMIT TRANSACTION
Servidor: OK: Transaccion confirmada y bloqueo liberado.

> EXIT
```

## Tipos de Archivos

### Archivos Fuente (permanentes)

- `servidor.c`, `cliente.c` - Código fuente
- `Makefile` - Configuración de compilación
- `servidor.conf` - Configuración del servidor
- `README.md` - Documentación

### Archivos Generados (eliminables con make clean)

- `servidor`, `cliente` - Ejecutables compilados
- `server.log` - Log de ejecución
- `temp.csv` - Archivos temporales de operaciones
- `core.*` - Archivos de volcado (si hay crashes)

## Características

- **Control de concurrencia**: Máximo N clientes concurrentes
- **Cola de espera**: Hasta M clientes pueden esperar cuando se alcance el límite
- **Notificaciones automáticas**: Los clientes son informados de su posición en cola
- **Promoción automática**: Cuando se libera un slot, el siguiente cliente en cola es promovido
- **Transacciones**: Bloqueo exclusivo del archivo durante transacciones
- **Configuración flexible**: Archivo de configuración para host, puerto, archivos
- **Logging**: Registro de actividades del servidor
- **Manejo de errores**: Detección automática de límites alcanzados
- **Limpieza automática**: Liberación completa de recursos al finalizar

## Cola de Espera

Cuando se alcanza el límite de clientes concurrentes:

1. **Nuevos clientes** son agregados automáticamente a una cola de espera
2. **Modo de espera**: Los clientes en cola NO pueden enviar comandos, solo esperar
3. **Posición informada**: Cada cliente recibe su posición actual (ej: "Posición 3 de 10")
4. **Actualizaciones automáticas**: Cuando otros clientes salen, las posiciones se actualizan
5. **Promoción automática**: Cuando se libera un slot, el primer cliente en cola es promovido
6. **Límite de cola**: Si la cola está llena, nuevos clientes son rechazados

### Comportamiento del Cliente en Cola

- **Sin input**: El cliente no muestra prompt (">") mientras está en cola
- **Solo escucha**: Recibe únicamente mensajes de actualización del servidor
- **Cancelación**: El usuario puede presionar Ctrl+C para cancelar la espera
- **Promoción automática**: Cuando es promovido, vuelve al modo normal de comandos

### Mensajes del Sistema

- `EN_COLA_ESPERA: Posición X de Y` - Cliente agregado a cola (modo espera activado)
- `POSICION_ACTUALIZADA: X/Y` - Nueva posición en cola (solo recepción)
- `CONEXION_APROBADA` - Cliente promovido (modo normal activado)
- `CONEXION_ESTABLECIDA` - Conexión directa sin cola (modo normal)
- `SERVIDOR_CERRANDO` - Servidor cerrándose (desconexión automática)
- `ERROR: Limite de clientes concurrentes y cola alcanzados` - Servidor completamente lleno

### Estados del Cliente

1. **Modo Normal**: Puede enviar comandos, muestra prompt ">"
2. **Modo Cola de Espera**: Solo recibe mensajes, no puede enviar comandos
3. **Desconectado**: Cliente cerrado o rechazado por el servidor
