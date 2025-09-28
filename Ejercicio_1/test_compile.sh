# Test de compilación sin warnings

echo "Compilando con verificación de warnings..."
echo "=========================================="

# Limpiar compilación previa
make clean

# Compilar con flags adicionales para detectar más warnings
echo "Compilando coordinador..."
gcc -Wall -Wextra -std=c99 -pedantic -D_GNU_SOURCE -Iinclude -Werror -c src/coordinador.c -o obj/coordinador.o

echo "Compilando shared..."
gcc -Wall -Wextra -std=c99 -pedantic -D_GNU_SOURCE -Iinclude -Werror -c src/shared.c -o obj/shared.o

echo "Compilando generador..."
gcc -Wall -Wextra -std=c99 -pedantic -D_GNU_SOURCE -Iinclude -Werror -c src/generador.c -o obj/generador.o

if [ $? -eq 0 ]; then
    echo "✅ Compilación exitosa sin warnings"
    
    # Enlazar ejecutables
    gcc -Wall -Wextra -std=c99 -pedantic -D_GNU_SOURCE -o coordinador obj/coordinador.o obj/shared.o
    gcc -Wall -Wextra -std=c99 -pedantic -D_GNU_SOURCE -o generador obj/generador.o obj/shared.o
    
    echo "✅ Enlazado completado"
else
    echo "❌ Errores de compilación detectados"
fi