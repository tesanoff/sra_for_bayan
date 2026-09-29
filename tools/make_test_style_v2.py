#!/usr/bin/env python3
"""
make_test_style_v2.py — генератор тестового стиля SRA в формате v2.

SMF Format 0, PPQ 120.  Meta-заголовок формата v2 (FF 7F BE EF 01 ...)
+ простое тело: Intro / Original / Fill / Variation / Fill / Ending,
× 3 блока (C / Cm / C7).

Используется для отладки загрузчика стилей libsracore.
Формат описан в SRA-style-file-description.md.

    python3 tools/make_test_style_v2.py [--out style0.mid]
                                        [--tempo 47] [--beat 4]
                                        [--il 4] [--ml-a 4] [--ml-b 8]
                                        [--el 4]
"""

import argparse
import struct


def vlq(value):
    out = [value & 0x7F]
    value >>= 7
    while value:
        out.insert(0, (value & 0x7F) | 0x80)
        value >>= 7
    return bytes(out)


# --- Meta header -----------------------------------------------------------

def build_meta(tempo, beat, il, ml_a, ml_b, el):
    t_time = 120 * beat
    data = bytes([
        0xBE, 0xEF, 0x01,
        tempo, beat,
        (t_time >> 7) & 0x7F, t_time & 0x7F,
        il, ml_a, ml_b, el,
    ])
    return bytes([0x00, 0xFF, 0x7F, len(data)]) + data


# --- Body ------------------------------------------------------------------

CH_BASS, PROG_BASS = 7, 32
CH_ACC1, PROG_ACC1 = 8, 0
CH_DRUM, PROG_DRUM = 9, 0


def add_bar(buf, first_delta, t_time, notes):
    """
    Один такт.
      first_delta: delta для первого события (NoteOn).
      notes: [(ch, note, vel), ...] — NoteOn в начале такта.
    NoteOff — в конце такта (delta = t_time - 1 для первого).
    ProgChg ставится ПОСЛЕ нот (см. §4.7 описания формата:
    сначала ноты, потом PC).
    """
    # NoteOn — первым событием
    for i, (ch, note, vel) in enumerate(notes):
        buf += vlq(first_delta if i == 0 else 0)
        buf += bytes([0x90 | ch, note, vel])

    # ProgChg — после нот
    for (ch, prog) in ((CH_BASS, PROG_BASS),
                       (CH_ACC1, PROG_ACC1),
                       (CH_DRUM, PROG_DRUM)):
        buf += vlq(0)
        buf += bytes([0xC0 | ch, prog])

    # NoteOff — в конце такта
    for i, (ch, note, _vel) in enumerate(notes):
        buf += vlq((t_time - 1) if i == 0 else 0)
        buf += bytes([0x90 | ch, note, 0])


def add_section(buf, n_bars, t_time, note_fn, first_delta=0):
    """Секция из n_bars тактов.  first_delta — только для первого такта."""
    for bar in range(n_bars):
        add_bar(buf, first_delta if bar == 0 else 0, t_time, note_fn(bar))

def build_body(il, ml_a, ml_b, el, beat):
    t_time = 120 * beat
    buf = bytearray()

    # Гаммы: каждый такт — своя нота, по возрастанию.
    # Так «2×4 такта» не спутать с «1×8 тактов».
    def _note_for_bar(bar, base_octave):
        # base_octave — MIDI-нота C в нужной октаве.
        # Диатоника C-dur: C D E F G A B (полутоны 0,2,4,5,7,9,11).
        steps = [0, 2, 4, 5, 7, 9, 11]
        return base_octave + steps[bar % 7]

    def notes_orig(bar):
        # Original: 4 такта гаммы C4-D4-E4-F4 (низкая октава)
        n = _note_for_bar(bar, 60)   # C4
        return [
            (CH_BASS, n - 24, 90),
            (CH_ACC1, n,      80),
            (CH_DRUM, 38,    100),
        ]

    def notes_var(bar):
        # Variation: 8 тактов гаммы C5-D5-E5-F5-G5-A5-B5-C6 (высокая)
        n = _note_for_bar(bar, 72)   # C5
        return [
            (CH_BASS, n - 24, 90),
            (CH_ACC1, n,      80),
            (CH_DRUM, 38,    100),
        ]

    # first_delta = t_time только для самого первого такта тела
    # (пауза от Meta до первой границы такта).
    first_delta = t_time
    for _ in range(3):
        add_section(buf, il,   t_time, notes_orig, first_delta=first_delta)
        first_delta = 0
        add_section(buf, ml_a, t_time, notes_orig)   # Original: C
        add_section(buf, 1,    t_time, notes_orig)   # Fill
        add_section(buf, ml_b, t_time, notes_var)    # Variation: G
        add_section(buf, 1,    t_time, notes_orig)   # Fill
        add_section(buf, el,   t_time, notes_orig)   # Ending: C

    buf += vlq(0)
    buf += bytes([0xFF, 0x2F, 0x00])
    return bytes(buf)

# --- SMF -------------------------------------------------------------------

def build_smf(tempo, beat, il, ml_a, ml_b, el):
    meta = build_meta(tempo, beat, il, ml_a, ml_b, el)
    body = build_body(il, ml_a, ml_b, el, beat)
    track_data = meta + body
    track = b'MTrk' + struct.pack('>I', len(track_data)) + track_data
    header = b'MThd' + struct.pack('>IHHH', 6, 0, 1, 120)
    return header + track


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--out', default='style0.mid')
    ap.add_argument('--tempo', type=int, default=47)
    ap.add_argument('--beat', type=int, default=4)
    ap.add_argument('--il', type=int, default=4)
    ap.add_argument('--ml-a', type=int, default=4)
    ap.add_argument('--ml-b', type=int, default=8)
    ap.add_argument('--el', type=int, default=4)
    args = ap.parse_args()

    smf = build_smf(args.tempo, args.beat,
                    args.il, args.ml_a, args.ml_b, args.el)
    with open(args.out, 'wb') as f:
        f.write(smf)
    print(f"Written: {args.out} ({len(smf)} bytes)")


if __name__ == '__main__':
    main()
