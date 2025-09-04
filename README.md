### **Propuesta de Proyecto: Generador de Datos Concurrente con Memoria Compartida**

#### **1. Resumen Ejecutivo**
[cite_start]Este proyecto se centra en el desarrollo de un sistema de generación de datos en paralelo, de acuerdo a los requisitos estipulados en el **Ejercicio 1** de la guía de trabajos prácticos[cite: 18]. Se implementará en **lenguaje C** una aplicación multi-proceso compuesta por un **proceso coordinador** y múltiples **procesos generadores**. La comunicación y sincronización entre ellos se realizará eficientemente a través de **memoria compartida (SHM)** y semáforos. El objetivo final es producir un archivo CSV con un volumen de datos configurable, garantizando la integridad de los registros y una gestión de recursos del sistema operativo limpia y controlada.

***

#### **2. Objetivos Específicos**
* [cite_start]**Implementar un sistema multi-proceso** para la generación de datos en paralelo, donde un proceso padre (coordinador) orquesta la creación y finalización de N procesos hijos (generadores)[cite: 25, 26].
* [cite_start]**Utilizar Memoria Compartida (SHM)** como el canal principal de Inter-Process Communication (IPC) para que los generadores envíen registros de datos al coordinador[cite: 31].
* [cite_start]**Garantizar la integridad y unicidad de los datos**, asegurando que los identificadores (ID) en el archivo final sean estrictamente correlativos, sin duplicados ni saltos en la numeración[cite: 43, 44].
* [cite_start]**Desarrollar un sistema robusto** que valide sus parámetros de entrada y gestione finalizaciones prematuras de manera controlada[cite: 45, 46].
* [cite_start]**Asegurar la correcta liberación de todos los recursos IPC** (memoria compartida, semáforos) al concluir la ejecución para no dejar residuos en el sistema[cite: 40, 47].

***

#### **3. Arquitectura y Diseño del Sistema**
La arquitectura se fundamenta en el patrón productor-consumidor, con los generadores como productores y el coordinador como el único consumidor.

##### **Componentes del Sistema**

* **Proceso Coordinador (`coordinador`):**
    * [cite_start]**Inicio:** Se ejecuta recibiendo por línea de comandos la cantidad total de registros a generar y el número de procesos generadores[cite: 28]. [cite_start]Valida que los parámetros sean correctos; de lo contrario, muestra un mensaje de ayuda y finaliza[cite: 45].
    * **Gestión de IPC:** Crea un segmento de memoria compartida para alojar un único registro a la vez y los semáforos necesarios para gestionar el acceso concurrente a la SHM y la asignación de IDs.
    * **Asignación de IDs:** Administra un contador global. [cite_start]Cuando un generador lo solicita, le entrega un bloque de **10 IDs válidos** para trabajar[cite: 29].
    * [cite_start]**Procesamiento:** Entra en un bucle donde espera a que un generador deposite un registro en la memoria compartida, lo lee y lo escribe en el archivo `datos_generados.csv`[cite: 32].
    * [cite_start]**Finalización:** Al alcanzar el total de registros, se encarga de terminar ordenadamente a los procesos hijos y ejecuta una rutina de limpieza para liberar todos los recursos IPC del sistema[cite: 47].

* **Proceso Generador (`generador`):**
    * **Ciclo de Vida:**
        1.  [cite_start]Solicita al coordinador un bloque de 10 IDs[cite: 29].
        2.  [cite_start]Para cada ID recibido, genera un registro con datos aleatorios[cite: 30]. La estructura del registro será, por ejemplo: `ID, ID_PROCESO, TIMESTAMP, DATO_ALEATORIO`.
        3.  [cite_start]Envía cada registro, **uno por vez**, al coordinador a través de la memoria compartida, utilizando semáforos para asegurar la exclusión mutua[cite: 31].
        4.  Repite el ciclo hasta que el coordinador le indique que no hay más IDs por asignar.

##### **Formato del Archivo de Salida (CSV)**
El archivo `datos_generados.csv` contendrá:
* [cite_start]Una primera fila con los nombres de las columnas (cabecera)[cite: 35].
* [cite_start]El **ID como primer campo obligatorio** en cada fila[cite: 35].
* [cite_start]Los registros aparecerán en el orden en que fueron recibidos por el coordinador, no necesariamente en orden numérico de ID[cite: 37].

***

#### **4. Plan de Entrega y Verificación**
* [cite_start]**Código Fuente:** El proyecto será desarrollado íntegramente en C y compilado con un **`Makefile`** provisto[cite: 10, 12]. [cite_start]No se incluirán binarios[cite: 13].
* **Lote de Prueba:** Se entregará un script de shell (`run_test.sh`) que ejecuta el sistema con parámetros de ejemplo. [cite_start]Además, se incluirá un script **`verify.awk`** que analizará el CSV resultante para verificar que los IDs sean correlativos y no existan duplicados, cumpliendo con el criterio de corrección[cite: 14, 42].
* [cite_start]**Documentación de Monitoreo:** Se adjuntará un breve informe con capturas de pantalla de herramientas como `ps`, `htop`, `vmstat` e `ipcs` para evidenciar la creación de procesos, el estado de la memoria compartida y la concurrencia del sistema[cite: 15, 39].
* [cite_start]**Empaquetado Final:** Todo el material (código fuente, makefile, scripts e informe) será entregado en un único archivo comprimido **ZIP**[cite: 16].