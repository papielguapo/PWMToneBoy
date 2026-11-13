# PWM TONE BOY — PAPIELGUAPO

**Compresor estéreo con color KI, recuperación con memoria y OUTPUT independiente.**

![PWM TONE BOY v1.0.1](docs/images/PWM-TONE-BOY.png)

## Descarga y guía de uso

- **[Descargar VST3 + Standalone para Windows x64](https://github.com/papielguapo/PWMToneBoy/releases/download/v1.0.1/PWM-TONE-BOY-Windows-x64.zip)**
- **[Guía completa en español: perillas, botones, clics, pantallas e instalación](docs/GUIA-ES.md)**
- [Página de la versión v1.0.1](https://github.com/papielguapo/PWMToneBoy/releases/tag/v1.0.1)

Arrastra las perillas para ajustar; doble clic restaura su valor inicial. Pulsa la LCD para alternar sus cuatro visores. OUTPUT sustituye a START/DAC. ProSound y DAC están desactivados. SELECT y M3 se conservan como controles/diagnósticos heredados y no activan cuantización. El editor se puede redimensionar entre el 80% y el 200%.

El ZIP descargable incluye la guía en español y la captura de la interfaz. El binario continúa siendo v1.0.1; esta actualización añade documentación e imagen, sin cambiar el audio.

## Technical documentation

Version 1.0.1. Windows x64 VST3 and Standalone, JUCE 8.0.4 / C++20.

## Current behavior

- Resizable vector editor, 336–840 px wide (80–200%), with proportional hit targets and saved window width.
- OUTPUT replaces START/DAC: -24 to +12 dB, initially 0 dB, smoothed over 20 ms, after peak protection. Auto-Gain remains active in the wet path.
- ProSound and DAC are disabled in DSP, including automation and older saved states. Their original parameter IDs remain for session compatibility. The jack, ProSound and AUTO-GAIN text/LED, and upper/lower rack legends have been removed. Other controls and positions are retained.
- Manufacturer: PAPIELGUAPO. Existing plugin identifiers are retained.

## Dynamics revision

This is an original design inspired by STC-8 behavior, not a measured hardware emulation. No hardware reference or capture set was available. Attack/release coefficients and harmonic levels are engineering choices, not recovered Crane Song circuitry.

The processor runs at 4x with integer latency compensation (6 samples). The detector combines peak and RMS with optional sidechain HPF/presence emphasis. Program memory has an 80 ms charge and 1.4 s discharge; sustained gain reduction lengthens the slow-envelope and gain-domain release. The gain curve is continuous, with a ratio-dependent soft knee. LINK retains the 85% shared compressor gain and links peak protection fully when enabled.

PDA now shortens the nominal 2.2 ms attack toward 0.35 ms when the predicted output approaches overload. This is an A-MOD-inspired interaction; the peak protector and compressor operate together rather than representing attack modification as transient slowing.

KI OFF uses a unity waveshaper. KI ON adds a small bounded even function, x + k2*x*x/(1+0.25*x*x), with k2 dependent on activity and compression. There is no positive-half-cycle logarithm and no independent bias/noise source. This implementation does not claim to reproduce the hardware's complete KI sidechain changes.

A dynamic peak protector acts near -0.45 dBFS, with a 0.5 ms hold and 60 ms recovery. A C2-continuous safety curve is exactly linear through +/-0.98 and approaches +/-1 above that. These limits are before OUTPUT; positive output trim can exceed 0 dBFS. The existing reconstruction filter, fourth-order 18 Hz high-pass, and +0.85 dB/52 Hz contour remain. Thus, "linear KI OFF" does not mean a flat frequency response or dry null across those filters.

Auto-Gain: (-threshold * (1 - 1/ratio)) * 0.54 dB. Bypass crossfades to a dry signal delayed by the declared latency. Diagnostics use a bounded SPSC queue; graphics and FFT run outside the audio callback. No buffers are allocated in normal DSP processing. The optional Standalone file player uses JUCE's transport.

## Controls and compatibility

Threshold: -42 to 0 dB; ratio: 1/2/3/5/10/20/100; decay: 100/200/400/800/1600/3200 ms. EQ, HPF SC, LINK, BYPASS, PAD, KI, PDA and H-SC remain available. The old clock control/diagnostic view is retained, but sample-and-hold and quantization have no audible effect while the DAC path is disabled. The old dacCrush and phonesRawMode parameters are retained but ignored by DSP. outputGain is appended as a new parameter, preserving old IDs.

The Standalone offers Drums, 808 Bass and Chiptune test loops and looping WAV/AIFF playback. JUCE Options selects the audio device. Input is muted by default by the Standalone wrapper; internal test loops remain available.

## Build and checks

Install CMake 3.22+ and Visual Studio C++/Windows SDK. JUCE 8 is found locally or fetched from the 8.0.4 release.

```powershell
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release --parallel
ctest --test-dir build -C Release --output-on-failure
```

Visual Studio selects Release with --config. Release uses MSVC /O2 /fp:fast, or -O3 on GCC/Clang.

Artifacts:

- build/PWMToneBoy_artefacts/Release/VST3/PWM TONE BOY.vst3 (copy the complete bundle).
- build/PWMToneBoy_artefacts/Release/Standalone/PWM TONE BOY.exe.

```powershell
& 'build/PWMToneBoyChecks_artefacts/Release/PWMToneBoyChecks.exe' QA
```

3,858 checks passed: parameter/state handling, corrupt-state rejection, continuous monotonic knee, exact silence/reset, finite/bounded output at 32/44.1/48/96/192 kHz, 1–4097 sample blocks, bypass alignment, mono/stereo, disabled legacy DSP, output calibration, long-tail settling, standalone loops and editor render/hit targets at four scales. pluginval strictness 10 passed with seed 1472.

Spectral tests use coherent 48 kHz signals, one second settling followed by one second measurement. Single tones use 0.2 peak amplitude at unity ratio, or 0.5 peak amplitude with 3:1 / -18 dB threshold. THD sums harmonics 2–12 below Nyquist. These are steady-state digital tests, not hardware comparisons or transient-distortion measurements.

Measured results: 1 kHz KI THD -62.03 dB (about 0.079%), H3 below H2; KI-off THD below -100 dB in the 1 kHz test. Compressed 50 Hz THD -72.67 dB. With 5+6 kHz tones at 0.15 peak each, the 1 kHz difference product is -58.54 dBc with KI and below -100 dBc without KI. The 1 kHz clean/steady compressed measurements approach the numerical floor of this test and are not analog performance specifications.

After 200 ms of silence, a short versus sustained test leaves approximately 4.98 versus 6.36 dB reduction, confirming program-dependent recovery. QA/ contains actual test logs and editor snapshots. Final compatibility and sonic preference should be checked in the intended DAW and on real program material.

The skin is a vector interpretation of the written specification; Arial/Arial Black replace the unavailable Inter asset. JUCE's source and licensing are in build/_deps/juce-src after configuration.
