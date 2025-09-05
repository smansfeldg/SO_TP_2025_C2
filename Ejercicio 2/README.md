### **Propuesta de Proyecto: Servicio de Base de Datos Transaccional Cliente-Servidor**

#### **1. Resumen Ejecutivo **
Este proyecto detalla el diseño e implementación de un sistema **cliente-servidor** robusto, conforme a los requisitos del **Ejercicio 2** de la guía. Se desarrollará en **lenguaje C** una aplicación de red que utiliza **sockets TCP/IP** para permitir el acceso remoto y concurrente a un conjunto de datos almacenado en un archivo CSV (generado en el Ejercicio 1). El servidor será capaz de gestionar múltiples clientes de forma simultánea e implementará un mecanismo de **transacciones con bloqueo exclusivo** para garantizar la consistencia e integridad de los datos durante las operaciones de modificación. El sistema se diseñará para ser resiliente ante fallos y configurable mediante parámetros de línea de comandos.

***

#### **2. Objetivos Específicos **
* **Desarrollar una arquitectura cliente-servidor** funcional utilizando la API de sockets para comunicación a través de una red TCP/IP.
* **Implementar un servidor concurrente** capaz de atender hasta N clientes de forma simultánea y mantener M peticiones en cola, con N y M configurables al inicio.
* **Diseñar un protocolo de comunicación** basado en texto para que los clientes puedan realizar consultas (búsquedas, filtros) y operaciones DML (alta, baja, modificación) sobre los datos.
* **Construir un sistema de transacciones** que permita a un cliente obtener un bloqueo (lock) exclusivo sobre el archivo de datos para realizar múltiples modificaciones de forma atómica.
* **Asegurar la robustez del sistema**, manejando correctamente cierres inesperados de conexiones y garantizando que el servidor permanezca operativo a la espera de nuevos clientes.
* **Garantizar la correcta gestión de los recursos del sistema**, liberando sockets, archivos temporales y otros descriptores al finalizar la ejecución.

***

#### **3. Arquitectura y Diseño del Sistema **
El sistema se dividirá en dos componentes principales que se comunican a través de la red.

##### **Componentes del Sistema**

* **Servidor (`servidor`):**
    * **Inicio y Configuración:** Se ejecutará recibiendo por parámetro o archivo de configuración la **dirección IP/hostname y el puerto** de escucha, el número máximo de clientes concurrentes y el tamaño de la cola de espera. Validará estrictamente estos parámetros.
    * **Gestión de Conexiones:** Utilizará un modelo de concurrencia (ej. `fork()` por cada cliente) para aislar y gestionar cada conexión de cliente en un proceso independiente.
    * **Lógica de Datos:** Al iniciar, cargará en memoria el contenido del archivo CSV para optimizar las operaciones de consulta. Las modificaciones se persistirán en el archivo tras un `COMMIT` exitoso.
    * **Manejo de Transacciones:**
        1.  Al recibir una petición `BEGIN TRANSACTION`, activará un **lock global** (implementado, por ejemplo, con un semáforo o un archivo de bloqueo).
        2.  Mientras el lock esté activo, todas las peticiones de otros clientes (tanto de consulta como de modificación) serán rechazadas con un mensaje de error apropiado, indicando que deben reintentar más tarde.
        3.  Al recibir `COMMIT TRANSACTION`, aplicará los cambios al archivo CSV y liberará el lock, permitiendo que otros clientes puedan operar.
    * **Persistencia:** Permanecerá en ejecución continua, listo para aceptar nuevas conexiones incluso después de que todos los clientes actuales se hayan desconectado.

* **Cliente (`cliente`):**
    * **Modo de Operación:** Será una aplicación de consola **interactiva**, que se conectará al servidor especificando su IP y puerto.
    * **Interfaz de Usuario:** Presentará un *prompt* al usuario, permitiéndole escribir y enviar comandos al servidor de forma manual.
    * **Comunicación:** Enviará las peticiones del usuario al servidor, esperará la respuesta y la mostrará en pantalla.
    * **Ciclo de Vida:** Mantendrá la conexión activa hasta que el usuario introduzca un comando específico para salir (ej. `EXIT`).

##### **Protocolo de Comunicación Propuesto**
Se definirá un protocolo de texto simple, donde cada comando es una línea de texto.
* **Consultas:** `GET ID <id>`, `FIND <columna> <operador> <valor>`
* **Modificaciones:** `INSERT <datos>`, `DELETE ID <id>`, `UPDATE ID <id> SET <columna>=<valor>`
* **Transacciones:** `BEGIN TRANSACTION`, `COMMIT TRANSACTION`
* **Control:** `EXIT`

***

#### **4. Plan de Entrega y Verificación **

* **Código Fuente:** Se entregará el código C completo para `servidor` y `cliente`, junto con un **`Makefile`** actualizado que permita compilar ambos ejecutables.
* **Validación de Parámetros:** Ambos programas mostrarán un mensaje de ayuda detallado si los argumentos de línea de comandos son incorrectos o no se proporcionan.
* **Documentación de Monitoreo:** Se incluirá un informe con evidencia capturada mediante las herramientas `netstat`, `lsof`, `ps` y `htop`. Estas capturas demostrarán:
    * La creación de sockets y el estado de las conexiones.
    * La gestión de múltiples procesos/hilos para clientes concurrentes.
    * El bloqueo efectivo de clientes durante una transacción activa.
* **Empaquetado Final:** El proyecto se entregará en un único archivo comprimido **ZIP** que contendrá todo el material solicitado.