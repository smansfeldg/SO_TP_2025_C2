#!/bin/bash

# debug_communication.sh - Script para debuggear comunicación coordinador-generador

echo "================================================================"
echo "  Debug de Comunicación Coordinador-Generador"
echo "================================================================"

# Función para mostrar estado de IPC
show_ipc_status() {
    echo ""
    echo "=== Estado de Recursos IPC ==="
    echo "Memoria compartida:"
    ipcs -m | grep $(whoami) || echo "  No hay memoria compartida"
    echo ""
    echo "Semáforos:"
    ipcs -s | grep $(whoami) || echo "  No hay semáforos"
    echo ""
}

# Limpiar recursos previos
echo "Limpiando recursos previos..."
make ipc-clean >/dev/null 2>&1
pkill coordinador >/dev/null 2>&1
pkill generador >/dev/null 2>&1
rm -f datos_generados.csv

# Recompilar
echo "Recompilando con información de debug..."
make clean >/dev/null 2>&1
make all

show_ipc_status

echo "================================================================"
echo "=== PRUEBA DEBUG: 10 registros, 2 generadores ==="
echo "================================================================"

# Ejecutar coordinador en background con output redirigido
echo "Iniciando coordinador..."
./coordinador 10 2 > coordinador_debug.log 2>&1 &
COORD_PID=$!

echo "PID del coordinador: $COORD_PID"
sleep 2

show_ipc_status

echo ""
echo "Estado de procesos:"
ps aux | grep -E "(coordinador|generador)" | grep -v grep

echo ""
echo "Esperando 10 segundos para observar comportamiento..."
for i in {1..10}; do
    sleep 1
    echo -n "."
    
    # Verificar si el coordinador sigue vivo
    if ! kill -0 $COORD_PID 2>/dev/null; then
        echo ""
        echo "¡El coordinador terminó prematuramente!"
        break
    fi
done
echo ""

echo ""
echo "Estado final de procesos:"
ps aux | grep -E "(coordinador|generador)" | grep -v grep || echo "No hay procesos activos"

echo ""
echo "=== LOG DEL COORDINADOR ==="
if [ -f coordinador_debug.log ]; then
    cat coordinador_debug.log
else
    echo "No se encontró log del coordinador"
fi

echo ""
echo "=== ANÁLISIS DE RESULTADOS ==="
if [ -f datos_generados.csv ]; then
    echo "Archivo CSV generado:"
    wc -l datos_generados.csv
    echo ""
    echo "Contenido del CSV:"
    cat datos_generados.csv
else
    echo "❌ No se generó archivo CSV"
fi

show_ipc_status

echo ""
echo "=== DIAGNÓSTICO DE PROBLEMAS POSIBLES ==="

# 1. Verificar permisos
echo "1. Verificando permisos de archivos:"
ls -la coordinador generador 2>/dev/null || echo "  ❌ Ejecutables no encontrados"

# 2. Verificar límites del sistema
echo ""
echo "2. Verificando límites de IPC:"
echo "  Memoria compartida:"
cat /proc/sys/kernel/shmmax 2>/dev/null || echo "    No se pudo verificar"
echo "  Semáforos:"
cat /proc/sys/kernel/sem 2>/dev/null || echo "    No se pudo verificar"

# 3. Verificar si hay recursos residuales
echo ""
echo "3. Verificando recursos residuales:"
ipcs | grep $(whoami) && echo "  ⚠️ Hay recursos residuales" || echo "  ✅ No hay recursos residuales"

# Limpiar
echo ""
echo "Limpiando recursos finales..."
if kill -0 $COORD_PID 2>/dev/null; then
    echo "Terminando coordinador..."
    kill $COORD_PID
    sleep 2
    kill -9 $COORD_PID 2>/dev/null
fi

pkill generador >/dev/null 2>&1
make ipc-clean >/dev/null 2>&1

echo ""
echo "================================================================"
echo "=== RECOMENDACIONES ==="
echo "================================================================"
echo ""
echo "Si no se generaron datos, verificar:"
echo "1. Los generadores se están creando correctamente"
echo "2. Los semáforos se están inicializando bien"
echo "3. La comunicación entre procesos funciona"
echo "4. No hay deadlocks en la sincronización"
echo ""
echo "Para más debug, ejecutar manualmente:"
echo "  strace -f ./coordinador 5 1"
echo "  gdb ./coordinador"
echo ""