# Auditoría de PWM TONE BOY v1.4.0

**Fecha:** 13 de noviembre de 2026. **Autor del plugin:** PAPIELGUAPO.

## Resultado y alcance

Compilación Windows x64 de VST3 y Standalone completada. **3.935 aserciones, cero fallos**, y pluginval nivel 10 aprobado con semilla 1472. El conteo incluye barridos de puntos de la curva: no significa 3.935 escenarios de host independientes.

Se revisaron los fuentes propios del procesador, parámetros/estado, editor, LookAndFeel y analizadores. Se probó DSP sin editor, referencias de D, PDA/H-SC, silencio, muestras inválidas, metadatos de preparación, particiones de bloque, navegación/render de GUI y ciclo de vida mediante pluginval. Los módulos de JUCE no se han auditado línea por línea.

**A ahora varía la ruta de audio de verdad.** La deriva se aplica a los coeficientes y a una ganancia muy leve del circuito A; no es una animación. D omite esa rama. La cuantización DAC y ProSound continúan desactivados.

## Hallazgos y correcciones

| ID | Prioridad | Hallazgo | Resolución / evidencia |
|---|---|---|---|
| F01 | Alta | Las muestras no finitas se descartaban después del upsampling. NaN/Inf podían contaminar estados IIR y la señal seca. | Validación por bits IEEE antes del oversampling y del delay seco. NaN/Inf se sustituyen por cero; magnitudes finitas extremas se limitan a ±16. Ensayo con NaN, ±Inf y máximo float: salida finita y recuperación de audio normal. Entradas finitas normales no cambian. |
| F02 | Media | Actualizar el contorno A con un índice relativo al bloque podía ligar su control al tamaño de bloque. | Contador persistente de 32 muestras internas. Render con bloques de 17 y 511: diferencia máxima medida **0** en A y D para el estímulo auditado. |
| F03 | Media | Una frecuencia de muestreo inválida o un tamaño anunciado enorme podían causar estados inválidos o reservas excesivas. | Tasa no finita/fuera de 8–768 kHz: fallback a 44.1 kHz. Buffer interno acotado a 32–65536 muestras; bloques mayores se procesan por fragmentos. Prueba con +Inf y máximo entero superada. |
| F04 | Media | La nueva deriva debía sobrevivir al reset del transporte/analizador para no reiniciar siempre la misma fase en cada sonda. | La fase permanece entre reset y prepare de la misma instancia. Ensayo de coeficientes tras reset y silencio exacto superado. Una instancia nueva inicia una secuencia nueva. |
| F05 | Media | El modelo de la LCD debía reflejar los coeficientes/gain realmente usados por A, no una curva nominal distinta. | Snapshot atómico del perfil aplicado y de su ganancia; el modelo y la estimación pasiva usan la ruta actual. Su lectura no modifica audio. |

Fuentes principales: [PluginProcessor.cpp](../Source/PluginProcessor.cpp), [PluginProcessor.h](../Source/PluginProcessor.h), [DotMatrixLCDComponent.h](../Source/DotMatrixLCDComponent.h), [AnalysisModels.h](../Source/AnalysisModels.h), [ProcessorChecks.cpp](../Tests/ProcessorChecks.cpp).

## Diseño de la variación de A

Es una **modulación lenta, deliberada y acotada de parámetros de audio**, no una identificación de componentes térmicos de un Crane Song real. Se usan dos fases de control que avanzan con las muestras, con velocidades base de 0.55 y 1.17 Hz que dependen levemente de la carga. No se añade una señal de ruido ni un oscilador a la salida de audio.

Límites nominales de la variación, además del contorno A existente:

- Pico de graves: ±0.025 dB como máximo.
- Pico de agudos: ±0.020 dB como máximo.
- Ganancia global de A: ±0.006 dB como máximo.
- Sintonía: hasta ±0.10% en graves y ±0.07% en agudos.

Los coeficientes pueden variar en reposo; una entrada cero con estados de audio en reposo sigue produciendo **cero exacto**. Las etapas asimétricas preceden al filtro de 18 Hz. La fase no se rebobina con reset/prepare de una instancia existente: A puede variar ligeramente entre reproducciones según su historia. D no recibe esa deriva. No avanza una simulación por reloj de pared mientras el host no procesa muestras.

### Medición independiente de la LCD

Procesadores nuevos, sin crear editor: 48 kHz, entrada seno de 0.2 pico (aprox. −13.98 dBFS), ratio 1:1, umbral 0 dB, KI apagado, EQ de reconstrucción a 20 kHz, PAD apagado y OUTPUT 0 dB. Dos segundos de asentamiento y cuatro segundos medidos con ventanas de 125 ms.

| Frecuencia | Variación pico a pico A | Residuo total A | Variación D | Residuo D |
|---|---:|---:|---:|---:|
| 64 Hz | 0.046041 dB | -55.04 dBc | <0.0001 dB | <-100 dBc |
| 1000 Hz | 0.009870 dB | -61.38 dBc | <0.0001 dB | <-100 dBc |
| 8000 Hz | 0.028901 dB | -57.76 dBc | <0.0001 dB | <-100 dBc |

**Residuo total no significa ruido añadido:** incluye armónicos, bandas laterales de modulación y error de ajuste al seno. La variación temporal real crea bandas laterales; es el compromiso del modo A. Los límites de las pruebas son específicos y no garantizan la misma cifra con cualquier señal o nivel. Los resultados extremadamente bajos de D reflejan el suelo numérico del ensayo, no una especificación de hardware.

Los datos están en `QA/actual-a-response.csv` y el registro completo en `QA/regression.txt` dentro del ZIP descargable. El código de `measureDrift` permite repetirlo sin abrir ninguna GUI.

## D, PDA y H-SC

Las referencias digitales anteriores se conservaron: KI OFF en el ensayo estable de 1 kHz/0.2 pico por debajo de −100 dB de THD; KI ON aproximadamente −62.03 dB, con segundo armónico dominante. En compresión a 1 kHz, 0.5 pico, ratio 3:1 y umbral −18 dB, las cuatro combinaciones PDA/H-SC con KI apagado dieron entre −121.30 y −122.73 dB de THD en este ensayo. Con KI encendido se comprobó H2 dominante.

Caso de regresión de 250 Hz a 0 dBFS, ratio 2:1, umbral 0 dB, DECAY 100 ms, KI/PDA/H-SC apagados: THD aproximada de −87.84 dB. Compresión transitoria y protección de picos pueden producir distorsión de control legítima: estas cifras no son universales.

Las correcciones de robustez afectan a entradas inválidas/extremas o a metadatos de host fuera de rango. No se ha añadido saturación, deriva, DAC ni generador de ruido en D.

## Tiempo real y estado

Los buffers del procesador y del oversampling se reservan en prepareToPlay. El nuevo control usa estado escalar y trabajo acotado. La estimación H1 y las FFT permanecen en el hilo de interfaz. La telemetría usa la cola SPSC existente y metadatos atómicos; no hay nuevos bloqueos de audio en el cambio.

La inspección del callback no sustituye a un trazador de reservas ni a una prueba de carga en todos los hosts. El reproductor opcional de archivos del Standalone conserva AudioTransportSource de JUCE, que puede usar sincronización interna. No se presenta ese reproductor como una implementación de streaming sin bloqueo.

IDs y parámetros se mantienen. Latencia declarada: 6 muestras. Se verificaron estado, rechazo de datos corruptos, mono/estéreo, tamaños de 1 a 4097 muestras y frecuencias de 32/44.1/48/96/192 kHz, además del control de metadatos inválidos. El rango aceptado hasta 768 kHz es una protección de entrada, no una certificación de prueba en todas las tasas.

## Comprobación de host y límites

PluginDoctor 2.2.6 cargó el VST3 instalado en Common Files; se verificó por la ruta del módulo cargado y se conmutó A en su editor, observando su respuesta externa. La exportación automática de datos del diálogo nativo se bloqueó; se cerró esa instancia de prueba. **No se atribuye al exportador de PluginDoctor ninguna cifra de la tabla:** las medidas cuantitativas proceden del ensayo directo del motor sin editor.

- No se comparó con hardware Crane Song ni se reconstruyó su circuito.
- No se ejecutaron ASan/TSan ni una auditoría de seguridad integral.
- No se probaron todos los DAWs, dispositivos físicos ni otros sistemas operativos.
- Si un host recrea una instancia por medición, la nueva instancia inicia la secuencia desde su fase inicial; promedios largos pueden ocultar variaciones pequeñas. La deriva es deliberadamente sutil.
- OUTPUT está después de la protección de picos: ganancia positiva puede superar 0 dBFS. No se añadió un limitador oculto para cambiar ese comportamiento.
- La respuesta H1 con señal de programa es una estimación de un sistema no lineal/variable, no un barrido lineal certificado. Bandas sin excitación o coherencia suficientes vuelven al modelo.

## Cómo reproducir

```powershell
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release --parallel
& 'build/PWMToneBoyChecks_artefacts/Release/PWMToneBoyChecks.exe' QA
```

Validación adicional utilizada: pluginval nivel 10, semilla 1472, timeout 60000 ms, con GUI habilitada. Se incluyen los registros en la descarga. Esta auditoría informa de lo inspeccionado y medido; no certifica ausencia absoluta de defectos.
