CC = gcc
CFLAGS = -Wall -Wextra -g # -Wall: Todas las advertencias. -Wextra: Más advertencias. -g: Incluir símbolos de depuración.

# Construye el servidor y el cliente
all: servidor cliente

# Depende del archivo Server.c
servidor: servidor.c
	$(CC) $(CFLAGS) servidor.c -o servidor

# Depende del archivo Client.c
cliente: cliente.c
	$(CC) $(CFLAGS) cliente.c -o cliente

# Elimina archivos ejecutables generados y archivos temporales de ejecución
clean:
	rm -f servidor cliente server.log temp.csv temp_backup.csv .temp_lock

# Limpieza completa: ejecutables + todos los archivos temporales/generados
clean-all: clean
	rm -f *.log *.tmp core core.* vgcore.* *.o *.out

# Verificar que no queden recursos abiertos (solo informativo)
check-resources:
	@echo "Verificando procesos del servidor..."
	@ps aux | grep -E "(servidor|cliente)" | grep -v grep || echo "No hay procesos activos"
	@echo "Verificando archivos temporales..."
	@ls -la temp* 2>/dev/null || echo "No hay archivos temporales"
	@echo "Verificando archivos de log..."
	@ls -la *.log 2>/dev/null || echo "No hay archivos de log"