#!/bin/bash

# test_single_generator_failure.sh - Prueba específica para fallo de un generador

echo "=========================================================="
echo "  Prueba: Fallo de un Generador Individual"
echo "=========================================================="

# Limpiar recursos previos
make ipc-clean >/dev/null 2>&1
pkill coordinador >/dev/null 2>&1
pkill generador >/dev/null 2>&1
rm -f datos_generados.csv

# Recompilar con debug
echo "Recompilando..."
make clean >/dev/null 2>&1
make all

echo ""
echo "Iniciando coordinador con 100 registros y 4 generadores..."
./coordinador 100 4 &
COORD_PID=$!

# Esperar un momento para que se establezcan los procesos
sleep 3

echo ""
echo "Estado inicial de procesos:"
ps aux | grep -E "(coordinador|generador)" | grep -v grep

# Obtener PIDs de generadores
GEN_PIDS=($(pgrep generador))
echo ""
echo "Generadores encontrados: ${GEN_PIDS[@]}"

if [ ${#GEN_PIDS[@]} -gt 0 ]; then
    # Esperar un poco más para que procesen algunos registros
    sleep 5
    
    TARGET_PID=${GEN_PIDS[0]}
    echo ""
    echo "=== MATANDO GENERADOR PID $TARGET_PID ==="
    kill $TARGET_PID
    
    # Monitorear el estado cada segundo
    for i in {1..10}; do
        sleep 1
        REMAINING=$(pgrep generador | wc -l)
        COORD_ALIVE=$(pgrep coordinador | wc -l)
        echo "Segundo $i: Coordinador=$COORD_ALIVE, Generadores=$REMAINING"
        
        if [ $COORD_ALIVE -eq 0 ]; then
            echo "¡El coordinador terminó prematuramente!"
            break
        fi
    done
    
    echo ""
    echo "Estado final de procesos:"
    ps aux | grep -E "(coordinador|generador)" | grep -v grep || echo "No hay procesos activos"
    
    # Si el coordinador sigue vivo, dejarlo terminar naturalmente
    if kill -0 $COORD_PID 2>/dev/null; then
        echo "Esperando que el coordinador termine naturalmente..."
        wait $COORD_PID
    fi
    
    echo ""
    if [ -f "datos_generados.csv" ]; then
        RECORDS=$(tail -n +2 datos_generados.csv | wc -l)
        echo "Registros generados: $RECORDS"
        echo "Primeros registros:"
        head -10 datos_generados.csv
    else
        echo "No se generó archivo CSV"
    fi
else
    echo "No se encontraron generadores"
    kill $COORD_PID 2>/dev/null
fi

echo ""
echo "Limpiando recursos..."
make ipc-clean >/dev/null 2>&1
echo "Prueba completada."