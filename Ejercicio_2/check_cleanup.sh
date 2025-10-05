#!/bin/bash

echo "=== Verificación de Limpieza de Recursos ==="
echo

echo "1. Verificando procesos activos del servidor/cliente:"
PROCS=$(ps aux | grep -E "(servidor|cliente)" | grep -v grep | grep -v check_cleanup)
if [ -z "$PROCS" ]; then
    echo "   ✓ No hay procesos activos"
else
    echo "   ⚠ Procesos encontrados:"
    echo "$PROCS"
fi
echo

echo "2. Verificando archivos temporales de ejecución:"
TEMP_FILES=$(find . -name "temp*" -o -name "*.tmp" -o -name ".temp_lock" 2>/dev/null)
if [ -z "$TEMP_FILES" ]; then
    echo "   ✓ No hay archivos temporales de ejecución"
else
    echo "   ⚠ Archivos temporales encontrados:"
    echo "$TEMP_FILES"
fi
echo

echo "3. Verificando archivos ejecutables generados:"
EXEC_FILES=""
if [ -f "servidor" ]; then
    EXEC_FILES="$EXEC_FILES servidor"
fi
if [ -f "cliente" ]; then
    EXEC_FILES="$EXEC_FILES cliente"
fi

if [ -z "$EXEC_FILES" ]; then
    echo "   ✓ No hay ejecutables generados (ya limpiados)"
else
    echo "   ℹ Ejecutables generados presentes:$EXEC_FILES"
    echo "     (Usar 'make clean' para eliminarlos)"
fi
echo

echo "4. Verificando descriptores de archivo abiertos:"
if command -v lsof >/dev/null 2>&1; then
    OPEN_FILES=$(lsof | grep -E "(servidor|cliente|temp\.csv)" 2>/dev/null)
    if [ -z "$OPEN_FILES" ]; then
        echo "   ✓ No hay descriptores abiertos relacionados"
    else
        echo "   ⚠ Descriptores abiertos encontrados:"
        echo "$OPEN_FILES"
    fi
else
    echo "   - lsof no disponible, no se puede verificar descriptores"
fi
echo

echo "5. Verificando sockets en el puerto configurado:"
PORT=$(grep "PORT=" servidor.conf 2>/dev/null | cut -d'=' -f2)
if [ -z "$PORT" ]; then
    PORT=8080
fi

if command -v netstat >/dev/null 2>&1; then
    SOCKETS=$(netstat -ln | grep ":$PORT ")
    if [ -z "$SOCKETS" ]; then
        echo "   ✓ No hay sockets activos en puerto $PORT"
    else
        echo "   ⚠ Sockets activos en puerto $PORT:"
        echo "$SOCKETS"
    fi
else
    echo "   - netstat no disponible, no se puede verificar sockets"
fi
echo

echo "6. Verificando memoria compartida y semáforos:"
if command -v ipcs >/dev/null 2>&1; then
    SHM=$(ipcs -m 2>/dev/null | grep -v "key\|---")
    SEM=$(ipcs -s 2>/dev/null | grep -v "key\|---")
    
    if [ -z "$SHM" ] && [ -z "$SEM" ]; then
        echo "   ✓ No hay memoria compartida ni semáforos activos"
    else
        if [ ! -z "$SHM" ]; then
            echo "   ⚠ Memoria compartida encontrada:"
            echo "$SHM"
        fi
        if [ ! -z "$SEM" ]; then
            echo "   ⚠ Semáforos encontrados:"
            echo "$SEM"
        fi
    fi
else
    echo "   - ipcs no disponible, no se puede verificar IPC"
fi
echo

echo "=== Verificación Completada ==="