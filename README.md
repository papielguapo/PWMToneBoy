# PWM TONE BOY

Plugin de dinámica y color PWM/DAC, C++20 y JUCE 8.0.4. Formatos Windows x64 VST3 y Standalone. Todos los controles de audio tienen automatización y persistencia mediante APVTS.

## Compilar

Requisitos: CMake 3.22+, un compilador C++20 y los componentes de desarrollo C++/Windows SDK de Visual Studio. JUCE se busca como paquete local versión 8 y, si no está disponible, FetchContent descarga la etiqueta 8.0.4. En esta entrega se utilizó JUCE 8.0.4 y Visual Studio 2026 x64.

```powershell
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release --parallel
ctest --test-dir build -C Release --output-on-failure
```

Visual Studio utiliza `--config Release`; `CMAKE_BUILD_TYPE` se aplica a generadores de configuración única. Las compilaciones Release utilizan `/O2 /fp:fast` en MSVC y `-O3` en GCC/Clang.

Resultados:

* `build/PWMToneBoy_artefacts/Release/VST3/PWM TONE BOY.vst3` (copiar la carpeta completa del bundle al directorio VST3 elegido).
* `build/PWMToneBoy_artefacts/Release/Standalone/PWM TONE BOY.exe`.

La aplicación y el plugin no se instalan automáticamente. El árbol `build/_deps/juce-src` contiene el código y la licencia de JUCE descargados por CMake. Para reconstruir desde el ZIP de fuentes, se vuelven a descargar las dependencias.

## Uso

Arrastrar la esquina inferior derecha redimensiona toda la interfaz entre el 80% y el 200%, conservando proporciones y coordenadas de interacción. El tamaño de referencia del plugin es 420 × 680; el Standalone añade 32 píxeles para los controles de prueba. Se recuerda el ancho junto con el estado del plugin. La escala DPI del sistema se suma a esta escala lógica.

Arrastrar una perilla cambia su parámetro; doble clic recupera el valor por defecto. Las perillas B/RATIO y A/DECAY tienen posiciones discretas. Las ruedas del ratón están deshabilitadas para evitar modificaciones accidentales. Mantener el ratón sobre un control muestra ayuda y el arrastre muestra su valor.

Pulsar la LCD alterna FFT, triodo/filtro de 18 Hz, rodilla/PDR y DAC/PWM. Los medidores IN/GR/OUT permanecen visibles. LINK controla el enlace estéreo; el Auto-Gain permanece siempre activo en la señal procesada. PHONES conmuta a los doce códigos del DAC raw y enciende el anillo verde.

En Standalone, elegir Drums, 808 Bass o Chiptune produce un bucle interno. WAV/AIFF abre un archivo y lo reproduce en bucle. `Audio input` vuelve a la entrada del dispositivo. El menú Options del contenedor JUCE permite seleccionar el dispositivo de audio; JUCE silencia inicialmente la entrada para evitar realimentación, sin silenciar los bucles internos.

## Parámetros

| ID | Rango / opciones | Inicial |
|---|---|---|
| threshold | −42 a 0 dB, 0.1 dB | −18 dB |
| ratioIndex | 1, 2, 3, 5, 10, 20, INF (100 internamente) | 3:1 |
| decayIndex | 100, 200, 400, 800, 1600, 3200 ms | 200 ms |
| clockRate | 4–250 kHz | 120 kHz |
| dacCrush | 0–100% | 42% |
| eqIn | Reconstrucción 11.2 kHz / 20 kHz | Sí |
| hpfIn | HPF del sidechain a 90 Hz | No |
| linkAutoGain | 85% de enlace estéreo | Sí |
| bypass | Bypass con compensación de latencia | No |
| pad | Entrada ×0.25 | No |
| kiMode | Segundo armónico ligado a GR | Sí |
| pdaMode | Ataque 0.35–2.2 ms | Sí |
| kWeightSc | Detector híbrido con preénfasis | Sí |
| phonesRawMode | DAC de doce códigos | No |

## Implementación y decisiones de DSP

La cadena se procesa muestra a muestra a 4× mediante oversampling IIR de JUCE con latencia entera declarada al host. Detector, PDA, PDR de doble envolvente, rodilla suave y enlace preceden al triodo asimétrico, sample-and-hold y DAC. Siguen la reconstrucción, los dos HPF de 18 Hz con Q 0.54119610/1.30656296, el peaking de 52 Hz y el techo Sapphire. No hay generador de ruido ni dither.

El Auto-Gain sigue `(-threshold * (1 - 1/ratio)) * 0.54` dB. Los controles y conmutadores interpolan durante 20 ms para reducir discontinuidades. El bypass cruza a una señal seca retrasada exactamente la latencia declarada, manteniendo el motor activo para evitar estados obsoletos al volver.

Se resta la polarización de entrada cero del triodo para evitar una fuente de DC independiente. El DAC utiliza códigos asimétricos mid-tread, incluyendo cero exacto: 16 códigos de −8/8 a +7/8, o 12 códigos de −6/6 a +5/6. La mezcla de cuantización depende de la puerta cuadrática para conservar colas suaves. En PHONES la mezcla raw pasa al 100%, conservando esa puerta y los filtros de protección. A frecuencias de reloj superiores a la frecuencia interna de proceso, el sample-and-hold actualiza en cada muestra interna; un reloj más rápido equivale al límite transparente de esta implementación a 4×.

La telemetría usa una cola SPSC acotada sin bloqueo; la FFT y los gráficos se calculan en el hilo de la interfaz a 60 Hz. No se reservan buffers ni se calculan FFT en el callback DSP. El reproductor opcional de archivos utiliza el transporte de JUCE.

La skin se dibuja con vectores a partir de las medidas y colores suministrados. Se usa Arial Black/Arial con fallback del sistema para las letras; no se ha suministrado una imagen de referencia ni la fuente Inter Black, por lo que no se afirma equivalencia visual píxel a píxel. Tampoco se afirma emulación medida de aparatos analógicos originales.

## Verificación

`PWMToneBoyChecks` verifica los parámetros, rodilla monotónica y continua, silencio exacto desde reposo, audio finito y acotado, cambios de conmutadores, frecuencias 32/44.1/48/96/192 kHz, bloques de 1 a 4097 muestras, bypass alineado, mono/estéreo, reset, estado inválido y restauración. También renderiza el editor en 336, 420, 630 y 840 píxeles de ancho.

```powershell
& 'build/PWMToneBoyChecks_artefacts/Release/PWMToneBoyChecks.exe' QA
```

Las pruebas automatizadas y las capturas reducen el riesgo de fallos; la compatibilidad final debe comprobarse en el DAW y hardware de destino.

Resultado de esta entrega (13 de noviembre de 2026): compilación Release x64 de ambos formatos, 3.848 comprobaciones con cero fallos, latencia de 6 muestras y validación VST3 con pluginval al nivel 10. Se comprobó la apertura del Standalone. Las capturas de `QA/` muestran las escalas 80%, 100%, 150% y 200%; las pruebas también verifican que el área de interacción de la perilla principal coincide con su dibujo en las cuatro escalas. Los registros completos de pluginval y las comprobaciones están en `QA/`.
