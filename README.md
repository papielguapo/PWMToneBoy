# PWM TONE BOY — PAPIELGUAPO

**v1.2.0 · Compresor estéreo con color analógico, curva viva y análisis de salida.**

![PWM TONE BOY](docs/images/PWM-TONE-BOY.png)

- **[Descargar VST3 + Standalone — Windows x64](https://github.com/papielguapo/PWMToneBoy/releases/download/v1.2.0/PWM-TONE-BOY-Windows-x64.zip)**
- **[Guía completa en español](docs/GUIA-ES.md)**: instalación, cada perilla/botón/clic, seis pantallas y Standalone.
- [Release v1.2.0](https://github.com/papielguapo/PWMToneBoy/releases/tag/v1.2.0)

## Qué cambia

**D ahora es más analógico y dinámico**, en lugar de omitir el color adicional. El nivel de entrada y la memoria de compresión gobiernan un contorno de graves/agudos y armónicos suaves. A conserva el perfil analógico más sutil. No hay ruido aleatorio, hiss, hum ni dither. Ambos modos y KI mantienen cero exacto desde reposo con entrada de cero.

**Linear Analysis se mueve con el DSP:** la curva brillante incluye filtros, carga dinámica y reducción actual de ganancia. La línea tenue es la referencia sin reducción, no una curva A/B. No se simula movimiento aleatorio. Es un modelo de magnitud del estado actual; no una medición del hardware ni de la distorsión no lineal.

![Curva viva](docs/images/Linear-Analysis.png)

**Harmonic Analysis muestra espectro con música:** entrada oscura y salida brillante, FFT de 8192 muestras. Con un seno válido añade marcadores de armónicos, F0, THD y THD+N; con música conserva las curvas y deja las cifras de distorsión en --.

![Espectro de salida con música](docs/images/Harmonic-Music.png)
![Análisis con seno](docs/images/Harmonic-Analysis.png)

## Controles y sonido

THRESHOLD: −42 a 0 dB; RATIO: 1/2/3/5/10/20/100; DECAY: 100–3200 ms. EQ IN cambia reconstrucción 11.2/20 kHz; HPF SC filtra el detector a 90 Hz. LINK combina el 85% del control de compresión y enlaza la protección de picos. PAD aplica x0.25. KI añade segundo armónico; PDA acorta el ataque cuando se aproxima una sobrecarga; H-SC combina pico/RMS con preénfasis.

A/D cruza en 40 ms. A conserva +0.30 dB/80 Hz, −0.25 dB/6 kHz y color suave. D, inicialmente seleccionado, varía aproximadamente entre +0.15/+1 dB a 65 Hz y −0.10/−0.85 dB a 8 kHz. Su carga tiene respuesta de 35 ms y relajación de 650 ms; aumenta también la curvatura H2/H3. Las etapas asimétricas preceden al paso alto de cuarto orden a 18 Hz.

Auto-Gain está siempre activo en la señal procesada: `(-threshold * (1 - 1/ratio)) * 0.54` dB. OUTPUT ajusta −24 a +12 dB después de la protección de picos; ganancias positivas pueden superar 0 dBFS. BYPASS cruza a la señal seca compensada por latencia, sin OUTPUT.

SELECT y la vista DAC/PWM son heredados: DAC y ProSound siguen desactivados incluso al automatizar parámetros antiguos. El editor vectorial conserva su skin, escala de 80–200% y zonas de clic proporcionales. Un clic en la LCD recorre seis vistas. El Standalone incluye bucles internos y reproducción WAV/AIFF.

**Compatibilidad sonora:** se conservan IDs, parámetros, estado y fabricante PAPIELGUAPO; el cambio de D es intencionado. Una sesión con D puede sonar diferente al cargar v1.2.0. Las releases anteriores siguen disponibles para recuperar su sonido. Es un diseño original inspirado en comportamiento analógico/STC-8, no una emulación calibrada de Crane Song ni una reconstrucción de su circuito.

## Compilación

C++20, CMake 3.22+, JUCE 8.0.4 descargado mediante FetchContent si no se encuentra JUCE 8 local. En Windows se utiliza Visual Studio con C++/Windows SDK.

```powershell
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release --parallel
ctest --test-dir build -C Release --output-on-failure
```

Artefactos:

- `build/PWMToneBoy_artefacts/Release/VST3/PWM TONE BOY.vst3` (copiar el bundle completo).
- `build/PWMToneBoy_artefacts/Release/Standalone/PWM TONE BOY.exe`.

```powershell
& 'build/PWMToneBoyChecks_artefacts/Release/PWMToneBoyChecks.exe' QA
```

La telemetría usa una cola SPSC; FFT y análisis armónico se ejecutan en la interfaz y no reservan buffers en el callback de audio. La latencia sigue siendo de 6 muestras. Los análisis usan el canal 1. La THD se estima con un seno estable y una ventana finita; se ocultan cifras no válidas. M4 estima magnitud con coeficientes y reducción actuales, sin representar fase, armónicos ni rizado de oversampling.

Los ZIP incluyen fuentes o binarios, guía, capturas y registros QA. Las pruebas cubren estado, automatización, silencio, mono/estéreo, bloques variables, diferentes frecuencias, A/D y seis visores. La licencia de JUCE está en `build/_deps/juce-src` tras configurar. La tipografía usa Arial/Arial Black como fallback del diseño vectorial original.


**Verificación v1.2.0:** 3.899 comprobaciones automatizadas superadas, incluidas visualización con música y silencio exacto en A y D con KI. pluginval nivel 10 superado (semilla 1472). Se incluyen los registros completos en QA.
