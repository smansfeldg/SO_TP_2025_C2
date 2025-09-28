#!/bin/bash

# cleanup_processes.sh - Script para limpiar procesos huérfanos y zombie
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
echo "  Sistema de Limpieza de Procesos - Generador de Datos"
echo "================================================================"
echo ""

# Verificar procesos coordinador y generador
print_status "Verificando procesos activos..."

COORD_PIDS=$(pgrep coordinador 2>/dev/null)
GEN_PIDS=$(pgrep generador 2>/dev/null)

if [ -n "$COORD_PIDS" ]; then
    echo "Procesos coordinador encontrados:"
    ps -f -p $COORD_PIDS 2>/dev/null
    echo ""
fi

if [ -n "$GEN_PIDS" ]; then
    echo "Procesos generador encontrados:"
    ps -f -p $GEN_PIDS 2>/dev/null
    echo ""
fi

# Verificar procesos zombie
print_status "Verificando procesos zombie..."
ZOMBIE_COUNT=$(ps aux | grep -E "(coordinador|generador).*<defunct>" | grep -v grep | wc -l)

if [ $ZOMBIE_COUNT -gt 0 ]; then
    print_warning "Se encontraron $ZOMBIE_COUNT procesos zombie:"
    ps aux | grep -E "(coordinador|generador).*<defunct>" | grep -v grep
    echo ""
fi

# Función para terminar procesos gradualmente
terminate_processes() {
    local process_name=$1
    local pids=$2
    
    if [ -n "$pids" ]; then
        print_status "Terminando procesos $process_name..."
        
        # Paso 1: SIGTERM (terminación ordenada)
        echo "Enviando SIGTERM..."
        for pid in $pids; do
            if kill -TERM $pid 2>/dev/null; then
                echo "  SIGTERM enviado a PID $pid"
            fi
        done
        
        # Esperar 3 segundos
        sleep 3
        
        # Verificar cuáles siguen activos
        still_active=""
        for pid in $pids; do
            if kill -0 $pid 2>/dev/null; then
                still_active="$still_active $pid"
            fi
        done
        
        if [ -n "$still_active" ]; then
            print_warning "Algunos procesos siguen activos. Enviando SIGKILL..."
            for pid in $still_active; do
                if kill -KILL $pid 2>/dev/null; then
                    echo "  SIGKILL enviado a PID $pid"
                fi
            done
            sleep 1
        fi
    fi
}

# Terminar procesos si el usuario confirma
if [ -n "$COORD_PIDS" ] || [ -n "$GEN_PIDS" ]; then
    echo ""
    read -p "¿Desea terminar los procesos encontrados? (y/N): " -n 1 -r
    echo ""
    
    if [[ $REPLY =~ ^[Yy]$ ]]; then
        terminate_processes "coordinador" "$COORD_PIDS"
        terminate_processes "generador" "$GEN_PIDS"
        
        print_success "Procesos terminados"
    else
        print_status "No se terminaron los procesos"
    fi
fi

# Limpiar recursos IPC
print_status "Verificando recursos IPC..."

# Memoria compartida
SHM_IDS=$(ipcs -m 2>/dev/null | grep $(whoami) | awk '{print $2}')
if [ -n "$SHM_IDS" ]; then
    print_warning "Se encontraron recursos de memoria compartida:"
    ipcs -m | grep $(whoami)
    
    echo ""
    read -p "¿Desea limpiar la memoria compartida? (y/N): " -n 1 -r
    echo ""
    
    if [[ $REPLY =~ ^[Yy]$ ]]; then
        for shm_id in $SHM_IDS; do
            if ipcrm -m $shm_id 2>/dev/null; then
                print_success "Memoria compartida $shm_id eliminada"
            fi
        done
    fi
fi

# Semáforos
SEM_IDS=$(ipcs -s 2>/dev/null | grep $(whoami) | awk '{print $2}')
if [ -n "$SEM_IDS" ]; then
    print_warning "Se encontraron semáforos:"
    ipcs -s | grep $(whoami)
    
    echo ""
    read -p "¿Desea limpiar los semáforos? (y/N): " -n 1 -r
    echo ""
    
    if [[ $REPLY =~ ^[Yy]$ ]]; then
        for sem_id in $SEM_IDS; do
            if ipcrm -s $sem_id 2>/dev/null; then
                print_success "Semáforo $sem_id eliminado"
            fi
        done
    fi
fi

# Verificación final
echo ""
print_status "Verificación final..."

REMAINING_PROCESSES=$(ps aux | grep -E "(coordinador|generador)" | grep -v grep | grep -v cleanup_processes)
if [ -n "$REMAINING_PROCESSES" ]; then
    print_warning "Procesos restantes:"
    echo "$REMAINING_PROCESSES"
else
    print_success "No hay procesos coordinador/generador activos"
fi

REMAINING_IPC=$(ipcs | grep $(whoami) 2>/dev/null)
if [ -n "$REMAINING_IPC" ]; then
    print_warning "Recursos IPC restantes:"
    echo "$REMAINING_IPC"
else
    print_success "No hay recursos IPC residuales"
fi

echo ""
print_status "Limpieza completada."
echo "================================================================"