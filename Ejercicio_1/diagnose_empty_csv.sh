#!/bin/bash

# diagnose_empty_csv.sh - Diagnosticar por qué el CSV queda vacío

echo "================================================================"
echo "  Diagnóstico: Por qué el CSV queda vacío"
echo "================================================================"

# Función para verificar un componente
check_component() {
    local component="$1"
    local status="$2"
    local details="$3"
    
    printf "%-30s: " "$component"
    if [ "$status" = "OK" ]; then
        echo -e "\033[32m✅ OK\033[0m $details"
    elif [ "$status" = "WARN" ]; then
        echo -e "\033[33m⚠️  WARN\033[0m $details"
    else
        echo -e "\033[31m❌ FAIL\033[0m $details"
    fi
}

echo "Realizando diagnóstico completo..."
echo ""

# 1. Verificar compilación
echo "=== VERIFICACIÓN DE COMPILACIÓN ==="
if [ -f "coordinador" ] && [ -x "coordinador" ]; then
    check_component "Coordinador ejecutable" "OK" "$(ls -la coordinador | awk '{print $1, $9}')"
else
    check_component "Coordinador ejecutable" "FAIL" "No existe o no es ejecutable"
fi

if [ -f "generador" ] && [ -x "generador" ]; then
    check_component "Generador ejecutable" "OK" "$(ls -la generador | awk '{print $1, $9}')"
else
    check_component "Generador ejecutable" "FAIL" "No existe o no es ejecutable"
fi

echo ""

# 2. Verificar recursos del sistema
echo "=== VERIFICACIÓN DE RECURSOS DEL SISTEMA ==="

# Límites de IPC
shmmax=$(cat /proc/sys/kernel/shmmax 2>/dev/null || echo "0")
if [ "$shmmax" -gt 1024 ]; then
    check_component "Límite memoria compartida" "OK" "$shmmax bytes"
else
    check_component "Límite memoria compartida" "WARN" "Muy bajo: $shmmax"
fi

# Verificar semáforos
sem_info=$(cat /proc/sys/kernel/sem 2>/dev/null || echo "0 0 0 0")
check_component "Configuración semáforos" "OK" "$sem_info"

echo ""

# 3. Prueba de comunicación básica
echo "=== PRUEBA DE COMUNICACIÓN BÁSICA ==="

# Limpiar recursos previos
make ipc-clean >/dev/null 2>&1
pkill coordinador >/dev/null 2>&1
pkill generador >/dev/null 2>&1
rm -f datos_generados.csv test_debug.log

# Ejecutar prueba mínima con logging
echo "Ejecutando prueba mínima (3 registros, 1 generador)..."

# Usar timeout y capturar salida
timeout 15s strace -f -e trace=shmget,semget,semop,fork,execve ./coordinador 3 1 > test_debug.log 2>&1 &
TEST_PID=$!

# Monitorear por 10 segundos
for i in {1..10}; do
    sleep 1
    
    # Verificar procesos
    coord_count=$(pgrep coordinador | wc -l)
    gen_count=$(pgrep generador | wc -l)
    
    echo "  Segundo $i: Coordinador=$coord_count, Generadores=$gen_count"
    
    # Verificar si terminó prematuramente
    if ! kill -0 $TEST_PID 2>/dev/null; then
        echo "  Proceso terminó en segundo $i"
        break
    fi
done

# Forzar terminación si es necesario
if kill -0 $TEST_PID 2>/dev/null; then
    kill $TEST_PID 2>/dev/null
    sleep 1
    kill -9 $TEST_PID 2>/dev/null
fi

echo ""

# 4. Analizar resultados de la prueba
echo "=== ANÁLISIS DE RESULTADOS DE PRUEBA ==="

if [ -f "test_debug.log" ]; then
    echo "Log de ejecución generado:"
    echo "  Tamaño: $(wc -l < test_debug.log) líneas"
    echo ""
    
    # Buscar indicadores clave en el log
    echo "Indicadores en el log:"
    
    if grep -q "Error creando memoria compartida" test_debug.log; then
        check_component "Creación memoria compartida" "FAIL" "Error en shmget"
    elif grep -q "shmget" test_debug.log; then
        check_component "Creación memoria compartida" "OK" "shmget exitoso"
    else
        check_component "Creación memoria compartida" "WARN" "No se detectó"
    fi
    
    if grep -q "Error creando semáforos" test_debug.log; then
        check_component "Creación semáforos" "FAIL" "Error en semget"
    elif grep -q "semget" test_debug.log; then
        check_component "Creación semáforos" "OK" "semget exitoso"
    else
        check_component "Creación semáforos" "WARN" "No se detectó"
    fi
    
    gen_created=$(grep -c "Generador.*creado" test_debug.log)
    if [ "$gen_created" -gt 0 ]; then
        check_component "Creación de generadores" "OK" "$gen_created generadores creados"
    else
        check_component "Creación de generadores" "FAIL" "No se crearon generadores"
    fi
    
    if grep -q "iniciando bucle principal" test_debug.log; then
        check_component "Inicio del bucle principal" "OK" "Coordinador inició"
    else
        check_component "Inicio del bucle principal" "FAIL" "Coordinador no inició bucle"
    fi
    
    ids_assigned=$(grep -c "Asignados IDs" test_debug.log)
    if [ "$ids_assigned" -gt 0 ]; then
        check_component "Asignación de IDs" "OK" "$ids_assigned asignaciones"
    else
        check_component "Asignación de IDs" "FAIL" "No se asignaron IDs"
    fi
    
    records_processed=$(grep -c "DEBUG: Procesando registro" test_debug.log)
    if [ "$records_processed" -gt 0 ]; then
        check_component "Procesamiento registros" "OK" "$records_processed registros"
    else
        check_component "Procesamiento registros" "FAIL" "No se procesaron registros"
    fi
    
else
    check_component "Log de debug" "FAIL" "No se generó archivo de log"
fi

echo ""

# 5. Verificar archivo CSV
echo "=== VERIFICACIÓN DEL ARCHIVO CSV ==="

if [ -f "datos_generados.csv" ]; then
    lines=$(wc -l < datos_generados.csv)
    size=$(du -b datos_generados.csv | cut -f1)
    
    if [ "$lines" -gt 1 ]; then
        check_component "Archivo CSV" "OK" "$((lines-1)) registros, $size bytes"
        echo ""
        echo "Contenido del CSV:"
        head -10 datos_generados.csv
    elif [ "$lines" -eq 1 ]; then
        check_component "Archivo CSV" "WARN" "Solo cabecera, $size bytes"
    else
        check_component "Archivo CSV" "FAIL" "Archivo vacío"
    fi
else
    check_component "Archivo CSV" "FAIL" "No se creó el archivo"
fi

echo ""

# 6. Verificar estado final de IPC
echo "=== ESTADO FINAL DE RECURSOS IPC ==="

shm_count=$(ipcs -m | grep $(whoami) | wc -l)
if [ "$shm_count" -eq 0 ]; then
    check_component "Limpieza memoria compartida" "OK" "Sin recursos residuales"
else
    check_component "Limpieza memoria compartida" "WARN" "$shm_count recursos residuales"
fi

sem_count=$(ipcs -s | grep $(whoami) | wc -l)  
if [ "$sem_count" -eq 0 ]; then
    check_component "Limpieza semáforos" "OK" "Sin recursos residuales"
else
    check_component "Limpieza semáforos" "WARN" "$sem_count recursos residuales"
fi

echo ""

# 7. Mostrar fragmentos relevantes del log
if [ -f "test_debug.log" ] && [ -s "test_debug.log" ]; then
    echo "=== FRAGMENTOS RELEVANTES DEL LOG ==="
    echo ""
    echo "Primeras 20 líneas:"
    head -20 test_debug.log
    echo ""
    echo "Últimas 20 líneas:"
    tail -20 test_debug.log
fi

echo ""
echo "================================================================"
echo "=== RECOMENDACIONES BASADAS EN DIAGNÓSTICO ==="
echo "================================================================"

# Limpiar recursos finales
make ipc-clean >/dev/null 2>&1
pkill coordinador >/dev/null 2>&1
pkill generador >/dev/null 2>&1

echo ""
echo "Archivos de diagnóstico generados:"
echo "  - test_debug.log (log completo de la prueba)"
echo ""
echo "Para más información, revisar:"
echo "  cat test_debug.log | grep -E '(Error|DEBUG|Asignados|Procesando)'"
echo "  strace -f ./coordinador 5 1"
echo "  gdb --args ./coordinador 5 1"