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
./servidor <clientes_concurrentes> <clientes_en_espera>
```

Ejemplo:
```bash
./servidor 5 10
```

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
- **Transacciones**: Bloqueo exclusivo del archivo durante transacciones
- **Configuración flexible**: Archivo de configuración para host, puerto, archivos
- **Logging**: Registro de actividades del servidor
- **Manejo de errores**: Detección automática de límite de clientes alcanzado
- **Limpieza automática**: Liberación completa de recursos al finalizar