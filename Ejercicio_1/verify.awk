#!/usr/bin/awk -f

# verify.awk - Script de verificación para el archivo CSV generado
# Sistema Generador de Datos Concurrente con Memoria Compartida
# Ejercicio 1 - Sistemas Operativos

BEGIN {
    print "=== Verificación del Archivo CSV ==="
    
    # Variables para estadísticas
    total_records = 0
    header_found = 0
    errors = 0
    warnings = 0
    
    # Arrays para verificación
    delete ids_seen
    delete process_ids
    delete timestamps
    
    # Variables para rangos
    min_id = 999999
    max_id = 0
    min_timestamp = 999999999999
    max_timestamp = 0
}

# Procesar cabecera
NR == 1 {
    expected_header = "ID,ID_PROCESO,TIMESTAMP,DATO_ALEATORIO"
    if ($0 == expected_header) {
        print "✓ Cabecera correcta encontrada"
        header_found = 1
    } else {
        print "✗ ERROR: Cabecera incorrecta"
        print "  Esperada: " expected_header
        print "  Encontrada: " $0
        errors++
    }
    next
}

# Procesar registros de datos
NR > 1 {
    total_records++
    
    # Dividir la línea por comas
    split($0, fields, ",")
    
    # Verificar número de campos
    if (length(fields) != 4) {
        print "✗ ERROR en línea " NR ": Número incorrecto de campos (" length(fields) " en lugar de 4)"
        print "  Línea: " $0
        errors++
        next
    }
    
    # Extraer campos
    id = fields[1]
    process_id = fields[2]
    timestamp = fields[3]
    random_data = fields[4]
    
    # Verificar ID
    if (id !~ /^[0-9]+$/ || id <= 0) {
        print "✗ ERROR en línea " NR ": ID inválido (" id ")"
        errors++
    } else {
        # Verificar duplicados
        if (id in ids_seen) {
            print "✗ ERROR en línea " NR ": ID duplicado (" id ")"
            print "  Primera aparición en línea " ids_seen[id]
            errors++
        } else {
            ids_seen[id] = NR
        }
        
        # Actualizar rangos
        if (id < min_id) min_id = id
        if (id > max_id) max_id = id
    }
    
    # Verificar Process ID
    if (process_id !~ /^[0-9]+$/ || process_id <= 0) {
        print "✗ ERROR en línea " NR ": Process ID inválido (" process_id ")"
        errors++
    } else {
        process_ids[process_id]++
    }
    
    # Verificar Timestamp
    if (timestamp !~ /^[0-9]+$/ || timestamp <= 0) {
        print "✗ ERROR en línea " NR ": Timestamp inválido (" timestamp ")"
        errors++
    } else {
        # Actualizar rangos de timestamp
        if (timestamp < min_timestamp) min_timestamp = timestamp
        if (timestamp > max_timestamp) max_timestamp = timestamp
        timestamps[timestamp]++
    }
    
    # Verificar datos aleatorios
    if (length(random_data) == 0) {
        print "⚠ ADVERTENCIA en línea " NR ": Datos aleatorios vacíos"
        warnings++
    }
    
    # Verificar longitud de datos aleatorios
    if (length(random_data) > 255) {
        print "✗ ERROR en línea " NR ": Datos aleatorios demasiado largos (" length(random_data) " caracteres)"
        errors++
    }
}

END {
    print ""
    print "=== Resumen de Verificación ==="
    print "Total de registros procesados: " total_records
    print "Errores encontrados: " errors
    print "Advertencias: " warnings
    
    if (total_records > 0) {
        print ""
        print "=== Análisis de IDs ==="
        print "Rango de IDs: " min_id " - " max_id
        print "IDs únicos encontrados: " length(ids_seen)
        print "Total esperado de IDs: " total_records
        
        # Verificar continuidad de IDs
        missing_ids = 0
        for (i = min_id; i <= max_id; i++) {
            if (!(i in ids_seen)) {
                if (missing_ids == 0) {
                    print "✗ IDs faltantes detectados:"
                }
                if (missing_ids < 10) {  # Mostrar solo los primeros 10
                    print "  ID faltante: " i
                }
                missing_ids++
            }
        }
        
        if (missing_ids > 0) {
            print "  Total de IDs faltantes: " missing_ids
            errors++
        } else {
            print "✓ Secuencia de IDs correcta (sin saltos)"
        }
        
        print ""
        print "=== Análisis de Procesos ==="
        print "Procesos generadores únicos: " length(process_ids)
        
        # Mostrar distribución de registros por proceso
        print "Distribución de registros por proceso:"
        for (pid in process_ids) {
            printf "  PID %s: %d registros (%.1f%%)\n", pid, process_ids[pid], (process_ids[pid] / total_records * 100)
        }
        
        print ""
        print "=== Análisis de Timestamps ==="
        printf "Rango de timestamps: %d - %d\n", min_timestamp, max_timestamp
        
        if (max_timestamp > min_timestamp) {
            printf "Duración total: %d segundos\n", (max_timestamp - min_timestamp)
        }
        
        # Verificar timestamps únicos vs repetidos
        unique_timestamps = length(timestamps)
        printf "Timestamps únicos: %d de %d (%.1f%%)\n", unique_timestamps, total_records, (unique_timestamps / total_records * 100)
        
        if (unique_timestamps < total_records) {
            print "✓ Múltiples registros por segundo (concurrencia detectada)"
        }
    }
    
    print ""
    print "=== Resultado Final ==="
    
    if (errors == 0 && header_found) {
        print "✅ VERIFICACIÓN EXITOSA - El archivo CSV cumple con todos los requisitos"
        exit 0
    } else {
        print "❌ VERIFICACIÓN FALLIDA - Se encontraron " errors " errores"
        if (!header_found) {
            print "   - Cabecera faltante o incorrecta"
        }
        exit 1
    }
}