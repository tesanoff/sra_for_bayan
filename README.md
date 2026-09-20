# Software-based Real-time Arranger — Version 4.06

**06-26-2026 · ZZ-Denis @ NazoMusic**

**Fork is maintained by Alexander Tesanov (https://tesanoff.klah.ru).**

The original project is here: https://github.com/imzzdenis/sra

---

## Copyright Notice

All source code in this project is licensed under the **AGPLv3**
(GNU Affero General Public License, Version 3).

---

## System Requirements

1. Windows or Linux operating system
2. A MIDI IN/OUT device
3. A MIDI keyboard with 49 keys or more

See [BUILD.md](BUILD.md) for build instructions.

---

## Note Naming Convention

This document uses **C5 = Middle C** (MIDI note 60) throughout, which
is the convention used by the original SRA documentation.  Under this
convention the MIDI note numbers map to note names as follows:

| MIDI note | This document | Scientific pitch notation (C4 = Middle C) |
|-----------|---------------|--------------------------------------------|
| 60        | C5            | C4                                         |
| 72        | C6            | C5                                         |
| 84        | C7            | C6                                         |
| 94        | Bb7           | Bb6                                        |
| 96        | C8            | C7                                         |

> **There is no single standard for octave numbering.**  The MIDI
> specification defines note numbers and fixes note 60 as Middle C,
> but it does not prescribe how octaves are *named*.  Different
> manufacturers and DAWs use C3, C4, or C5 for Middle C.  When in
> doubt, refer to the MIDI note number, which is unambiguous.
>
> For example, the Start command is MIDI note **94**.  In this
> document it is written `Bb7`; in scientific pitch notation it would
> be `Bb6`.  It is the same note.

---

## Overview

1. Provides real-time intelligent auto-accompaniment (simulates a live arranger keyboard).
2. Splits the keyboard into two zones: **Lower Channel** and **Upper Channel**.
3. Style files use **SMF format (MIDI Format 0)** and can be user-created.
   *(SMF Format 1 is not supported.)*
4. Fully controlled via the MIDI keyboard and/or **SysEx** messages;
   no computer keyboard required.
5. Runs either interactively (terminal UI) or as a background
   **daemon** managed by systemd *(Linux only)*.
6. \* The file `sra_init.hex` in the SRA directory can store MIDI messages for device initialization at startup.

---

## Command Line

```
sra                                  interactive mode
sra --daemon [options]               daemon mode (Linux only)
```

| Option | Description |
|--------|-------------|
| `--daemon` | Run as a daemon (no UI, logs via syslog). **Linux only** — rejected with an error on Windows. |
| `--config PATH` | Config file path. Linux default: `/etc/sra/sra.conf`. Windows default: `%APPDATA%\sra\sra.conf`. |
| `--in ADDR` | MIDI IN rawmidi address, e.g. `hw:5,0` (Linux). On Windows, use the device name. |
| `--out ADDR` | MIDI OUT rawmidi address, e.g. `hw:5,1` (Linux). On Windows, use the device name. |
| `--chord-ch N` | Chord channel, **1-based** (1–16). |
| `--ctrl-offset N` | Command key zone shift: `-1`, `0`, or `+1`. |
| `--help` | Show help and exit. |
| `--version` | Show version and exit. |

Command-line options override values from the config file.

> All channel numbers in the config file and on the command line are
> **1-based** (1–16), matching the number shown in the interactive
> setup screen as *Chord Ch*.  Internally SRA converts them to 0–15.
> SysEx control messages, by contrast, use **0-based** channel numbers
> (see the SysEx section).

In interactive mode, `--in` and `--out` set the *initial* port
selection; you can still change it with `[Q]/[Z]` and `[E]/[C]`.

---

## Setup Parameters (interactive mode)

When SRA starts interactively, it shows a setup screen:

```
Software-based Real-time Arranger v4.06 by ZZ-Denis @ NazoMusic
===============================================================

  [Q/Z] MIDI IN  [1/4]: hw:5,0
         Virtual Raw MIDI
  [E/C] MIDI OUT [2/4]: hw:5,1
         Virtual Raw MIDI
  [A/D] Chord Ch: 3
  [O]   Ctrl Offset: 0

  [S] START
```

| Key | Parameter | Description |
|-----|-----------|-------------|
| `[Q][Z]` | MIDI IN | Select the MIDI input port. `Q` — next port, `Z` — previous port. |
| `[E][C]` | MIDI OUT | Select the MIDI output port. `E` — next port, `C` — previous port. |
| `[A][D]` | Chord Ch | MIDI channel reserved for chord input (1–16). Must match the channel your keyboard uses for the left-hand chord zone. `A` decreases, `D` increases. |
| `[O]` | CTRL Offset | Shifts the command key zone. Values: `{-1, 0, +1}` — use `+1` for 76-key, `-1` for 49-key. |
| `[S]` | START | Open the selected MIDI ports and start the arranger. |

> The `[W][X] Channel` parameter from earlier versions has been removed.
> Live notes are no longer merged onto a single channel — they are
> forwarded on the same MIDI channel they arrived on.

---

## Daemon Mode

> **Linux only.**  Daemon mode requires systemd.  On Windows,
> `--daemon` is rejected with an error at startup.

SRA can run as a systemd service, without a terminal.

### Configuration file

Place `/etc/sra/sra.conf` with the following content (see
`sra.conf.example` in the source tree):

```
# SRA configuration file.
in  = hw:5,0
out = hw:5,1
chord_ch = 3
ctrl_offset = 0
```

Format: `key = value`, one per line. `#` starts a comment.
Recognised keys: `in`, `out`, `chord_ch`, `ctrl_offset`.
Unknown keys and malformed lines abort startup with an error.

`chord_ch` is **1-based** (1–16), matching the value shown in the UI.
In daemon mode, `in` and `out` are **required** — either in the config
file or via `--in` / `--out` on the command line.

### systemd unit

An example unit is provided as `sra.service`:

```ini
[Unit]
Description=Software-based Real-time Arranger
After=sound.target
Wants=sound.target

[Service]
Type=simple
ExecStart=/usr/local/bin/sra --daemon --config /etc/sra/sra.conf
Restart=on-failure
RestartSec=2

[Install]
WantedBy=multi-user.target
```

### Install

```sh
sudo cp build/sra /usr/local/bin/sra
sudo mkdir -p /etc/sra
sudo cp sra.conf.example /etc/sra/sra.conf
sudo nano /etc/sra/sra.conf              # set in/out at minimum
sudo cp sra.service /etc/systemd/system/
sudo systemctl daemon-reload
sudo systemctl enable --now sra
```

### Logs

SRA writes to syslog (ident `sra`). On a systemd host this is
available through the journal:

```sh
journalctl -u sra -f
```

Typical entries:

```
sra[PID]: started, in=hw:5,0 out=hw:5,1 chord_ch=3 ctrl_offset=0
sra[PID]: stopped
```

Errors (missing ports, MIDI open failure) are also logged.

### Stopping

```sh
sudo systemctl stop sra
```

The daemon handles `SIGTERM` gracefully: it stops the worker
threads, releases the MIDI ports, and exits.

---

## MIDI Keyboard Command Reference (deprecated)

> **Note:** These Note-On commands are kept for backward compatibility.
> They can be disabled via SysEx (`F0 7D 50 00 F7`) and are expected to
> be superseded by the SysEx protocol in future versions. New setups
> should prefer SysEx control.

> **Note names** follow the convention described at the top of this
> document (**C5 = Middle C**).  The raw MIDI note number is given in
> parentheses for reference.

| Key Combination | MIDI note | Function |
|-----------------|-----------|----------|
| `C8` | 96 | **Shift** (same role as the Shift key on a computer keyboard) |
| `B7` | 95 | Fill to Original |
| `A7` | 93 | Fill to Variation |
| `Bb7` | 94 | Start |
| `Ab7` | 92 | Intro / Ending |
| `Shift + F#7` | 90 | Fade Out |
| `Shift + C#7` | 85 | Tempo + |
| `Shift + Eb7` | 87 | Tempo − |
| `Shift + G7` | 91 | Change Mode |
| `Shift + F7` | 89 | Toggle M.Bass |
| `Shift + E7` | 88 | Toggle Acc. |
| `Shift + D7` | 86 | Toggle Acc.Bass |
| `Shift + C7` | 84 | Toggle Drum |
| `Shift + B7` | 95 | To Original |
| `Shift + A7` | 93 | To Variation |
| `Shift + Bb7` | 94 | Sync Start |
| `Shift + Ab7` | 92 | *(obsolete — Function mode has been removed)* |

---

## SysEx Control Protocol

SRA accepts control messages as MIDI System Exclusive (SysEx) data.
The protocol uses the non-commercial Manufacturer ID `0x7D`.

### Message format

```
F0 7D <CMD> [<DATA...>] F7
```

| Byte | Meaning |
|------|---------|
| `F0` | SysEx start |
| `7D` | Manufacturer ID (non-commercial) |
| `<CMD>` | Command byte (see table below) |
| `<DATA...>` | Optional parameter bytes (0–127 each) |
| `F7` | SysEx end |

- Unknown CMD values are ignored silently.
- Malformed messages with our Manufacturer ID are reported to `stderr`
  but never abort the engine.
- SysEx is processed independently of Note-On commands: it always works,
  even when Note-On commands are disabled.

### Command table

| CMD | Function | Data |
|-----|----------|------|
| `01` | Start | — |
| `02` | Stop | — |
| `03` | Sync Start | — |
| `04` | Fill to Original | — |
| `05` | Fill to Variation | — |
| `06` | Intro / Ending (toggle) | — |
| `07` | Tempo + | — |
| `08` | Tempo − | — |
| `09` | Toggle M.Bass | — |
| `0A` | Toggle Acc. | — |
| `0B` | Toggle Acc.Bass | — |
| `0C` | Toggle Drum | — |
| `0D` | Fade Out (toggle) | — |
| `0E` | Change Mode | — |
| `20` | Load Style | style number (0–127) |
| `50` | Enable / disable Note-On commands | `00` = off, `01` = on |
| `51` | Set chord channel | channel, **0-based** (0–15) |

> **Channel numbering.** The SysEx command `0x51` uses **0-based**
> channel numbers (0–15), i.e. `00` = MIDI channel 1, `02` = MIDI
> channel 3.  This is different from the config file and `--chord-ch`,
> which use 1-based numbers.  The reason: SysEx is a low-level
> protocol and MIDI convention for raw values is 0-based; the config
> file, by contrast, mirrors what the user sees in the UI.

### Examples

Start the arranger:

```
F0 7D 01 F7
```

Load `style5.mid`:

```
F0 7D 20 05 F7
```

Set the chord channel to MIDI channel 4 (0-based 3):

```
F0 7D 51 03 F7
```

Disable the legacy Note-On commands:

```
F0 7D 50 00 F7
```

### Sending SysEx from a shell

On Linux, `amidi` from `alsa-utils` can send raw SysEx:

```sh
amidi -p hw:5,2 -S 'F0 7D 01 F7'
```

On Windows, use `sendmidi` or a similar MIDI utility.

---

## Creating Your Own Style Files

### File Format

- Save as **SMF Format 0**. Name files `style0.mid` through `style127.mid`.
- Place style files in the **same directory** as the SRA executable (e.g. `C:\NazoMusic-SRA\`).
- Each file is mapped to a key and loaded via the SysEx `Load Style`
  command (`F0 7D 20 <NN> F7`).

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
| Ch8  | Acc.Bass | Auto bass |
| Ch9  | Acc1     | Accompaniment part 1 |
| Ch10 | Drum     | Drum kit (GM standard drum channel) |
| Ch11 | Acc2     | Accompaniment part 2 |
| Ch12 | Acc3     | Accompaniment part 3 (sustained tones, e.g. Strings) |
| Ch13 | Acc4     | Accompaniment part 4 (sustained tones, e.g. Strings) |
| Ch14 | M.Bass   | Melodic bass |
| Ch15 | Lower    | Lower voice (sustained tones) |

> Channels **1–7** and **16** are reserved for live playing. SRA does not
> send accompaniment data to them, and ignores any incoming MIDI
> messages on the arranger-owned channels Ch8–Ch15.

> **Note on this fork.** The channel layout above matches the current
> version of this fork.  Style files created for the original project
> by ZZ-Denis used different channels (1, 4, 5, 6, 7) and must be
> migrated — see "Migrating Old Style Files" below.

---

## Migrating Old Style Files

Earlier versions of SRA used different MIDI channels for the
accompaniment parts (1, 4, 5, 6, 7). The current version uses
channels 7, 8, 10, 11, 12 (0-based; see above). If you have style
files created for the old channel layout, they must be migrated.

A helper script is provided:

```sh
./convert_style.sh style0.mid style1.mid style2.mid
```

It creates a `.bak` backup next to each file, remaps the channels
in place, and verifies the result. Requires `midicsv` / `csvmidi`
(`sudo apt install midicsv` on Debian/Ubuntu).

Channel 0 (style header) and channel 9 (drum) are left untouched.
If the script finds any other channel, it refuses to migrate the
file and reports an error — this usually means the file is not
a valid SRA style.

---

## FAQ

**1. After installation, how do I select a style?**

After installation you will have one style file: `style0.mid`, which loads
automatically when SRA starts. To begin on a 61-key keyboard, press the
highest Bb (`Bb7`, MIDI note 94) to start the rhythm, or send
`F0 7D 01 F7` via SysEx.

For **Sync Start**, hold the highest C (`C8`, MIDI note 96) — keep it
held — then press `Bb7` and release (or send `F0 7D 03 F7`). The rhythm
will start as soon as you play a chord.

Chords are detected only on the channel selected as **Chord Ch** in the
setup screen. A 3-note recognition algorithm is used, supporting chord
inversions (C, C/G, C/D, etc.). The current chord and tempo are shown on
screen.

To load a different style, use the SysEx command:

```
F0 7D 20 <NN> F7
```

where `<NN>` is the style number (0–127). For example, to load
`style36.mid`, send `F0 7D 20 24 F7` (0x24 = 36).

**2. After selecting a style, how do I adjust the tempo?**

Hold Shift (`C8`, MIDI note 96) and press `C#7` (MIDI note 85) to
increase tempo, or `Eb7` (MIDI note 87) to decrease it. Each press
changes the tempo by 1. Alternatively, send `F0 7D 07 F7` (Tempo +)
or `F0 7D 08 F7` (Tempo −) via SysEx.

**3. My old style files no longer load — why?**

You are probably using style files created for an earlier version of
SRA, with the old MIDI channel layout. Run `./convert_style.sh` on them
(see "Migrating Old Style Files" above).

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

### SysEx Errors

SysEx errors do not abort the engine. They are printed to standard error
in the following format:

```
SRA SysEx error: <description> (CMD=<cmd>, len=<len>)
```

Typical messages:

| Message | Cause |
|---------|-------|
| `missing CMD byte` | `F0 7D F7` — no command byte |
| `message too long` | SysEx payload exceeds 256 bytes |
| `CMD 0x20 requires 1 data byte` | Load Style without a style number |
| `CMD 0x20 data out of range` | Style number > 127 |
| `CMD 0x50 requires 1 data byte` | Enable/disable command without a value |
| `CMD 0x50 data must be 0 or 1` | Value other than 0 or 1 |
| `CMD 0x51 requires 1 data byte` | Set Chord Channel without a value |
| `CMD 0x51 data out of range (0..15)` | Channel > 15 |

---

## Project Plans and Goals

- Development focus will remain on evolving and extending **libsracore**'s core functionality and flexibility. UI and interface work (e.g. adding support for Qt, SDL, JACK, etc.) is out of scope for this project.
- If you wish to improve or adapt the UI or interface, please **fork** this project.
- As libsracore evolves, breaking changes to the opaque pointer API, callbacks, MIDI file format, and related interfaces may occur in future versions.

---

Compatible soundfonts for SRA: https://archive.org/details/SF_zzdenis
