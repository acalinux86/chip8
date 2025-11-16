# CHIP-8 Emulator

A simple and lightweight CHIP-8 emulator written in C using SDL2 for graphics, input, and audio.

> **⚠️ Warning**
> The emulator produces a **beep sound** immediately on startup (classic CHIP-8 behavior).
> Lower your volume if needed.

---

## Input Mapping

CHIP-8 uses a hexadecimal keypad.
In this emulator, the keys **0–F** are mapped directly to your keyboard:

```
1 2 3 4        →   1 2 3 4
Q W E R        →   5 6 7 8
A S D F        →   9 A B C
Z X C V        →   D E F 0
```

---

##  Building

Make sure SDL2 is installed.

```bash
make -B
```

This will compile the emulator and output the binary into `./build/`.

---

## Running a ROM

```bash
./build/chip8 ./assets/octojam2title.ch8
```

You can load any `.ch8` program from the available assests folder or load your own sourced .che program.

---

## ROMs

Most of the ROMs used during testing are from:

* [https://johnearnest.github.io/chip8Archive/](https://johnearnest.github.io/chip8Archive/)
* [https://github.com/Timendus/chip8-test-suite.git](https://github.com/Timendus/chip8-test-suite.git)

---

## Features

* Full CHIP-8 instruction set (work in progress)
* Configurable CPU speed (default 700 Hz)
* Almost Accurate 60 Hz timers
* Square-wave audio beep
* SDL2 renderer with window resizing
* Modern, clean codebase designed for readability

---

## Todo

* Complete missing opcodes
* Improve sound system (volume, toggling)
* Add debugger and step-mode
* Add settings for custom resolutions / themes

---

## Acknowledgements

Thanks to the CHIP-8 community and the ROM archivists who help preserve classic programs.
