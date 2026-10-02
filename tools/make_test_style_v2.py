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
                                        [--running-status]
                                        [--ch11 N]
                                        [--cc]
                                        [--pitch-bend]
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
CH_ACC3, PROG_ACC3 = 11, 49   # ACC3 = "тянущиеся" звуки


# Диатоника C-dur: полутоны от C
C_MAJOR = [0, 2, 4, 5, 7, 9, 11]
# C natural minor (для minor-блока)
C_MINOR = [0, 2, 3, 5, 7, 8, 10]


def add_note(buf, delta, status, note, vel, last_status, running_status):
    """Записать NoteOn/NoteOff.  Если running_status и status == last_status,
       пишем только data1+data2."""
    buf += vlq(delta)
    if running_status and status == last_status:
        buf += bytes([note, vel])
    else:
        buf += bytes([status, note, vel])
    return status


def add_bar(buf, first_delta, t_time, notes, opts):
    """
    Один такт.
      first_delta: delta для первого события (NoteOn).
      notes: [(ch, note, vel), ...] — NoteOn в начале такта.
      opts: dict (running_status, cc, pitch_bend).
    NoteOff — в конце такта.
    ProgChg — после нот.
    """
    running = opts.get('running_status', False)
    last_status = None

    # --- NoteOn ------------------------------------------------------------
    for i, (ch, note, vel) in enumerate(notes):
        status = 0x90 | ch
        delta = first_delta if i == 0 else 0
        last_status = add_note(buf, delta, status, note, vel,
                               last_status, running)

    # --- CC (volume) -------------------------------------------------------
    if opts.get('cc'):
        # CC 7 (volume) на CH_ACC1, CH_ACC3
        for ch in (CH_ACC1, CH_ACC3):
            status = 0xB0 | ch
            buf += vlq(0)
            if running and status == last_status:
                buf += bytes([7, 100])
            else:
                buf += bytes([status, 7, 100])
            last_status = status

    # --- Pitch bend --------------------------------------------------------
    if opts.get('pitch_bend'):
        # Pitch bend на CH_ACC1 (центр = 0x2000)
        status = 0xE0 | CH_ACC1
        buf += vlq(0)
        if running and status == last_status:
            buf += bytes([0x00, 0x40])
        else:
            buf += bytes([status, 0x00, 0x40])
        last_status = status

    # --- Program Change ---------------------------------------------------
    for (ch, prog) in ((CH_BASS, PROG_BASS),
                       (CH_ACC1, PROG_ACC1),
                       (CH_DRUM, PROG_DRUM),
                       (CH_ACC3, PROG_ACC3)):
        status = 0xC0 | ch
        buf += vlq(0)
        if running and status == last_status:
            buf += bytes([prog])
        else:
            buf += bytes([status, prog])
        last_status = status

    # --- NoteOff -----------------------------------------------------------
    # Первый NoteOff — delta = t_time (конец такта).  NoteOn был delta=0
    # в начале такта, поэтому NoteOff смещается ровно на t_time вперёд.
    for i, (ch, note, _vel) in enumerate(notes):
        status = 0x90 | ch   # NoteOn с vel=0 = NoteOff
        delta = t_time if i == 0 else 0
        last_status = add_note(buf, delta, status, note, 0,
                               last_status, running)

    return last_status


def add_section(buf, n_bars, t_time, note_fn, opts, first_delta=0):
    """Секция из n_bars тактов.  first_delta — только для первого такта."""
    last = None
    for bar in range(n_bars):
        d = first_delta if bar == 0 else 0
        last = add_bar(buf, d, t_time, note_fn(bar), opts)
    return last


def build_body(il, ml_a, ml_b, el, beat, opts):
    t_time = 120 * beat
    buf = bytearray()

    def _note_for_bar(bar, scale, base_octave):
        return base_octave + scale[bar % len(scale)]

    def make_notes_orig(scale, base_octave, ch11_base):
        """Original: 4 такта гаммы по scale."""
        def fn(bar):
            n = _note_for_bar(bar, scale, base_octave)
            n11 = _note_for_bar(bar, scale, ch11_base)
            notes = [
                (CH_BASS, n - 24, 90),
                (CH_ACC1, n,      80),
                (CH_DRUM, 38,    100),
            ]
            # ch=11 — аккорд из 3 нот подряд (root, 3rd, 5th),
            # чтобы running status реально сжимал поток.
            if opts.get('ch11'):
                notes.append((CH_ACC3, n11,      90))
                notes.append((CH_ACC3, n11 + 4,  90))
                notes.append((CH_ACC3, n11 + 7,  90))
            return notes
        return fn

    def make_notes_var(scale, base_octave, ch11_base):
        """Variation: 8 тактов гаммы по scale (высокая октава)."""
        def fn(bar):
            n = _note_for_bar(bar, scale, base_octave)
            n11 = _note_for_bar(bar, scale, ch11_base)
            notes = [
                (CH_BASS, n - 24, 90),
                (CH_ACC1, n,      80),
                (CH_DRUM, 38,    100),
            ]
            if opts.get('ch11'):
                notes.append((CH_ACC3, n11,      90))
                notes.append((CH_ACC3, n11 + 4,  90))
                notes.append((CH_ACC3, n11 + 7,  90))
            return notes
        return fn

    # Три блока: major (C), minor (Cm), other (C7)
    # Для major — C-dur; для minor — C-minor; для other — C-dur (пока).
    blocks = [
        # (scale, base_orig, base_var, ch11_orig, ch11_var)
        (C_MAJOR, 60, 72, 60, 72),   # major
        (C_MINOR, 60, 72, 60, 72),   # minor
        (C_MAJOR, 60, 72, 60, 72),   # other (упрощённо: C-dur)
    ]

    first_delta = t_time
    for (scale, b_orig, b_var, c11_orig, c11_var) in blocks:
        notes_orig = make_notes_orig(scale, b_orig, c11_orig)
        notes_var  = make_notes_var (scale, b_var,  c11_var)

        add_section(buf, il,   t_time, notes_orig, opts, first_delta=first_delta)
        first_delta = 0
        add_section(buf, ml_a, t_time, notes_orig, opts)
        add_section(buf, 1,    t_time, notes_orig, opts)   # Fill
        add_section(buf, ml_b, t_time, notes_var,  opts)   # Variation
        add_section(buf, 1,    t_time, notes_orig, opts)   # Fill
        add_section(buf, el,   t_time, notes_orig, opts)   # Ending

    buf += vlq(0)
    buf += bytes([0xFF, 0x2F, 0x00])
    return bytes(buf)


# --- SMF -------------------------------------------------------------------

def build_smf(tempo, beat, il, ml_a, ml_b, el, opts):
    meta = build_meta(tempo, beat, il, ml_a, ml_b, el)
    body = build_body(il, ml_a, ml_b, el, beat, opts)
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
    ap.add_argument('--running-status', action='store_true',
                    help='использовать running status в теле')
    ap.add_argument('--ch11', action='store_true',
                    help='добавить канал 11 (ACC3)')
    ap.add_argument('--cc', action='store_true',
                    help='добавить Control Change (volume)')
    ap.add_argument('--pitch-bend', action='store_true',
                    help='добавить Pitch Bend')
    args = ap.parse_args()

    opts = {
        'running_status': args.running_status,
        'ch11':           args.ch11,
        'cc':             args.cc,
        'pitch_bend':     args.pitch_bend,
    }

    smf = build_smf(args.tempo, args.beat,
                    args.il, args.ml_a, args.ml_b, args.el, opts)
    with open(args.out, 'wb') as f:
        f.write(smf)
    print(f"Written: {args.out} ({len(smf)} bytes)")
    print(f"  running_status={opts['running_status']} "
          f"ch11={opts['ch11']} cc={opts['cc']} "
          f"pitch_bend={opts['pitch_bend']}")


if __name__ == '__main__':
    main()