#!/bin/bash

# test_fault_tolerance.sh - Script para probar tolerancia a fallos
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

echo "================================================================"
echo "  Prueba de Tolerancia a Fallos - Sistema Generador de Datos"
echo "================================================================"
echo ""

# Compilar si es necesario
if [ ! -f "./coordinador" ] || [ ! -f "./generador" ]; then
    print_status "Compilando proyecto..."
    make clean && make all
fi

# Limpiar recursos previos
print_status "Limpiando recursos previos..."
make ipc-clean >/dev/null 2>&1
pkill coordinador >/dev/null 2>&1
pkill generador >/dev/null 2>&1
rm -f datos_generados.csv

echo ""
print_status "=== PRUEBA 1: Sistema sin fallos (control) ==="
print_status "Ejecutando: ./coordinador 200 4"

./coordinador 200 4 &
COORD_PID=$!
sleep 3

print_status "Estado de procesos:"
ps aux | grep -E "(coordinador|generador)" | grep -v grep

wait $COORD_PID
if [ -f "datos_generados.csv" ]; then
    RECORDS=$(tail -n +2 datos_generados.csv | wc -l)
    print_success "Prueba de control completada: $RECORDS registros generados"
else
    print_error "Prueba de control falló"
fi

echo ""
echo "================================================================"
print_status "=== PRUEBA 2: Fallo de un generador (sin reinicio) ==="
print_status "Ejecutando: ./coordinador 500 5"

make ipc-clean >/dev/null 2>&1
rm -f datos_generados.csv

./coordinador 500 5 &
COORD_PID=$!
sleep 2

# Obtener PIDs de generadores
GEN_PIDS=($(pgrep generador))
print_status "Generadores activos: ${#GEN_PIDS[@]}"

if [ ${#GEN_PIDS[@]} -gt 0 ]; then
    # Matar un generador después de 3 segundos
    sleep 3
    TARGET_PID=${GEN_PIDS[0]}
    print_warning "Matando generador PID $TARGET_PID para simular fallo"
    kill -9 $TARGET_PID
    
    # Monitorear por 5 segundos más
    for i in {1..5}; do
        sleep 1
        REMAINING=$(pgrep generador | wc -l)
        print_status "Generadores restantes: $REMAINING"
    done
fi

wait $COORD_PID
RECORDS_FAULT=$(tail -n +2 datos_generados.csv 2>/dev/null | wc -l)
print_warning "Prueba con fallo completada: $RECORDS_FAULT registros generados"

echo ""
echo "================================================================"
print_status "=== PRUEBA 3: Fallo con reinicio automático ==="
print_status "Ejecutando: ./coordinador 300 4 --restart-failed --max-failures=2"

make ipc-clean >/dev/null 2>&1
rm -f datos_generados.csv

./coordinador 300 4 --restart-failed --max-failures=2 &
COORD_PID=$!
sleep 2

# Obtener PIDs de generadores
GEN_PIDS=($(pgrep generador))
print_status "Generadores activos iniciales: ${#GEN_PIDS[@]}"

if [ ${#GEN_PIDS[@]} -gt 0 ]; then
    # Simular múltiples fallos
    sleep 3
    TARGET_PID=${GEN_PIDS[0]}
    print_warning "Primer fallo: Matando generador PID $TARGET_PID"
    kill -9 $TARGET_PID
    
    sleep 3
    # Encontrar el generador reiniciado y matarlo de nuevo
    NEW_PIDS=($(pgrep generador))
    for pid in "${NEW_PIDS[@]}"; do
        if [[ ! " ${GEN_PIDS[@]} " =~ " ${pid} " ]]; then
            print_warning "Segundo fallo: Matando generador reiniciado PID $pid"
            kill -9 $pid
            break
        fi
    done
    
    # Monitorear el estado final
    sleep 5
    FINAL_COUNT=$(pgrep generador | wc -l)
    print_status "Generadores finales activos: $FINAL_COUNT"
fi

wait $COORD_PID
RECORDS_RESTART=$(tail -n +2 datos_generados.csv 2>/dev/null | wc -l)
print_success "Prueba con reinicio completada: $RECORDS_RESTART registros generados"

echo ""
echo "================================================================"
print_status "=== PRUEBA 4: Fallo masivo de generadores ==="
print_status "Ejecutando: ./coordinador 400 6"

make ipc-clean >/dev/null 2>&1
rm -f datos_generados.csv

./coordinador 400 6 &
COORD_PID=$!
sleep 3

# Matar la mayoría de generadores
GEN_PIDS=($(pgrep generador))
print_status "Generadores iniciales: ${#GEN_PIDS[@]}"

if [ ${#GEN_PIDS[@]} -gt 2 ]; then
    print_warning "Simulando fallo masivo: matando ${#GEN_PIDS[@]} generadores"
    for (( i=0; i<${#GEN_PIDS[@]}-1; i++ )); do
        kill -9 ${GEN_PIDS[i]}
        print_warning "  Matado PID ${GEN_PIDS[i]}"
        sleep 0.5
    done
    
    # Monitorear recuperación
    for i in {1..8}; do
        sleep 1
        REMAINING=$(pgrep generador | wc -l)
        print_status "Generadores restantes: $REMAINING"
        if [ $REMAINING -eq 0 ]; then
            print_warning "Todos los generadores han fallado"
            break
        fi
    done
fi

wait $COORD_PID
RECORDS_MASSIVE=$(tail -n +2 datos_generados.csv 2>/dev/null | wc -l)
print_warning "Prueba de fallo masivo completada: $RECORDS_MASSIVE registros generados"

echo ""
echo "================================================================"
print_status "=== RESUMEN DE PRUEBAS ==="
echo "Prueba de control (sin fallos):     $RECORDS registros"
echo "Prueba con un fallo:                $RECORDS_FAULT registros"  
echo "Prueba con reinicio automático:     $RECORDS_RESTART registros"
echo "Prueba de fallo masivo:             $RECORDS_MASSIVE registros"

echo ""
if [ $RECORDS_FAULT -gt 0 ] && [ $RECORDS_RESTART -ge $RECORDS_FAULT ]; then
    print_success "✅ Sistema demostró tolerancia a fallos"
    print_success "✅ Reinicio automático funcionó correctamente"
else
    print_warning "⚠️  Revisar configuración de tolerancia a fallos"
fi

echo ""
print_status "Limpiando recursos finales..."
make ipc-clean >/dev/null 2>&1

print_status "Pruebas de tolerancia a fallos completadas"
echo "================================================================"