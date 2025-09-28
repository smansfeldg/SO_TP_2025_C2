#!/bin/bash

# test_batch_progressive.sh - Lotes de prueba progresivos para diagnosticar problemas
# Sistema Generador de Datos Concurrente

# Colores para output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

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

# Función para limpiar y preparar
cleanup_and_prepare() {
    print_status "Limpiando recursos previos..."
    make ipc-clean >/dev/null 2>&1
    pkill -9 coordinador >/dev/null 2>&1
    pkill -9 generador >/dev/null 2>&1
    rm -f datos_generados.csv
    sleep 2
}

# Función para analizar resultados
analyze_results() {
    local test_name="$1"
    local expected_records="$2"
    
    echo ""
    print_status "=== Análisis de Resultados: $test_name ==="
    
    if [ -f "datos_generados.csv" ]; then
        local total_lines=$(wc -l < datos_generados.csv)
        local data_lines=$((total_lines - 1))
        
        echo "Archivo CSV generado:"
        echo "  - Total de líneas: $total_lines"
        echo "  - Registros de datos: $data_lines"
        echo "  - Esperados: $expected_records"
        
        if [ $data_lines -gt 0 ]; then
            print_success "✅ Datos generados correctamente"
            echo ""
            echo "Primeros 5 registros:"
            head -6 datos_generados.csv | tail -5
            
            if [ $data_lines -ge 10 ]; then
                echo ""
                echo "Últimos 5 registros:"
                tail -5 datos_generados.csv
            fi
            
            # Verificar integridad básica
            echo ""
            print_status "Verificación de integridad:"
            
            # IDs únicos
            local unique_ids=$(tail -n +2 datos_generados.csv | cut -d',' -f1 | sort -u | wc -l)
            echo "  - IDs únicos: $unique_ids de $data_lines"
            
            # Procesos únicos
            local unique_pids=$(tail -n +2 datos_generados.csv | cut -d',' -f2 | sort -u | wc -l)
            echo "  - Procesos únicos: $unique_pids"
            
            # Verificar formato
            local format_errors=$(tail -n +2 datos_generados.csv | awk -F',' 'NF != 4 {print NR}' | wc -l)
            if [ $format_errors -eq 0 ]; then
                print_success "  - Formato correcto"
            else
                print_warning "  - Errores de formato: $format_errors líneas"
            fi
            
        else
            print_error "❌ CSV vacío (solo cabecera)"
        fi
    else
        print_error "❌ No se generó archivo CSV"
    fi
    
    echo ""
}

echo "================================================================"
echo "  Lotes de Prueba Progresivos - Sistema Generador de Datos"
echo "================================================================"
echo ""

# Recompilar proyecto
print_status "Compilando proyecto..."
make clean >/dev/null 2>&1
make all

echo ""
echo "================================================================"
print_status "=== LOTE 1: Prueba Mínima (5 registros, 1 generador) ==="
echo "================================================================"

cleanup_and_prepare

print_status "Ejecutando: ./coordinador 5 1"
timeout 30s ./coordinador 5 1
analyze_results "Lote 1" 5

echo ""
echo "================================================================"
print_status "=== LOTE 2: Prueba Pequeña (20 registros, 2 generadores) ==="
echo "================================================================"

cleanup_and_prepare

print_status "Ejecutando: ./coordinador 20 2"
timeout 30s ./coordinador 20 2
analyze_results "Lote 2" 20

echo ""
echo "================================================================"
print_status "=== LOTE 3: Prueba Media (100 registros, 3 generadores) ==="
echo "================================================================"

cleanup_and_prepare

print_status "Ejecutando: ./coordinador 100 3"
timeout 45s ./coordinador 100 3
analyze_results "Lote 3" 100

echo ""
echo "================================================================"
print_status "=== LOTE 4: Prueba Balanceada (200 registros, 4 generadores) ==="
echo "================================================================"

cleanup_and_prepare

print_status "Ejecutando: ./coordinador 200 4"
timeout 60s ./coordinador 200 4
analyze_results "Lote 4" 200

echo ""
echo "================================================================"
print_status "=== LOTE 5: Prueba Grande (500 registros, 5 generadores) ==="
echo "================================================================"

cleanup_and_prepare

print_status "Ejecutando: ./coordinador 500 5"
timeout 90s ./coordinador 500 5
analyze_results "Lote 5" 500

echo ""
echo "================================================================"
print_status "=== LOTE 6: Prueba de Estrés (1000 registros, 8 generadores) ==="
echo "================================================================"

cleanup_and_prepare

print_status "Ejecutando: ./coordinador 1000 8"
timeout 120s ./coordinador 1000 8
analyze_results "Lote 6" 1000

echo ""
echo "================================================================"
print_status "=== LOTE 7: Caso Límite (10 registros, 10 generadores) ==="
echo "================================================================"

cleanup_and_prepare

print_status "Ejecutando: ./coordinador 10 10"
timeout 30s ./coordinador 10 10
analyze_results "Lote 7" 10

echo ""
echo "================================================================"
print_status "=== RESUMEN FINAL ==="
echo "================================================================"

cleanup_and_prepare

print_status "Lotes de prueba completados"
print_status "Si algún lote falló, revisar:"
echo "  1. Configuración de semáforos"
echo "  2. Comunicación entre coordinador y generador"
echo "  3. Logs de debug en la salida"
echo "  4. Recursos IPC disponibles"

echo ""
print_status "Para debug manual, use:"
echo "  strace -e trace=shmget,semget,semop ./coordinador 10 2"
echo "  ipcs -a (para ver recursos IPC)"
echo ""