#!/bin/bash

# generate_synthetic_data.sh - Generar datos sintéticos para comparación y pruebas

echo "================================================================"
echo "  Generador de Datos Sintéticos de Referencia"
echo "================================================================"

# Función para generar datos aleatorios
generate_random_string() {
    local length=${1:-20}
    cat /dev/urandom | tr -dc 'a-zA-Z0-9_-' | fold -w $length | head -n 1
}

# Función para generar un CSV sintético
generate_synthetic_csv() {
    local records=$1
    local processes=$2
    local filename=$3
    
    echo "Generando $records registros con $processes procesos simulados..."
    
    # Escribir cabecera
    echo "ID,ID_PROCESO,TIMESTAMP,DATO_ALEATORIO" > "$filename"
    
    # PIDs simulados (números altos para simular procesos reales)
    local base_pid=10000
    local pids=()
    for ((i=0; i<processes; i++)); do
        pids+=($((base_pid + i + RANDOM % 1000)))
    done
    
    # Timestamp base
    local base_timestamp=$(date +%s)
    
    # Generar registros
    for ((id=1; id<=records; id++)); do
        local pid_index=$((RANDOM % processes))
        local pid=${pids[$pid_index]}
        local timestamp=$((base_timestamp + RANDOM % 3600))  # Variación de 1 hora
        local random_data="DATA_${id}_${timestamp}_$(generate_random_string 15)"
        
        echo "$id,$pid,$timestamp,$random_data" >> "$filename"
        
        # Mostrar progreso cada 100 registros
        if ((id % 100 == 0)); then
            echo "  Generados $id/$records registros..."
        fi
    done
    
    echo "✅ Archivo $filename generado con $records registros"
}

# Función para analizar un CSV
analyze_csv() {
    local filename=$1
    local name="$2"
    
    echo ""
    echo "=== Análisis de $name ==="
    
    if [ ! -f "$filename" ]; then
        echo "❌ Archivo no encontrado: $filename"
        return 1
    fi
    
    local total_lines=$(wc -l < "$filename")
    local data_lines=$((total_lines - 1))
    
    echo "Estadísticas básicas:"
    echo "  - Total de líneas: $total_lines"
    echo "  - Registros de datos: $data_lines"
    echo "  - Tamaño del archivo: $(du -h "$filename" | cut -f1)"
    
    if [ $data_lines -gt 0 ]; then
        echo ""
        echo "Análisis de contenido:"
        
        # IDs únicos
        local unique_ids=$(tail -n +2 "$filename" | cut -d',' -f1 | sort -u | wc -l)
        echo "  - IDs únicos: $unique_ids"
        
        # Rango de IDs
        local min_id=$(tail -n +2 "$filename" | cut -d',' -f1 | sort -n | head -1)
        local max_id=$(tail -n +2 "$filename" | cut -d',' -f1 | sort -n | tail -1)
        echo "  - Rango de IDs: $min_id - $max_id"
        
        # Procesos únicos
        local unique_pids=$(tail -n +2 "$filename" | cut -d',' -f2 | sort -u | wc -l)
        echo "  - Procesos únicos: $unique_pids"
        
        # Distribución por proceso
        echo "  - Distribución por proceso:"
        tail -n +2 "$filename" | cut -d',' -f2 | sort | uniq -c | head -5 | while read count pid; do
            echo "    PID $pid: $count registros"
        done
        
        # Verificar formato
        local format_errors=$(tail -n +2 "$filename" | awk -F',' 'NF != 4 {count++} END {print count+0}')
        if [ $format_errors -eq 0 ]; then
            echo "  - Formato: ✅ Correcto"
        else
            echo "  - Formato: ❌ $format_errors errores"
        fi
        
        echo ""
        echo "Muestra de datos (primeros 5 registros):"
        head -6 "$filename" | tail -5 | nl
        
        if [ $data_lines -gt 10 ]; then
            echo ""
            echo "Muestra de datos (últimos 5 registros):"
            tail -5 "$filename" | nl -v$((data_lines - 4))
        fi
    fi
}

# Generar diferentes tamaños de datos sintéticos
echo ""
echo "Generando datasets de referencia..."

# Dataset pequeño
generate_synthetic_csv 50 3 "datos_referencia_pequeño.csv"
analyze_csv "datos_referencia_pequeño.csv" "Dataset Pequeño"

echo ""
echo "================================================================"

# Dataset medio
generate_synthetic_csv 200 4 "datos_referencia_medio.csv"  
analyze_csv "datos_referencia_medio.csv" "Dataset Medio"

echo ""
echo "================================================================"

# Dataset grande
generate_synthetic_csv 1000 6 "datos_referencia_grande.csv"
analyze_csv "datos_referencia_grande.csv" "Dataset Grande"

echo ""
echo "================================================================"
echo "=== COMPARACIÓN CON DATOS REALES ==="
echo "================================================================"

# Analizar datos generados por el sistema real si existen
if [ -f "datos_generados.csv" ]; then
    analyze_csv "datos_generados.csv" "Sistema Real"
    
    # Comparar con referencia
    local real_lines=$(tail -n +2 "datos_generados.csv" 2>/dev/null | wc -l)
    echo ""
    echo "=== Comparación ==="
    echo "Sistema Real: $real_lines registros"
    echo "Referencia Pequeña: 50 registros"
    echo "Referencia Media: 200 registros" 
    echo "Referencia Grande: 1000 registros"
    
    if [ $real_lines -eq 0 ]; then
        echo ""
        echo "❌ El sistema real no generó datos"
        echo "   Usar los datasets de referencia para comparar estructura esperada"
    fi
else
    echo "No se encontró archivo de datos reales (datos_generados.csv)"
    echo "Ejecutar el coordinador primero, luego este script para comparar"
fi

echo ""
echo "================================================================"
echo "=== ARCHIVOS GENERADOS ==="
echo "================================================================"
ls -la datos_referencia_*.csv 2>/dev/null || echo "No se generaron archivos de referencia"

echo ""
echo "Usar estos archivos para:"
echo "1. Comparar estructura y formato esperado"
echo "2. Probar el script verify.awk"
echo "3. Verificar herramientas de análisis"
echo "4. Benchmarking de rendimiento"

echo ""
echo "Comandos útiles:"
echo "  ./verify.awk datos_referencia_medio.csv"
echo "  diff datos_generados.csv datos_referencia_pequeño.csv"
echo "  wc -l datos_referencia_*.csv"