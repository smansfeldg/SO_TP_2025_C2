# Makefile para Sistema Generador de Datos Concurrente
# Ejercicio 1 - Sistemas Operativos

# Configuración del compilador
CC = gcc
CFLAGS = -Wall -Wextra -std=c99 -pedantic -D_GNU_SOURCE
LDFLAGS = 
INCLUDES = -Iinclude

# Directorios
SRCDIR = src
INCDIR = include
OBJDIR = obj

# Archivos fuente
SHARED_SRC = $(SRCDIR)/shared.c
COORDINADOR_SRC = $(SRCDIR)/coordinador.c
GENERADOR_SRC = $(SRCDIR)/generador.c

# Archivos objeto
SHARED_OBJ = $(OBJDIR)/shared.o
COORDINADOR_OBJ = $(OBJDIR)/coordinador.o
GENERADOR_OBJ = $(OBJDIR)/generador.o

# Ejecutables
COORDINADOR_BIN = coordinador
GENERADOR_BIN = generador

# Archivos de salida
CSV_FILE = datos_generados.csv

# Objetivo principal
all: $(COORDINADOR_BIN) $(GENERADOR_BIN)

# Crear directorio de objetos
$(OBJDIR):
	mkdir -p $(OBJDIR)

# Compilar coordinador
$(COORDINADOR_BIN): $(COORDINADOR_OBJ) $(SHARED_OBJ)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)
	@echo "Coordinador compilado exitosamente"

# Compilar generador
$(GENERADOR_BIN): $(GENERADOR_OBJ) $(SHARED_OBJ)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)
	@echo "Generador compilado exitosamente"

# Compilar objeto del coordinador
$(COORDINADOR_OBJ): $(COORDINADOR_SRC) $(INCDIR)/shared.h | $(OBJDIR)
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

# Compilar objeto del generador
$(GENERADOR_OBJ): $(GENERADOR_SRC) $(INCDIR)/shared.h | $(OBJDIR)
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

# Compilar objeto compartido
$(SHARED_OBJ): $(SHARED_SRC) $(INCDIR)/shared.h | $(OBJDIR)
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

# Ejecutar prueba básica
test: all
	@echo "=== Ejecutando prueba básica ==="
	@echo "Generando 100 registros con 3 generadores..."
	./$(COORDINADOR_BIN) 100 3

# Ejecutar prueba completa
test-full: all
	@echo "=== Ejecutando prueba completa ==="
	@echo "Generando 1000 registros con 5 generadores..."
	./$(COORDINADOR_BIN) 1000 5

# Verificar archivo CSV (requiere awk)
verify: $(CSV_FILE)
	@echo "=== Verificando archivo CSV ==="
	@if command -v awk >/dev/null 2>&1; then \
		./verify.awk $(CSV_FILE); \
	else \
		echo "AWK no disponible. Verificación manual requerida."; \
		echo "Líneas en CSV: $$(wc -l < $(CSV_FILE))"; \
		echo "Primeras 10 líneas:"; \
		head -10 $(CSV_FILE); \
	fi

# Limpiar archivos compilados
clean:
	rm -rf $(OBJDIR)
	rm -f $(COORDINADOR_BIN) $(GENERADOR_BIN)
	@echo "Archivos compilados eliminados"

# Limpiar todo incluyendo archivos de salida
clean-all: clean
	rm -f $(CSV_FILE)
	@echo "Todos los archivos generados eliminados"

# Mostrar estado de recursos IPC
ipc-status:
	@echo "=== Estado de recursos IPC ==="
	@echo "Memoria compartida:"
	@ipcs -m 2>/dev/null || echo "Comando ipcs no disponible o sin permisos"
	@echo "Semáforos:"
	@ipcs -s 2>/dev/null || echo "Comando ipcs no disponible o sin permisos"

# Limpiar recursos IPC (en caso de terminación abrupta)
ipc-clean:
	@echo "=== Limpiando recursos IPC ==="
	@echo "Eliminando memoria compartida con clave 0x1234..."
	@ipcrm -M 0x1234 2>/dev/null || echo "No se encontró memoria compartida con esa clave"
	@echo "Eliminando semáforos con clave 0x5678..."
	@ipcrm -S 0x5678 2>/dev/null || echo "No se encontraron semáforos con esa clave"

# Mostrar información de uso
help:
	@echo "Sistema Generador de Datos Concurrente - Makefile"
	@echo ""
	@echo "Objetivos disponibles:"
	@echo "  all          - Compilar coordinador y generador"
	@echo "  test         - Ejecutar prueba básica (100 registros, 3 generadores)"
	@echo "  test-full    - Ejecutar prueba completa (1000 registros, 5 generadores)"
	@echo "  verify       - Verificar integridad del archivo CSV generado"
	@echo "  clean        - Eliminar archivos compilados"
	@echo "  clean-all    - Eliminar archivos compilados y de salida"
	@echo "  ipc-status   - Mostrar estado de recursos IPC"
	@echo "  ipc-clean    - Limpiar recursos IPC residuales"
	@echo "  help         - Mostrar esta ayuda"
	@echo ""
	@echo "Uso típico:"
	@echo "  make all              # Compilar todo"
	@echo "  make test             # Ejecutar prueba"
	@echo "  make verify           # Verificar resultados"
	@echo ""
	@echo "Uso manual:"
	@echo "  ./coordinador <total_registros> <num_generadores>"

# Configurar objetivos que no generan archivos
.PHONY: all test test-full verify clean clean-all ipc-status ipc-clean help

# Configuraciones adicionales
.DEFAULT_GOAL := all
.SUFFIXES:
.DELETE_ON_ERROR: