# Software-based Real-time Arranger — Version 4.06

**06-26-2026 · ZZ-Denis @ NazoMusic**

---

## Copyright Notice

All source code in this project is licensed under the **AGPLv3**
(GNU Affero General Public License, Version 3).

---

## System Requirements

1. Windows or Linux operating system
2. A MIDI IN/OUT device
3. A MIDI keyboard with 49 keys or more

---

## Overview

1. Provides real-time intelligent auto-accompaniment (simulates a live arranger keyboard).
2. Splits the keyboard into two zones: **Lower Channel** and **Upper Channel**.
3. Style files use **SMF format (MIDI Format 0)** and can be user-created.
   *(SMF Format 1 is not supported.)*
4. Fully controlled via the MIDI keyboard; no computer keyboard required.
5. \* The file `sra_init.hex` in the SRA directory can store MIDI messages for device initialization at startup.

---

## Setup Parameters

| Key | Parameter | Description |
|-----|-----------|-------------|
| `[W][X]` | MIDI Channel | The channel your keyboard transmits on |
| `[O]` | CTRL Offset | Shifts the command key zone. Values: `{-1, 0, +1}` — use `+1` for 76-key, `-1` for 49-key |

---

## MIDI Keyboard Command Reference

> **C5 = Middle C**

| Key Combination | Function |
|-----------------|----------|
| `C8` | **Shift** (same role as the Shift key on a computer keyboard) |
| `B7` | Fill to Original |
| `A7` | Fill to Variation |
| `Bb7` | Start |
| `Ab7` | Intro / Ending |
| `Shift + F#7` | Fade Out |
| `Shift + C#7` | Tempo + |
| `Shift + Eb7` | Tempo − |
| `Shift + G7` | Change Mode |
| `Shift + F7` | Toggle M.Bass |
| `Shift + E7` | Toggle Acc. |
| `Shift + D7` | Toggle Acc.Bass |
| `Shift + C7` | Toggle Drum |
| `Shift + B7` | To Original |
| `Shift + A7` | To Variation |
| `Shift + Bb7` | Sync Start |
| `Shift + Ab7` | **Function** (see below) |

### Function Mode (`Shift + Ab7`)

Press `Shift + Ab7` while the rhythm is **stopped** to enter Function mode.
In this mode, each key loads a different style file (except Shift, Tempo+, Tempo−, Intro/Ending, Fade Out, and Start).

| Example | Action |
|---------|--------|
| `Shift+Ab7` + `C3` | Load `style36.mid` |
| `Shift+Ab7` + `C#3` | Load `style37.mid` |
| *(and so on…)* | |
| `Shift+Ab7` + Intro/Ending | Toggle MIDI Clock (Start/Stop) output / Reset |
| `Shift+Ab7` + Start | Octave shift up (Upper keyboard) |
| `Shift+Ab7` + Fade Out | Octave shift down (Upper keyboard) |
| `Shift+Ab7` + Tempo+ | Transpose + |
| `Shift+Ab7` + Tempo− | Transpose − |
| `Shift+Ab7` + Shift | Exit the program |

---

## Creating Your Own Style Files

### File Format

- Save as **SMF Format 0**. Name files `style0.mid` through `style127.mid`.
- Place style files in the **same directory** as the SRA executable (e.g. `C:\NazoMusic-SRA\`).
- Each file is mapped to a key and loaded via Function mode.

### Parameters

Each style file contains five parameters placed in order in the first bar:

| Parameter | Range | Description |
|-----------|-------|-------------|
| `Tempo` | 10–125 | Initial tempo — enter **half** the actual BPM value |
| `Beat` | 2, 3, 4, 6 | Beats per bar |
| `IL` | 1–8 | Number of bars in the intro section |
| `ML` \* | 1–8 | Number of bars in the main (normal) section |
| `EL` | 1–8 | Number of bars in the ending section |

> Refer to `style0.mid` using a DAW or MIDI editor for the exact format.

### Section Order (from bar 2 onwards)

```
Chord C  : Intro → Original×ML → Original-to-Variation fill
         → Variation×ML → Variation-to-Original fill → Ending →
Chord Cm : Intro → Original×ML → Original-to-Variation fill
         → Variation×ML → Variation-to-Original fill → Ending →
Chord C7 : Intro → Original×ML → Original-to-Variation fill
         → Variation×ML → Variation-to-Original fill → Ending
```

- Each style bar must begin with initialization commands (e.g. MIDI PATCH / BANK settings).

### MIDI Channel Assignments

| Channel | Name | Role |
|---------|------|------|
| Ch2 | Acc.Bass | Auto bass |
| Ch5 | Acc1 | Accompaniment part 1 |
| Ch6 | Acc2 | Accompaniment part 2 |
| Ch7 | Acc3 | Accompaniment part 3 (sustained tones, e.g. Strings) |
| Ch8 | Acc4 | Accompaniment part 4 (sustained tones, e.g. Strings) |
| Ch10 | Drum | Drum kit |

---

## FAQ

**1. After installation, how do I select a style?**

After installation you will have one style file: `style0.mid`, which loads automatically when SRA starts. To begin on a 61-key keyboard, press the highest Bb (`Bb7`) to start the rhythm.

For **Sync Start**, hold the highest C (`C8`) — keep it held — then press `Bb7` and release. The rhythm will start as soon as you play a chord.

Chords are detected using a three-key method and support chord inversions (C, C/G, C/D, etc.). The current chord and tempo are shown on screen.

To change styles, download or create additional style files. Name them `style36.mid`, `style37.mid`, etc. To load `style36.mid`, hold Shift (`C8`), press `Ab7`, then release. The system enters style-selection mode. On a 61-key keyboard, `C3` (the lowest key) maps to number 36, so pressing `C3` loads `style36.mid`. `D3` loads `style38.mid`, and so on.

**2. After selecting a style, how do I adjust the tempo?**

Hold Shift (`C8`) and press `C#7` to increase tempo, or `Eb7` to decrease it. Each press changes the tempo by 1.

---

## Error Codes

| Code | Description |
|------|-------------|
| `Error(1)` | Style file not found |
| `Error(2)` | Style file format invalid |
| `Error(3)` | Style file parameters out of range |
| `Error(4)` | Style file contains an invalid MIDI channel |
| `Error(5)` | Style file bar is missing initialization data |
| `Error(6)` | Style file has insufficient bars |
| `Error(7)` | Style file data exceeds buffer size |
| `Error(888)` | MIDI input device error |
| `Error(999)` | MIDI output device error |

---

## Project Plans and Goals

- Development focus will remain on evolving and extending **libsracore**'s core functionality and flexibility. UI and interface work (e.g. adding support for Qt, SDL, JACK, etc.) is out of scope for this project.
- If you wish to improve or adapt the UI or interface, please **fork** this project.
- As libsracore evolves, breaking changes to the opaque pointer API, callbacks, MIDI file format, and related interfaces may occur in future versions.

---

Compatible soundfonts for SRA: https://archive.org/details/SF_zzdenis
