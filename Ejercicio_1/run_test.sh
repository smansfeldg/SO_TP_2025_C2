#!/bin/bash

# run_test.sh - Script de prueba para el Sistema Generador de Datos Concurrente
# Ejercicio 1 - Sistemas Operativos

# Configuración de colores para output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Función para mostrar mensajes con colores
print_status() {
    echo -e "${BLUE}[INFO]${NC} $1"
}

print_success() {
    echo -e "${GREEN}[ÉXITO]${NC} $1"
}

print_warning() {
    echo -e "${YELLOW}[ADVERTENCIA]${NC} $1"
}

print_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

# Función para limpiar recursos IPC residuales
cleanup_ipc() {
    print_status "Limpiando recursos IPC residuales..."
    
    # Intentar limpiar memoria compartida
    ipcrm -M 0x1234 2>/dev/null && print_success "Memoria compartida limpiada" || true
    
    # Intentar limpiar semáforos
    ipcrm -S 0x5678 2>/dev/null && print_success "Semáforos limpiados" || true
}

# Función para verificar dependencias
check_dependencies() {
    print_status "Verificando dependencias..."
    
    # Verificar make
    if ! command -v make >/dev/null 2>&1; then
        print_error "make no está disponible"
        exit 1
    fi
    
    # Verificar gcc
    if ! command -v gcc >/dev/null 2>&1; then
        print_error "gcc no está disponible"
        exit 1
    fi
    
    # Verificar herramientas del sistema
    if ! command -v ps >/dev/null 2>&1; then
        print_warning "ps no disponible - monitoreo de procesos limitado"
    fi
    
    if ! command -v ipcs >/dev/null 2>&1; then
        print_warning "ipcs no disponible - monitoreo IPC limitado"
    fi
    
    print_success "Dependencias verificadas"
}

# Función para compilar el proyecto
compile_project() {
    print_status "Compilando proyecto..."
    
    if make clean >/dev/null 2>&1; then
        print_success "Limpieza previa completada"
    fi
    
    if make all; then
        print_success "Compilación exitosa"
        return 0
    else
        print_error "Error en la compilación"
        return 1
    fi
}

# Función para ejecutar una prueba
run_test() {
    local records=$1
    local generators=$2
    local test_name="$3"
    
    print_status "Ejecutando $test_name ($records registros, $generators generadores)..."
    
    # Limpiar recursos previos
    cleanup_ipc
    
    # Eliminar CSV anterior
    rm -f datos_generados.csv
    
    # Ejecutar coordinador
    echo "Comando: ./coordinador $records $generators"
    echo "----------------------------------------"
    
    if timeout 30s ./coordinador $records $generators; then
        print_success "$test_name completada"
        return 0
    else
        local exit_code=$?
        if [ $exit_code -eq 124 ]; then
            print_error "$test_name terminada por timeout (30s)"
        else
            print_error "$test_name falló (código: $exit_code)"
        fi
        return 1
    fi
}

# Función para verificar el archivo CSV
verify_csv() {
    local csv_file="datos_generados.csv"
    
    print_status "Verificando archivo CSV generado..."
    
    if [ ! -f "$csv_file" ]; then
        print_error "Archivo CSV no encontrado: $csv_file"
        return 1
    fi
    
    # Información básica del archivo
    local total_lines=$(wc -l < "$csv_file")
    local data_lines=$((total_lines - 1))  # Excluir cabecera
    
    echo "Archivo: $csv_file"
    echo "Total de líneas: $total_lines"
    echo "Registros de datos: $data_lines"
    echo "Tamaño: $(du -h "$csv_file" | cut -f1)"
    
    # Verificar cabecera
    local header=$(head -n1 "$csv_file")
    local expected_header="ID,ID_PROCESO,TIMESTAMP,DATO_ALEATORIO"
    
    if [ "$header" = "$expected_header" ]; then
        print_success "Cabecera CSV correcta"
    else
        print_error "Cabecera CSV incorrecta"
        echo "Esperada: $expected_header"
        echo "Encontrada: $header"
    fi
    
    # Mostrar algunos registros de ejemplo
    echo ""
    echo "Primeros 5 registros:"
    head -n6 "$csv_file" | tail -n5 | nl
    
    echo ""
    echo "Últimos 5 registros:"
    tail -n5 "$csv_file" | nl -v$((data_lines - 4))
    
    # Usar verify.awk si está disponible
    if [ -f "verify.awk" ] && command -v awk >/dev/null 2>&1; then
        print_status "Ejecutando verificación detallada con AWK..."
        if ./verify.awk "$csv_file"; then
            print_success "Verificación AWK completada"
        else
            print_error "Verificación AWK falló"
            return 1
        fi
    else
        print_warning "verify.awk no disponible - verificación básica solamente"
    fi
    
    return 0
}

# Función para mostrar estado del sistema
show_system_status() {
    print_status "Estado del sistema:"
    
    echo ""
    echo "=== Procesos activos ==="
    if command -v ps >/dev/null 2>&1; then
        ps aux | grep -E "(coordinador|generador)" | grep -v grep || echo "No se encontraron procesos relacionados"
    else
        print_warning "Comando ps no disponible"
    fi
    
    echo ""
    echo "=== Recursos IPC ==="
    if command -v ipcs >/dev/null 2>&1; then
        echo "Memoria compartida:"
        ipcs -m
        echo ""
        echo "Semáforos:"
        ipcs -s
    else
        print_warning "Comando ipcs no disponible"
    fi
}

# Función principal
main() {
    echo "================================================================"
    echo "  Sistema Generador de Datos Concurrente - Script de Pruebas"
    echo "  Ejercicio 1 - Sistemas Operativos"
    echo "================================================================"
    echo ""
    
    # Verificar dependencias
    check_dependencies
    
    # Limpiar recursos previos
    cleanup_ipc
    
    # Compilar proyecto
    if ! compile_project; then
        exit 1
    fi
    
    echo ""
    echo "================================================================"
    echo "  EJECUTANDO PRUEBAS"
    echo "================================================================"
    echo ""
    
    # Prueba 1: Básica
    if run_test 50 2 "Prueba Básica"; then
        verify_csv
    fi
    
    echo ""
    echo "----------------------------------------------------------------"
    echo ""
    
    # Prueba 2: Media
    if run_test 200 4 "Prueba Media"; then
        verify_csv
    fi
    
    echo ""
    echo "----------------------------------------------------------------"
    echo ""
    
    # Prueba 3: Avanzada
    if run_test 500 6 "Prueba Avanzada"; then
        verify_csv
    fi
    
    echo ""
    echo "================================================================"
    echo "  RESUMEN FINAL"
    echo "================================================================"
    echo ""
    
    # Mostrar estado final
    show_system_status
    
    # Limpieza final
    cleanup_ipc
    
    print_success "Todas las pruebas completadas"
    echo ""
    echo "Archivos generados:"
    ls -la datos_generados.csv 2>/dev/null || print_warning "No se encontró archivo CSV final"
    
    echo ""
    print_status "Para ejecutar pruebas manuales, use:"
    echo "  ./coordinador <total_registros> <num_generadores>"
    echo ""
    print_status "Para verificar el CSV manualmente, use:"
    echo "  make verify"
}

# Configurar manejo de señales
trap cleanup_ipc EXIT INT TERM

# Ejecutar función principal
main "$@"