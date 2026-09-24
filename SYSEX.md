# SRA SysEx Control Protocol — Command Reference

Complete reference for the SysEx control protocol of SRA
(Software-based Real-time Arranger).  This document describes every
implemented command, its exact behaviour as of the current version,
and its interaction with other commands.

- For a general overview of SRA, see [README.md](README.md).
- For style file format, see [SRA-style-file-description.md](SRA-style-file-description.md).

---

## 1. Message format

```
F0 7D <CMD> [<DATA...>] F7
```

| Byte | Meaning |
|------|---------|
| `F0` | SysEx start |
| `7D` | Manufacturer ID (non-commercial) |
| `<CMD>` | Command byte (see the table below) |
| `<DATA...>` | Optional parameter bytes (0–127 each, 7-bit) |
| `F7` | SysEx end |

The `F0` / `F7` framing bytes are stripped by the platform layer
before the message reaches the engine; the dispatcher sees only
`[0x7D, CMD, DATA...]`.

### Data length

Each command specifies how many data bytes it expects:

- **0 bytes** — no data; any extra byte is an error.
- **1 byte** — exactly one 7-bit value (`00`–`7F`).

Malformed messages (wrong length, out-of-range value) are reported to
`stderr` and **silently ignored** — they never abort the engine.

### Unknown commands

Unknown `CMD` values are **silently ignored**.  This is intentional:
the protocol is designed to be forward-compatible, so a newer sender
can use commands the receiver does not yet understand without
breaking anything.

### Channel numbering

SysEx commands that take a MIDI channel use **0-based** numbering
(0–15), following MIDI convention:

- `00` = MIDI channel 1
- `02` = MIDI channel 3
- `0F` = MIDI channel 16

This differs from the config file and the `--chord-ch` command-line
option, which are **1-based** (1–16) to match the UI.

---

## 2. Engine model (minimal context)

To make the command descriptions meaningful, a short overview of the
relevant engine state.

### Playback state

- **Stopped** — the arranger is idle.  Chord detection may be active
  (see `Change Mode`), but no style data is being played.
- **Running** — the arranger is playing a style.  Styles consist of
  **sections** (Intro, Original, Fill, Variation, Ending) that
  auto-advance at bar boundaries.

### Sections

| Section | Meaning |
|---------|---------|
| Intro   | Opening section, plays `IL` bars, then goes to Original. |
| Original | Main section (variation A), loops for `ML` bars. |
| Fill    | One-bar transition.  Plays, then returns to Original or Variation. |
| Variation | Alternative main section (variation B), loops for `ML` bars. |
| Ending  | Closing section, plays `EL` bars, then stops. |

The current section is selected by two flags:

- `var_f` — 0 = Original, 1 = Variation.
- `session` — internal index (Intro = 0, Original = 1, fill-to-var = 2,
  Variation = 3, fill-to-orig = 4, Ending = 5).

Fill commands set `fill_f = 1`; the engine inserts a fill bar and
switches the main section at the bar boundary.  The "To Original" /
"To Variation" commands (0x0F / 0x10) set `var_f` directly, without
a fill — the switch happens **at the next bar boundary**.

### Chord detection

Notes arriving on the **chord channel** are analysed as chords (when
the arranger is active).  Notes on all other channels are forwarded
to the output as live playing, on their original channel.

The chord channel is configurable via `--chord-ch` (1-based) or SysEx
`0x51` (0-based).

Notes on the arranger-owned output channels (see the table below)
are filtered only while the arranger is running: during playback
they are ignored on input, to avoid mixing with the generated
parts; in Stop they pass through unchanged.  Note-On control
commands (`Start`, `Fill`, `Tempo`, part toggles) are never
interpreted on arranger-owned channels, in either state.

### Arranger-owned output channels

SRA generates accompaniment on a fixed set of MIDI channels.
Input on these channels is filtered **only while the arranger is
running** (`start_f == 1`), to avoid feedback when MIDI OUT is
looped back to MIDI IN.  In Stop, all channels pass through
unchanged.  See "Chord detection" above.

| Role | 0-based | UI |
|------|:---:|:---:|
| Acc5   | 6  | 7  |
| Acc.Bass | 7 | 8 |
| Acc1   | 8  | 9  |
| Drum   | 9  | 10 |
| Acc2   | 10 | 11 |
| Acc3   | 11 | 12 |
| Acc4   | 12 | 13 |
| M.Bass | 13 | 14 |
| Lower  | 14 | 15 |
| Phrase | 15 | 16 |

Channels **0–5** (UI 1–6) are free for live playing.

---

## 3. Command table

| CMD  | Function | Data |
|------|----------|------|
| `01` | Start | — |
| `02` | Stop | — |
| `03` | Sync Start | — |
| `04` | Fill to Original | — |
| `05` | Fill to Variation | — |
| `06` | Intro / Ending | — |
| `07` | Tempo + | — |
| `08` | Tempo − | — |
| `09` | Toggle M.Bass | — |
| `0A` | Toggle Acc. | — |
| `0B` | Toggle Acc.Bass | — |
| `0C` | Toggle Drum | — |
| `0D` | Fade Out | — |
| `0E` | Change Mode | — |
| `0F` | To Original (no fill) | — |
| `10` | To Variation (no fill) | — |
| `20` | Load Style | style number (0–127) |
| `50` | Enable / disable Note-On commands | `00` = off, `01` = on |
| `51` | Set chord channel | channel, 0-based (0–15) |
| `52` | Master Volume | volume (0–127) |
| `53` | Toggle Lower | — |

Commands are described in detail below, grouped by function.

---

## 4. Transport

### `01` — Start

Starts playback.

- **If stopped:** initialises the engine, resets all voices, and
  begins playback.  The starting section is set **explicitly**
  from the internal flags `ief` (Intro/Ending armed) and `var_f`
  (Original / Variation): Intro if `ief` is set, otherwise
  Original or Variation according to `var_f`.
- **If running:** **toggles to stopped** — this is the same code path
  as the Stop command (see below).  Calling `01` twice starts, then
  stops.
- **If `Sync Start` (`03`) was armed:** this command is refused
  (returns immediately without starting), because the arranger is
  waiting for the first chord.

Data: none.

**Example:** `F0 7D 01 F7`

---

### `02` — Stop

Stops playback.

- **If running:** stops the arranger.  All sounding notes are
  released, MIDI clock (if enabled) is stopped.
- **If already stopped:** does nothing.

Data: none.

**Example:** `F0 7D 02 F7`

---

### `03` — Sync Start

Arms the arranger; playback starts automatically on the first chord.

- **If already running:** does nothing.
- **If already armed:** does nothing (idempotent).
- **Otherwise:** sets a pending flag.  When the next chord is
  detected (on the chord channel), playback starts automatically.

This is the SysEx equivalent of the Note-On `Shift + Bb7` gesture.

Data: none.

**Example:** `F0 7D 03 F7`

---

## 5. Sections

### `04` — Fill to Original

Plays a fill bar, then switches to the Original section.

- **If the arranger is not running:** sets `var_f = 0` but does **not**
  play a fill.  Use `0F` for a pure section switch.
- **If running:** sets `fill_f = 1` and `var_f = 0`.  A fill bar is
  inserted at the next bar boundary, then the Original section plays.
- **If a fill is already pending:** the new request replaces the old.

Data: none.

**Example:** `F0 7D 04 F7`

---

### `05` — Fill to Variation

Same as `04`, but switches to Variation (`var_f = 1`).

Data: none.

**Example:** `F0 7D 05 F7`

---

### `06` — Intro / Ending

Arms the Intro or Ending section.

- **If the arranger is stopped:** arms the Intro.  The next `Start`
  (or chord, in Sync Start mode) begins with the Intro section.
- **If the arranger is running:** arms the Ending.  The current
  section finishes its current bar, then transitions to the Ending
  at the next bar boundary.
- **If a fill is currently playing:** the command is deferred —
  `ief` is set, but the switch to Ending happens only at the next
  bar boundary **after** the fill completes.

The command is a **toggle at the semantic level**: the same command
selects Intro (when stopped) or Ending (when running).  There is no
separate "arm Intro" vs "arm Ending" command.

Data: none.

**Example:** `F0 7D 06 F7`

---

### `0F` — To Original (no fill)

Sets `var_f = 0` — the engine will switch to Original at the next
bar boundary.  **No fill is played.**

- **If the arranger is not running:** sets `var_f` but has no
  immediate audible effect.  The next Start will begin on Original.
- **If running:** the switch happens at the next bar boundary.
- **If a fill is already pending:** the fill is **not** cancelled —
  `fill_f` remains set.  The section switch happens after the fill.

Data: none.

**Example:** `F0 7D 0F F7`

---

### `10` — To Variation (no fill)

Same as `0F`, but sets `var_f = 1` (Variation).

Data: none.

**Example:** `F0 7D 10 F7`

---

## 6. Tempo

### `07` — Tempo +

Increases the tempo by 1 BPM.

- Range: 20–250.  Values at the upper bound are silently ignored.
- The change takes effect immediately: the tick duration is
  recalculated and the next step uses the new tempo.

Data: none.

**Example:** `F0 7D 07 F7`

---

### `08` — Tempo −

Decreases the tempo by 1 BPM.  Same range and immediacy as `07`.

Data: none.

**Example:** `F0 7D 08 F7`

---

## 7. Part toggles

These commands enable or disable a generated accompaniment part.
Each is a **toggle** — calling it twice returns to the original state.
The default for all parts is **enabled** (`1`): M.Bass, Acc.,
Acc.Bass, Drum, and Lower all start enabled.

When a part is disabled, its velocity multiplier becomes 0, so the
notes are generated as Note-On with velocity 0 (equivalent to
Note-Off).  The notes are still sent, but they are silent.

### `09` — Toggle M.Bass

Toggles the **manual bass** voice (channel 13 / UI 14).

While the arranger is **stopped** (`start_f == 0`) and chord
mode is on (`mode == 1`), M.Bass plays the **lowest note of
the currently held chord**.  Velocity is that of the lowest
note, multiplied by this toggle (`0` or `1`).

M.Bass **never sounds during playback** — the accompaniment
bass comes from the style, on the Acc.Bass channel instead.

The default patch is 35 (fretless bass), set by `sra_reset`.

### `0A` — Toggle Acc.

Toggles Acc1–Acc4 and Acc5 (channels 8, 10, 11, 12, 6 / UI 9, 11, 12, 13, 7).

### `0B` — Toggle Acc.Bass

Toggles the auto-bass part (channel 7 / UI 8).

### `0C` — Toggle Drum

Toggles the drum part (channel 9 / UI 10).

### `53` — Toggle Lower

Toggles the **Lower** voice (channel 14 / UI 15) — all notes of
the held chord, transposed up by +12 semitones (plus internal
offsets).

Unlike M.Bass, Lower **sounds both while stopped and during
playback** — it echoes the chord you play, on top of the style
accompaniment.  It is silenced only when:

- `Change Mode` is off (`mode == 0`), or
- this toggle is off (`lower_vf == 0`), or
- the engine is in an Intro or Ending section (internal
  `voice_lock`).

Unlike the other toggles, `53` **immediately re-triggers** the
current chord (`sra_chord_off` + `sra_chord_on`), so currently
sounding Lower notes are cleanly replaced.  This avoids stuck notes.

The Lower voice is **independent of `Change Mode`** (`0E`):
toggling `0E` does not change the Lower state, and vice versa.

Data: none for all five toggles.

**Examples:** `F0 7D 09 F7`, `F0 7D 53 F7`

---

## 8. Volume and effects

### `0D` — Fade Out

Starts or cancels a fade-out.

The engine keeps a three-state flag `fadeout_f`:

| State | Meaning |
|-------|---------|
| `-1` | Idle (no fade in progress) |
| `126 … 1` | Fade-out in progress; value is the current volume |
| `-2` | Fade-out cancelled; volume restores to 127 |

> **Note.** `127` is only the *starting* moment: the first fade
> step moves the value to `126`.  The final value is `1`, not `0`
> — the fade stops one step short of silence.

Behaviour of the command:

- **From idle (`-1`):** starts a fade-out.  Volume decreases
  smoothly from 127 to 0 over several bars (speed depends on tempo).
- **During fade (`> 0`):** cancels the fade.  Volume is
  **immediately** restored to 127 (not gradually).
- **After a fade has completed (state 0, effectively idle again):**
  starts a new fade-out on the next call.

The fade applies **CC11 (Expression)**, not CC7.  This means it
multiplies on top of the current Master Volume (see `52`).

Data: none.

**Example:** `F0 7D 0D F7`

---

### `52` — Master Volume

Sets the Channel Volume (CC7) on all arranger-owned channels
(LOWER, M.Bass, Acc1–Acc5, Acc.Bass, Drum, Phrase — 10 channels).

```
F0 7D 52 <VOL> F7
```

- `<VOL>` is a 7-bit value (0–127).
- The value is **applied immediately** — CC7 messages are sent to
  all 10 channels.
- The value is **stored** in the engine and re-applied on every
  `sra_reset()` (i.e. on every Start), instead of the default 100.

Fade Out is unaffected: it operates on CC11 (Expression) and
continues to work on top of the current Master Volume.

Data: 1 byte, 0–127.

**Errors:**
- `CMD 0x52 requires 1 data byte` — wrong length.
- `CMD 0x52 data out of range (0..127)` — value > 127.

**Example (volume 80):** `F0 7D 52 50 F7`

---

## 9. Mode and routing

### `0E` — Change Mode

Toggles the arranger's **chord mode** flag.

The `mode` flag affects two things:

1. **Chord detection before Start.**  When `mode = 1`, notes on the
   chord channel are analysed as chords even when the arranger is
   not running.  This allows playing chords (and hearing Lower /
   M.Bass) without starting the accompaniment.
2. **Lower and M.Bass velocity.**  When `mode = 0`, both Lower and
   M.Bass are silent, regardless of their own toggles.  When
   `mode = 1`, they play according to their respective toggles
   (`53` for Lower, `09` for M.Bass).

The default is `mode = 0` (chord detection before Start is off;
Lower and M.Bass are silent until the mode is enabled).

On toggle **from 1 to 0**, any sounding chord is released
(`sra_chord_off`) and the chord key tracking is cleared
(`key_on_count = 0`) — so no notes are left hanging.

Data: none.

**Example:** `F0 7D 0E F7`

---

### `50` — Enable / disable Note-On commands

Enables or disables the legacy Note-On control commands (Start,
Fill, Tempo, part toggles, etc.).

```
F0 7D 50 <VAL> F7
```

- `<VAL>` = `00` — disable Note-On commands.
- `<VAL>` = `01` — enable Note-On commands (default).

When disabled:

- Note-On commands (the `CMD_*` note ranges) are **not processed**.
- **Chord detection still works** on the chord channel.
- **SysEx commands are unaffected** — the protocol always works,
  regardless of this setting.

This is useful when the Note-On command zone overlaps with actual
musical notes (e.g. on a 49-key keyboard with `--ctrl-offset -1`).

Data: 1 byte, 0 or 1.

**Errors:**
- `CMD 0x50 requires 1 data byte` — wrong length.
- `CMD 0x50 data must be 0 or 1` — value not 0 or 1.

**Examples:** `F0 7D 50 00 F7` (disable), `F0 7D 50 01 F7` (enable).

---

### `51` — Set chord channel

Sets the MIDI channel on which notes are analysed as chords.

```
F0 7D 51 <CH> F7
```

- `<CH>` is a **0-based** channel number (0–15).
- The change is **applied immediately**: the next incoming note on
  the new channel will be treated as a chord note.
- Notes already held on the old chord channel remain held; they are
  not re-classified.

Data: 1 byte, 0–15.

**Errors:**
- `CMD 0x51 requires 1 data byte` — wrong length.
- `CMD 0x51 data out of range (0..15)` — value > 15.

**Example (channel 4, 0-based 3):** `F0 7D 51 03 F7`

---

## 10. Styles

### `20` — Load Style

Loads a style file.

```
F0 7D 20 <NN> F7
```

- `<NN>` is the style number (0–127).  SRA loads `style<NN>.mid`
  from the configured style directory.
- The change is **applied immediately**: if the arranger is running,
  the current section is interrupted and the new style begins from
  the start of the current section (Intro, Original, etc.).
- **Silent failure:** if the file does not exist or is malformed,
  the command is ignored — no error is reported to the sender.
  The current style remains loaded.

Data: 1 byte, 0–127.

**Errors:**
- `CMD 0x20 requires 1 data byte` — wrong length.
- `CMD 0x20 data out of range` — value > 127.

**Example (style 5):** `F0 7D 20 05 F7`

---

## 11. Error handling

### Message errors

Malformed SysEx messages with our Manufacturer ID (`0x7D`) are
reported to `stderr` in this format:

```
SRA SysEx error: <description> (CMD=<cmd>, len=<len>)
```

The message is then **dropped** — the engine continues running.

Possible error messages:

| Message | Cause |
|---------|-------|
| `missing CMD byte` | `F0 7D F7` — Manufacturer ID present, no command byte |
| `message too long` | SysEx payload exceeds 256 bytes total (see §1) |
| `CMD 0x01 takes no data` | Start with unexpected data |
| `CMD 0x02 takes no data` | Stop with unexpected data |
| `CMD 0x03 takes no data` | Sync Start with unexpected data |
| `CMD 0x04 takes no data` | Fill to Original with unexpected data |
| `CMD 0x05 takes no data` | Fill to Variation with unexpected data |
| `CMD 0x06 takes no data` | Intro / Ending with unexpected data |
| `CMD 0x07 takes no data` | Tempo + with unexpected data |
| `CMD 0x08 takes no data` | Tempo − with unexpected data |
| `CMD 0x0D takes no data` | Fade Out with unexpected data |
| `CMD 0x0E takes no data` | Change Mode with unexpected data |
| `CMD 0x0F takes no data` | To Original with unexpected data |
| `CMD 0x10 takes no data` | To Variation with unexpected data |
| `CMD 0x53 takes no data` | Toggle Lower with unexpected data |
| `CMD 0x20 requires 1 data byte` | Load Style without a style number |
| `CMD 0x20 data out of range` | Style number > 127 |
| `CMD 0x50 requires 1 data byte` | Note-On enable without a value |
| `CMD 0x50 data must be 0 or 1` | Value other than 0 or 1 |
| `CMD 0x51 requires 1 data byte` | Set Chord Channel without a value |
| `CMD 0x51 data out of range (0..15)` | Channel > 15 |
| `CMD 0x52 requires 1 data byte` | Master Volume without a value |
| `CMD 0x52 data out of range (0..127)` | Volume > 127 |

### Unknown commands

Unknown `CMD` values are ignored silently — no error message.

### Unknown manufacturer

Messages with a Manufacturer ID other than `0x7D` are ignored
silently, without any check on their content.

### Oversized messages

Messages longer than **256 bytes total** (including the `7D`
Manufacturer ID and the `CMD` byte, but excluding the `F0` / `F7`
framing) are reported as `message too long` and dropped.  This
limit is enforced by the platform layer, not by the dispatcher.

---

## 12. Examples

### Basic transport

Start the arranger:

```
F0 7D 01 F7
```

Stop:

```
F0 7D 02 F7
```

Sync Start — wait for the first chord:

```
F0 7D 03 F7
```

### Section switching

Go to Variation with a fill:

```
F0 7D 05 F7
```

Go back to Original without a fill (switch at next bar):

```
F0 7D 0F F7
```

Arm the Ending (if running) or Intro (if stopped):

```
F0 7D 06 F7
```

### Real-time control

Reduce the tempo by 10 BPM:

```
F0 7D 08 F7    (×10)
```

Set master volume to 64 (half):

```
F0 7D 52 40 F7
```

Fade out / cancel fade:

```
F0 7D 0D F7
```

Cancel the fade (restore volume immediately):

```
F0 7D 0D F7
```

> **Note.** Both actions use the **same command** `F0 7D 0D F7`.
> The first call starts the fade; a second call while the fade is
> in progress cancels it and restores the volume to 127
> immediately.  There is no separate "cancel fade" command.

### Loading a style

Load `style36.mid`:

```
F0 7D 20 24 F7
```

### Setup

Disable Note-On commands (use only SysEx):

```
F0 7D 50 00 F7
```

Set the chord channel to MIDI channel 4 (0-based 3):

```
F0 7D 51 03 F7
```

### Sending SysEx from a shell

On Linux with ALSA (`amidi` from `alsa-utils`):

```sh
amidi -p hw:5,2 -S 'F0 7D 01 F7'
```

On PipeWire systems, `amidi` may not see your devices; use
`pw-link` / `qpwgraph` / `helvum` for routing, and `pw-cli` for
inspection.

On Windows, use `sendmidi` or a similar MIDI utility.

---

## 13. Comparison with Note-On commands

Every SysEx command has a Note-On equivalent (or vice versa), except
where noted.  This table is useful for users migrating from the
deprecated Note-On control.

| SysEx | Note-On | Notes |
|-------|---------|-------|
| `01` Start | `Bb7` (94) | Same effect. |
| `02` Stop | `Bb7` (94) | There is no separate Note-On Stop
  command: Stop is the **same code path** as Start (toggle).
  Pressing `Bb7` again stops the arranger. |
| `03` Sync Start | `Shift + Bb7` (96+94) | Same effect. |
| `04` Fill to Original | `B7` (95) | Same effect. |
| `05` Fill to Variation | `A7` (93) | Same effect. |
| `06` Intro / Ending | `Ab7` (92) | Same effect. |
| `07` Tempo + | `Shift + Eb7` (96+87) | Same effect. |
| `08` Tempo − | `Shift + C#7` (96+85) | Same effect. |
| `09` Toggle M.Bass | `Shift + F7` (96+89) | Same effect. |
| `0A` Toggle Acc. | `Shift + E7` (96+88) | Same effect. |
| `0B` Toggle Acc.Bass | `Shift + D7` (96+86) | Same effect. |
| `0C` Toggle Drum | `Shift + C7` (96+84) | Same effect. |
| `0D` Fade Out | `Shift + F#7` (96+90) | Same effect. |
| `0E` Change Mode | `Shift + G7` (96+91) | Same effect. |
| `0F` To Original | `Shift + B7` (96+95) | Same effect. |
| `10` To Variation | `Shift + A7` (96+93) | Same effect. |
| `20` Load Style | — | SysEx only. |
| `50` Enable/disable Note-On | — | SysEx only. |
| `51` Set chord channel | — | SysEx only. |
| `52` Master Volume | — | SysEx only. |
| `53` Toggle Lower | — | SysEx only. |

Note-On commands can be disabled with `50 00`, but SysEx commands
always work.

---

## 14. Protocol versioning

This document describes the protocol as implemented in SRA 4.06
(fork by Alexander Tesanov).  The protocol is **not versioned** —
there is no version byte in the messages.  New commands may be added
in future versions; old commands will not change semantics without
a corresponding bump in the SRA version number.

Unknown commands are silently ignored by design, so a sender written
for a newer SRA will not break an older one — it will just have no
effect on the unsupported commands.