# Secret of Evermore SNES Recomp (Port Nativo para PC)

Port nativo para Windows 64-bit del clásico de Super Nintendo **Secret of Evermore** (versión oficial en castellano / ESP), desarrollado mediante técnicas de ingeniería inversa y recompilación estática sobre la base de [snesrecomp](https://github.com/RetroPortingToolKit/snesrecomp).

---

## 🌟 Características Principales

* **🖥️ Resolución 1080p y 4K UHD:** Ejecución nativa para monitores modernos con una nitidez absoluta.
* **📺 Modo 16:9 Pantalla Completa (Cero Pop-in):** Arquitectura de presentación en pantalla panorámica que garantiza la precarga del 100% de los mapas y tiles en memoria (estilo *Alundra Recompiled*), eliminando la aparición repentina de bloques de textura en los laterales.
* **🎨 Filtros y Anti-Aliasing de Última Generación:**
  * **HD-2D Remaster:** Suavizado subpíxel de bordes, micro-contraste en contornos y expansión de la paleta original de 15 bits a color dinámico de 24 bits (estilo remakes modernos de Square Enix como *Live A Live* y *Octopath Traveler*).
  * **xBR-lv2 Anti-Aliasing:** Algoritmo avanzado de vectorización que redondea los dientes de sierra de los sprites y fondos sin difuminar la imagen.
  * **FXAA Retro:** Anti-aliasing rápido direccional para bordes suaves.
  * **Smooth Bicúbico:** Suavizado de texturas mediante curvas Catmull-Rom.
  * **CRT Soft y Warm Composite:** Simulación de líneas de escaneado y calidez de televisión clásica de tubo.
  * **Sharp HD:** Escalado píxel-perfect de máxima definición.
* **🎮 Compatibilidad Total con Mandos:** Detección plug & play de mandos de Xbox, PlayStation (DualShock / DualSense), Nintendo Switch Pro y controladores genéricos SDL3.
* **⏪ Rebobinado en Tiempo Real y Guardado Rápido:** Vuelve atrás hasta 60 segundos con F12 (o atajo de mando) y usa ranuras de guardado y carga rápida inmediatas (F1 a F10).
* **✨ Pantalla de Nombres Corregida:** Solucionado el fallo gráfico en modos de alta resolución (Modos 5 y 6 PPU), evitando duplicaciones en los márgenes laterales.
* **📖 Prólogo Completo sin Cortes:** Memoria SRAM inicializada de fábrica para que al empezar una Nueva Partida se reproduzca siempre la cinemática íntegra de 1965 y 1995 en Podunk.
* **⚡ 60 FPS Sincronizados:** Fluidez de refresco sincronizada por haz de escaneo con audio de bajísima latencia vía WASAPI.

---

## ⚖️ Aviso Legal

> **Es imprescindible poseer una copia legal original del juego.**  
> Este repositorio no contiene ROMs, código binario con copyright ni recursos protegidos de Nintendo o SquareSoft. El ejecutable comprueba y carga tu propio volcado legal de `Secret of Evermore (ESP).sfc`.

### Datos de la ROM compatible

| Campo | Valor |
| :--- | :--- |
| **Juego** | Secret of Evermore |
| **Región** | España / Castellano (ESP) |
| **Archivo** | `Secret of Evermore (ESP).sfc` (o `.smc`) |
| **Mapeado** | HiROM |
| **CRC32** | `7bfe934f` |
| **SHA-256** | `3c7e2101b6fc8d63fa5e6341629eef4a5d8f69c99a8826a90bc5417c5bb48bf1` |

---

## 🚀 Cómo Jugar

1. Coloca tu archivo `Secret of Evermore (ESP).sfc` en la carpeta raíz del proyecto o junto al ejecutable.
2. Inicia el juego con el perfil que prefieras:
   * **`JUGAR_16_9.bat`**: Pantalla completa 16:9 panorámica, cero pop-in, texturas precargadas y shader HD-2D activado.
   * **`JUGAR_SIN_VENTANAS.vbs`**: Ejecución directa silenciosa sin que parpadee la consola de comandos CMD.
   * **`JUGAR_4_3.bat`**: Modo clásico original 4:3 pixel-perfect con barras laterales.
   * **`JUGAR_HACK_16_9.bat`**: Modo hack con campo de visión (FOV) extendido.
   * **`LAUNCHER.bat`**: Menú gráfico de configuración para personalizar resolución, mandos, audio y shaders.

---

## 🛠️ Cómo Compilar desde el Código Fuente

### Requisitos
* Windows 10 u 11 (64 bits)
* Visual Studio 2022 (Community o Build Tools) con las herramientas de C/C++
* CMake 3.20 o superior
* Git

### Pasos de compilación

```cmd
git clone --recursive https://github.com/tu-usuario/SoE-Recomp.git
cd SoE-Recomp
COMPILAR.bat
```

O directamente mediante CMake en consola:
```cmd
cmake -B build -S .
cmake --build build --config Release
```

---

## 🕹️ Controles por Defecto

### Teclado
| Acción | Tecla |
| :--- | :--- |
| **Movimiento** | Flechas de dirección |
| **Atacar / Aceptar (B)** | Z |
| **Correr / Cancelar (A)** | X |
| **Menú Anillo (Y)** | A |
| **Controlar Perro (X)** | S |
| **Giro / Menú L y R** | C / V |
| **Pausa / Start** | Enter / Return |
| **Select** | Shift Derecho |

### Atajos de Teclado
* **Alt + Enter:** Pantalla completa / Modo ventana
* **F12:** Rebobinar partida
* **Shift + F1 a F10:** Guardado rápido (ranuras 1 a 10)
* **F1 a F10:** Carga rápida (ranuras 1 a 10)
* **Tab:** Acelerar / Turbo
* **P:** Pausa congelada
* **F:** Mostrar estadísticas de rendimiento y FPS

---

## 📜 Créditos

* Desarrollado con el framework de recompilación **snesrecomp** por RetroPortingToolKit.
* Juego original © 1995 Square Co., Ltd. / SquareSoft.
