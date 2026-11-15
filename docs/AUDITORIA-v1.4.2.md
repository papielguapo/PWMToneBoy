# Auditoría de PWM TONE BOY v1.4.2

Fecha: **15 de noviembre de 2026**. Autor del plugin: **PAPIELGUAPO**. Código de audio/GUI auditado: commit `dbf4269`; esta revisión actualiza documentación, sin modificar el ejecutable.

## Resultado

La regresión repetida hoy terminó con **3.935 aserciones y cero fallos**, sin exportar capturas. La ejecución anterior con capturas sumó **3.940**, también sin fallos. Son aserciones, muchas de ellas puntos de barridos, no miles de escenarios independientes. Se volvió a ejecutar pluginval nivel 10, semilla 1472 y timeout 60000 ms; el registro se incluye en el paquete.

**Hay un defecto pendiente del analizador a tasas altas**, descrito abajo. Que pasen las pruebas existentes no implica que todos los comportamientos de la interfaz estén cubiertos.

SHA-256 del módulo VST3 auditado: `26e02cb3ac860ead1a6dd53ffd46b400d11586119efa2cf98bd9055afda7983f`.

## Alcance

Revisión de los fuentes propios del procesador, parámetros/estado, editor, LCD, estimadores H1/THD y worker armónico. Reejecución del banco DSP/GUI y validación del VST3. No se auditó JUCE línea por línea ni se realizaron ASan/TSan, evaluación de seguridad integral o pruebas en todos los DAWs. No se comparó con hardware Crane Song.

## Hallazgos

### [P2 · pendiente] El consumidor de telemetría no alcanza 176,4/192 kHz

La [LCD](https://github.com/papielguapo/PWMToneBoy/blob/dbf4269/Source/DotMatrixLCDComponent.h#L13) se actualiza a 30 Hz y [lee como máximo 4096 frames por llamada](https://github.com/papielguapo/PWMToneBoy/blob/dbf4269/Source/DotMatrixLCDComponent.h#L168). El [procesador](https://github.com/papielguapo/PWMToneBoy/blob/dbf4269/Source/PluginProcessor.h#L98) tiene una cola de 8192 posiciones, con 8191 utilizables. Por tanto, el consumo máximo nominal es **4096 × 30 = 122.880 muestras/s**. A 192 kHz se producen unas 6400 muestras entre llamadas y solo pueden retirarse 4096: la cola acaba desbordándose incluso con un timer ideal.

Al detectar el desbordamiento, la LCD reinicia la ventana armónica y la transferencia H1. Esto puede impedir que THD o la curva observada de A se estabilicen. **Se pierden diagnósticos, no muestras del audio del host:** la cola es una ruta de observación independiente. El motor puede pasar la regresión DSP a 192 kHz y, a la vez, existir este defecto de visualización.

Reproducción analítica: simular 300 intervalos a 30 Hz, capacidad útil 8191, producción según sample rate y lectura máxima 4096. El CSV de QA `audit-telemetry-budget.csv` da 0 pérdidas a 44,1/48/96 kHz; 531.105 a 176,4 kHz y 687.105 a 192 kHz en diez segundos. **Es una simulación del presupuesto de la cola, no una medición capturada en un DAW.** Un timer real puede retrasarse y causar pérdidas también a tasas inferiores.

Corrección propuesta: drenar suficiente telemetría según la tasa o diezmar con filtrado y adaptar las tasas de los analizadores; revisar capacidad y coste de interfaz. Debe probarse el timer real, no solo llamadas manuales frecuentes a refreshDiagnostics. **No se ha aplicado esa corrección en esta auditoría.** Para evaluar los analizadores actuales, usa 44,1/48/96 kHz o un analizador externo si trabajas a tasas superiores; no se garantiza ausencia de pérdidas si la interfaz se bloquea.

### [Documentación · corregido] La guía todavía indicaba 60 Hz

El timer de la LCD recuperada es de **30 Hz**, no 60 Hz. Se corrigen esas referencias. El cálculo pesado de ajuste armónico está en el worker; las FFT y la estimación H1 siguen en el hilo de interfaz. No se presenta toda la analítica como trabajo de fondo.

### [Documentación · corregido] Identificación del selector inferior

Se explicita en README y guía: **A = ANALOG / analógico**, a la izquierda; **D = DIGITAL / digital**, a la derecha. Es el selector horizontal inferior y no la etiqueta A — DECAY de la perilla superior. D no es bypass ni una ruta sin filtros.

## Lo comprobado y sus límites

- D omite la rama de contorno, saturación y deriva de A. KI es independiente; con KI apagado esa etapa es lineal, pero compresión, filtros y protección de picos permanecen.
- A modifica realmente filtros y ganancia de forma leve y variable. La transferencia observada se calcula desde entrada/salida; las bandas sin excitación/coherencia suficientes regresan al modelo. Los piquitos no prueban por sí solos mayor realismo analógico.
- No hay generador de hiss, hum, dither o ruido aleatorio. La entrada cero desde reposo produce cero exacto. Una señal real puede producir armónicos, modulación y bandas laterales en A.
- Las referencias se mantienen: D a 1 kHz/0.2 pico/KI OFF en el ensayo sin compresión dio THD −146,754 dB; KI ON aproximadamente −62,028 dB con H2 dominante. Son resultados numéricos de un ensayo concreto, no especificaciones universales ni rendimiento de hardware.
- Variación real de A, sin editor: 0,0460415 dB pico a pico a 64 Hz; 0,00986953 dB a 1 kHz; 0,028901 dB a 8 kHz. D permanece estable en esos ensayos. Condiciones completas en la [auditoría del motor v1.4.0](AUDITORIA-v1.4.0.md).
- Se ejercitaron silencio, mono/estéreo, entradas NaN/Inf/extremas y recuperación, tamaños de bloque, estado, A/D y parámetros, referencias PDA/H-SC, análisis de tono/música y render de seis vistas a varios tamaños. No constituye una prueba de larga duración del timer del editor a todas las tasas.
- El worker usa entrega/recogida con try-lock en la interfaz y no bloquea el callback de audio. Su cierre espera a terminar el cálculo en curso; no hay análisis de carrera mediante TSan.
- La protección de picos está **antes de OUTPUT**. Subir OUTPUT puede superar 0 dBFS. No es una garantía true-peak ni un limitador de entrega final.
- Auto-Gain es una fórmula dependiente de umbral y ratio, no igualación perceptiva ni normalización LUFS. Es obligatorio comparar a un nivel parecido para juzgar el efecto.
- SELECT, DAC y ProSound siguen sin efecto de degradación en audio. No hay EXP, UP, release continuo ni entrada numérica por doble clic en esta versión.

## Reproducir

```powershell
cmake --build build --config Release --target PWMToneBoyChecks
& 'build/PWMToneBoyChecks_artefacts/Release/PWMToneBoyChecks.exe'
# Para exportar las capturas y añadir las aserciones de exportación:
& 'build/PWMToneBoyChecks_artefacts/Release/PWMToneBoyChecks.exe' QA
```

Los ZIP actualizados incluyen los registros de la regresión, pluginval y el CSV de presupuesto de telemetría. El CSV no sustituye una prueba temporal en host.
