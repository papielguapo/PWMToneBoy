# Guía de uso — PWM TONE BOY v1.0.1

**Creado por PAPIELGUAPO · Windows x64 · VST3 + Standalone**

![Interfaz actual de PWM TONE BOY](images/PWM-TONE-BOY.png)

## Qué hace el plugin

PWM TONE BOY es un compresor estéreo con rodilla suave, recuperación con memoria del programa y color armónico KI opcional. Permite controlar la dinámica conservando una ruta de procesamiento lineal en la etapa de color cuando KI está apagado. Incluye protección dinámica de picos, filtros de reconstrucción y subgraves, compensación automática de ganancia y ajuste manual OUTPUT.

Es un diseño original inspirado en comportamientos del STC-8. No se ha calibrado contra un equipo Crane Song real y no se presenta como una réplica certificada. La etapa KI apagada es lineal; los filtros y la compresión siguen funcionando.

## Descargar e instalar

1. Descarga **PWM-TONE-BOY-Windows-x64.zip** en la [release v1.0.1](https://github.com/papielguapo/PWMToneBoy/releases/tag/v1.0.1).
2. Descomprime el ZIP. Copia la carpeta completa `VST3/PWM TONE BOY.vst3` a `C:\Program Files\Common Files\VST3`. Cierra los programas que estén usando el plugin antes de sustituirlo. Windows puede solicitar permisos de administrador.
3. Vuelve a escanear los plugins en tu DAW y carga **PWM TONE BOY**, fabricante **PAPIELGUAPO**.
4. Para usarlo sin DAW, abre `Standalone/PWM TONE BOY.exe`.

El ZIP también incluye esta guía, la fotografía de la interfaz y registros de pruebas. `PWMToneBoy-source.zip` contiene el proyecto CMake y las fuentes para recompilarlo.

## Cómo manejarlo

- **Arrastra una perilla** horizontal o verticalmente para cambiar su valor. Durante el ajuste aparece el valor numérico.
- **Doble clic en una perilla:** vuelve a su valor de fábrica. No reinicia los demás controles.
- **Clic en una palanca o lámpara:** alterna activado/desactivado. Las seis lámparas se iluminan en blanco al estar activas; en BYPASS, una lámpara encendida significa que el procesamiento está puenteado.
- **Clic en la LCD:** cambia de pantalla, en el orden M0 → M1 → M2 → M3 → M0. Cambiar la pantalla no cambia el sonido.
- **Arrastra la esquina inferior derecha:** redimensiona del 80% al 200%, manteniendo las proporciones. El plugin recuerda el ancho con el estado del proyecto.
- Mantén el puntero sobre un control para ver su ayuda. La rueda del ratón no modifica las perillas, evitando cambios accidentales.

## Perillas

| Perilla | Qué hace | Rango y valor inicial |
|---|---|---|
| **PWM THRESHOLD** | Fija el umbral del compresor. Al bajarlo, una mayor parte de la señal entra en compresión. La rodilla hace que la transición alrededor del umbral sea gradual. El display inferior muestra dB. | −42 a 0 dB; inicial **−18 dB**. |
| **B — RATIO** | Define la intensidad de la compresión por encima de la rodilla. 1:1 desactiva la reducción del compresor, pero conserva filtros, KI si está activo, Auto-Gain según su fórmula y protección de picos. INF es una relación interna de 100:1. | **1:1, 2:1, 3:1, 5:1, 10:1, 20:1, INF**; inicial **3:1**. |
| **A — DECAY** | Ajusta la recuperación base de la dinámica. Valores cortos recuperan antes y largos mantienen más tiempo la reducción. El tiempo real también depende de la memoria de la compresión: no es un cronómetro fijo de vuelta a cero. | **100, 200, 400, 800 ms, 1.6 y 3.2 s**; inicial **200 ms**. |
| **SELECT** | Control de reloj PWM heredado. Conserva su posición y lectura, pero **no modifica el audio en v1.0.1**, porque sample-and-hold y DAC están desactivados. | 4–250 kHz; inicial **120 kHz**. |
| **OUTPUT** | Sube o baja el nivel procesado después de Auto-Gain y de la protección de picos. No modifica el umbral ni el ratio. Los cambios se suavizan durante 20 ms. Ganancias positivas pueden superar 0 dBFS después de la protección; comprueba el nivel de salida del DAW. En BYPASS se escucha la señal seca, sin OUTPUT. | **−24 a +12 dB**; inicial **0 dB**. |

## Palancas y botones

| Control | Activado | Desactivado | Inicial |
|---|---|---|---|
| **EQ IN** | Filtro de reconstrucción con corte de **11.2 kHz**, para una parte alta más contenida. | Cambia el corte a **20 kHz**; no elimina todo el filtrado del plugin. | Activado. |
| **HPF SC** | Filtra el detector con un paso alto Butterworth a **90 Hz**, reduciendo la influencia de los graves en la compresión. | El detector recibe también los graves sin ese paso alto. No es un corte directo de graves en la ruta de audio. | Desactivado. |
| **LINK** | Comparte el 85% de la ganancia del compresor y mantiene un 15% independiente por canal. La protección de picos se enlaza para cuidar la imagen estéreo. | Compresión y protección de picos independientes por canal. | Activado. |
| **BYPASS** | Escucha la señal seca con el retraso que compensa la latencia del plugin. La transición es suave y el motor continúa actualizando sus estados. BATTERY se apaga. | Escucha la señal procesada. | Desactivado. |
| **PAD** | Atenúa la entrada multiplicándola por **0.25**, aproximadamente **−12.04 dB**, antes del procesamiento. | Entrada sin esa atenuación. | Desactivado. |
| **KI** | Añade una curvatura par sutil, principalmente segundo armónico, dependiente de actividad y compresión. | El waveshaper es lineal; no quedan ni el antiguo logaritmo del semiciclo positivo ni una saturación permanente en esa etapa. | Activado. |
| **PDA** | Ante riesgo de sobrecarga, acorta el ataque nominal de **2.2 ms** hacia **0.35 ms**. Trabaja junto al control de picos. | Mantiene el ataque nominal sin ese acortamiento dinámico. | Activado. |
| **H-SC** | Activa preénfasis diferencial en el sidechain y mezcla de detección **65% pico + 35% RMS**. Cambia cómo reacciona el compresor; no es un ecualizador directo de la salida. | Usa detección de pico sin ese preénfasis híbrido. | Activado. |

## Auto-Gain y protección de picos

Auto-Gain está siempre activo en la ruta procesada y no tiene botón propio. Su compensación se calcula a partir de umbral y ratio: `(-threshold × (1 − 1/ratio)) × 0.54 dB`. No es una normalización a LUFS ni garantiza que el volumen percibido coincida con bypass. Usa OUTPUT para comparar a oído a un nivel similar.

La protección de picos actúa cerca de −0.45 dBFS antes de OUTPUT, con retención de 0.5 ms y recuperación de 60 ms. Después hay una curva de seguridad que permanece lineal hasta ±0.98. Al final, OUTPUT permite ajustar el volumen manualmente.

El filtrado fijo incluye un paso alto de cuarto orden a **18 Hz** y un contorno de **+0.85 dB a 52 Hz**. No hay generador de ruido ni dither; desde reposo, una entrada de cero produce salida de cero exacto. La latencia declarada es de **6 muestras**.

## LCD: cuatro pantallas con un clic

La interfaz actualiza sus diagnósticos a 60 Hz. Los títulos históricos permanecen en la skin; describimos aquí qué muestran realmente en esta versión.

| Pantalla | Qué ves y cómo interpretarlo |
|---|---|
| **M0 — FFT SPECTRUM** | Analizador de 48 barras con retención visual de picos. Muestra el espectro de la ruta procesada interna, **antes de OUTPUT y de la mezcla de bypass**. Sirve para ver la distribución de frecuencias; no sustituye un analizador de distorsión calibrado. |
| **M1 — KI TRIODE+18Hz** | Traza punteada antes del filtro infrasónico y traza sólida después del paso alto de 18 Hz, ambas antes del contorno de 52 Hz y de la protección final. Permite observar el recentrado tras la etapa de color. **KI:%** es el coeficiente interno de color escalado, no un porcentaje de THD. **G:%** indica actividad de la envolvente cuadrática usada para modular el color; no es un botón de puerta de ruido ni un umbral editable. El nombre TRIODE es histórico, no una afirmación de modelado de válvulas. |
| **M2 — STC-8M KNEE+PDR** | A la izquierda, curva estática de compresión según THRESHOLD/RATIO. A la derecha, historial de reducción de ganancia. Las lecturas indican PDA y H-SC activos/inactivos. **PDR:%** representa la memoria interna del programa: una cifra mayor implica mayor contribución de recuperación prolongada. La curva estática no muestra por sí sola el limitador ni la evolución temporal completa. |
| **M3 — 4BIT DAC+PWM** | Vista de diagnóstico heredada: arriba se dibuja el punto interno de la antigua etapa DAC y abajo un tren PWM ilustrativo. **El DAC y ProSound están desactivados en audio**: no debes esperar escalones de cuantización ni que SELECT cambie el sonido. Las leyendas de DAC/RAW/% pueden reflejar parámetros antiguos conservados por compatibilidad, aunque el DSP los ignore. El tren PWM es una visualización, no una señal adicional enviada al DAW. |

### Medidores inferiores, siempre visibles

| Medidor | Lectura |
|---|---|
| **IN** | Pico de la entrada procesada después de PAD, expresado en dBFS. |
| **GR** | Reducción máxima de ganancia observada en el bloque, incluyendo compresor y protección de picos, en dB positivos. En bypass el motor sigue trabajando y este indicador puede seguir mostrando reducción interna. |
| **OUT** | Pico de la salida entregada, después de OUTPUT y de la mezcla de bypass, en dBFS. |

Los medidores son de pico por bloque, no integradores LUFS ni medidores certificados de true peak. `-INF` es la indicación visual del límite inferior de la escala (aproximadamente −100 dB), no una medición de precisión ilimitada. La regla superior es una referencia gráfica y los valores numéricos en dB son la lectura útil para niveles.

## Zonas decorativas y funciones retiradas

La pastilla superior **OFF•ON** es decorativa: no es un interruptor. El bypass real es el botón BYPASS. **BATTERY** es un indicador ligado a bypass; no se pulsa. La placa inferior vacía, los tornillos, ranuras y el altavoz dibujado no tienen acción al hacer clic.

El jack PHONES/ProSound fue retirado de la interfaz. No existe un botón oculto para activarlo. Los parámetros antiguos `phonesRawMode` y `dacCrush` se conservan para compatibilidad con sesiones y automatización, pero no pueden reactivar esas etapas de audio. OUTPUT es un parámetro nuevo e independiente.

## Standalone: probar sin DAW

En la barra superior:

- **Audio input:** procesa la entrada del dispositivo elegido en Options. El contenedor JUCE silencia inicialmente la entrada para evitar realimentación; la configuración de audio permite ajustarla.
- **Drums loop / 808 Bass loop / Chiptune loop:** selecciona un bucle sintetizado interno para probar el compresor de inmediato.
- **WAV / AIFF:** abre el selector de archivos. Un archivo válido comienza a reproducirse en bucle y selecciona File loop. Cancelar el selector no carga un archivo nuevo.
- **File loop:** reproduce el archivo previamente cargado; primero debes cargar uno con WAV / AIFF.
- **Options**, en el contenedor del Standalone: permite acceder a la configuración de audio y a las funciones de estado que proporciona JUCE.

## Primer ajuste para escuchar el efecto

Empieza con ratio 3:1 y DECAY 200 ms. Baja THRESHOLD hasta ver unos pocos dB de GR; compara KI encendido y apagado. Prueba HPF SC si el bombo o el bajo dominan la reducción. Ajusta OUTPUT para comparar con bypass a un volumen parecido: Auto-Gain no garantiza igualdad perceptiva. Para recuperación más lenta, sube DECAY. Para una acción más fuerte, aumenta RATIO.

## Verificación y alcance

La versión binaria v1.0.1 pasó **3.858 comprobaciones automatizadas y pluginval nivel 10**. Se probaron silencio, estado, mono/estéreo, cambios de parámetros, distintas frecuencias de muestreo, bloques de 1–4097 muestras y cuatro tamaños del editor. Los registros se incluyen en QA. Son pruebas de estabilidad y comportamiento del software, no una certificación de equivalencia con hardware.
