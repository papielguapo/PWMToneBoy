# PWM TONE BOY — PAPIELGUAPO

**v1.3.0 · A con respuesta observada; D digital limpio conservado.**

![PWM TONE BOY](docs/images/PWM-TONE-BOY.png)

- **[Descargar VST3 + Standalone — Windows x64](https://github.com/papielguapo/PWMToneBoy/releases/download/v1.3.0/PWM-TONE-BOY-Windows-x64.zip)**
- **[Guía completa en español](docs/GUIA-ES.md)**: cada perilla, botón, clic, pantalla e instalación.
- [Release v1.3.0](https://github.com/papielguapo/PWMToneBoy/releases/tag/v1.3.0)

## Qué cambia, solo en A

Linear Analysis sigue una estimación de la **transferencia real entrada/salida** en A, usando espectros cruzados H1. El movimiento procede de los datos del audio y de su procesamiento: no es un temblor dibujado con un oscilador ni ruido aleatorio.

![A: entrada/salida observada y modelo](docs/images/Analog-A.png)

La etiqueta **A I/O+MODEL** indica el método. Solo se admiten bandas excitadas y coherentes; las demás conservan el modelo. El estimador usa ventanas de 8192 muestras, suaviza espectros y requiere tres ventanas activas por banda. La información nueva llega aproximadamente hasta seis veces por segundo y la representación se suaviza a 60 Hz. Un seno estable no obliga a que la curva tiemble: no se inventa variación donde no la hay.

Esta es una estimación de transferencia con señal de programa, no un barrido lineal de laboratorio ni una medición certificada de un circuito Crane Song. La coherencia, la ventana y la no linealidad limitan la lectura. El DAC de 4 bits y ProSound siguen desactivados.

## D se mantiene limpio

**El motor de audio no cambia respecto a v1.2.1.** D conserva la ruta digital restaurada de v1.1.0. KI es independiente y mantiene su segundo armónico sutil; PDA y H-SC actúan sobre la dinámica/detección, sin un nuevo generador de distorsión añadido. Desactivar KI no desactiva el compresor ni los filtros principales.

La nueva respuesta observada no se aplica a la vista D. No se añade ruido, hiss, hum ni dither en ninguno de los modos; cero de entrada desde reposo mantiene cero exacto de salida. Las pruebas cubren PDA/H-SC en todas sus combinaciones, con KI OFF/ON, además de las referencias de D anteriores.

![D conservado](docs/images/Linear-Analysis.png)

## Funciones que se conservan

- THRESHOLD −42 a 0 dB; RATIO 1/2/3/5/10/20/100; DECAY 100–3200 ms.
- OUTPUT −24 a +12 dB, después de protección de picos. Auto-Gain siempre activo en la ruta procesada.
- EQ IN, HPF SC, LINK, BYPASS, PAD, KI, PDA, H-SC y selector A/D. A/D cruza en 40 ms.
- A aplica el contorno/saturación dependientes del nivel y la memoria existentes en v1.2.1; D omite esa etapa adicional.
- Seis visores LCD; Harmonic Analysis mantiene espectro con música y solo publica THD/THD+N con seno válido.
- Editor vectorial reescalable del 80% al 200%, con estado y automatización. Standalone con bucles y WAV/AIFF.
- SELECT y la vista DAC/PWM son heredados; la cuantización continúa desactivada aunque se automaticen parámetros antiguos.

![Análisis armónico](docs/images/Harmonic-Analysis.png)
![Espectro con música](docs/images/Harmonic-Music.png)

Diseño original inspirado en comportamiento analógico/STC-8; no emulación calibrada ni reconstrucción de su circuito. Se conservan fabricante PAPIELGUAPO, IDs, parámetros y latencia de 6 muestras.

## Compilar y verificar

C++20, CMake 3.22+, JUCE 8.0.4 mediante FetchContent si no se encuentra JUCE 8 local. En Windows, Visual Studio con C++ y Windows SDK.

```powershell
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release --parallel
ctest --test-dir build -C Release --output-on-failure
```

- `build/PWMToneBoy_artefacts/Release/VST3/PWM TONE BOY.vst3` (bundle completo).
- `build/PWMToneBoy_artefacts/Release/Standalone/PWM TONE BOY.exe`.

```powershell
& 'build/PWMToneBoyChecks_artefacts/Release/PWMToneBoyChecks.exe' QA
```

La estimación y las FFT se ejecutan en la interfaz sobre la cola SPSC existente. No se añaden reservas ni análisis al callback de audio. Los ZIP incluyen guía, fotos y QA. La licencia JUCE se encuentra en `build/_deps/juce-src` tras configurar.


**Resultado v1.3.0:** 3.917 comprobaciones con cero fallos y pluginval nivel 10 superado (semilla 1472). D con KI apagado mantuvo THD inferior a −121 dB en las cuatro combinaciones PDA/H-SC del ensayo estable de 1 kHz, ratio 3:1 y umbral −18 dB. Con KI encendido se verifica segundo armónico dominante. Son condiciones de prueba concretas, no una garantía universal de THD ni una medida de un STC-8 real. Los archivos del motor de audio no cambian respecto a v1.2.1.
