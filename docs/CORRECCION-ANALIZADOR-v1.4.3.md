# Corrección del analizador — v1.4.3

**Fecha: 15 de noviembre de 2026.** Resuelve el hallazgo P2 de la [auditoría v1.4.2](AUDITORIA-v1.4.2.md). Conserva los 16 parámetros, el audio de A/D, el título PWM y los controles por pasos; sin EXP ni UP.

## Problema y corrección

La LCD retiraba 4096 frames a 30 Hz: máximo nominal 122.880 frames/s. A 176,4/192 kHz no podía seguir al productor y reiniciaba sus estimadores por desbordamiento.

La cola y el buffer lector pasan a **65.536 posiciones**. La LCD retira todo lo disponible en cada refresco, hasta ese límite, sin el cuello de botella de 4096. Ambos buffers se reservan en el heap al construir procesador/editor, fuera de processBlock y del timer. Su carga conjunta es aproximadamente 3,5 MiB, más el resto de buffers ya existentes. No hay reservas nuevas en el callback de audio ni al refrescar diagnósticos.

El código de procesamiento de muestras de PluginProcessor.cpp no cambia. Solo se modifica el almacenamiento de telemetría en el encabezado y su consumo en la LCD. Latencia: **6 muestras**, como antes.

## Pruebas antes/después

La nueva prueba produce una cantidad de audio equivalente a 1/30 s entre consumos, durante dos fases (banda ancha en A y seno en D), omitiendo dos refrescos consecutivos para simular una pausa breve de GUI. Mantiene el audio de cada bloque; no sustituye la cola por una simulación matemática.

| Tasa | Antes: eventos de desbordamiento | Después | Resultado después |
|---|---:|---:|---|
| 176.400 Hz | 263 | 0 | Backlog vaciado, transferencia A y tono válidos |
| 192.000 Hz | 324 | 0 | Backlog vaciado, transferencia A y tono válidos |

El contador registra **eventos de escritura con pérdidas**, no número de muestras perdidas. La versión anterior produjo ocho fallos en las aserciones de estas pruebas; la corregida supera esas mismas comprobaciones.

Se añadió una prueba del **timer JUCE real de la LCD**: un hilo alimenta el procesador con bloques de audio a 192 kHz según reloj, mientras el hilo principal atiende el bucle de mensajes. Un callback detiene la prueba tras aproximadamente dos segundos. Resultado de esta ejecución: **361675 frames producidos, 0 desbordamientos y 157 bandas observadas válidas**. No usa un dispositivo físico ni acredita el rendimiento en todos los hosts.

**3.953 aserciones sin fallos**, incluidas las exportaciones de capturas; pluginval nivel 10, semilla 1472, aprobado. Se mantienen las referencias de D/KI/PDA/H-SC, variación de A sin editor, silencio, estado y particiones de bloque. Los registros antes/después y pluginval se incluyen en el ZIP.

## Límites

La cola sigue siendo finita: pausas de interfaz suficientemente largas, bloques extraordinariamente grandes o una carga extrema pueden descartar diagnósticos. El mecanismo de reinicio por pérdida se mantiene; no se descarta audio del host por esta cola. La capacidad nominal de consumo supera 768 kHz, pero la nueva prueba temporal se realizó a **192 kHz**; no certifica todos los valores aceptados ni todos los escenarios.

No cambia la protección de picos, que sigue antes de OUTPUT. No se añade ruido, modulación al audio, EXP/UP ni controles continuos. Esta corrección resuelve el cuello de botella reproducido; no equivale a certificar ausencia de cualquier otro defecto.
