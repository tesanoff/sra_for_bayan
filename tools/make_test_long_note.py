#!/usr/bin/env python3
"""
make_test_long_note.py — тестовый стиль с NoteOn/NoteOff внутри одного
и того же такта, но с большой задержкой (например, 400 тиков).
Используется, чтобы проверить гипотезу: если NoteOff в одном такте
с NoteOn — нота не висит.

PPQ 120, 4/4, t_time = 480.
"""
import struct
import sys

def vlq(v):
    out = [v & 0x7F]; v >>= 7
    while v:
        out.insert(0, (v & 0x7F) | 0x80); v >>= 7
    return bytes(out)

def build():
    t_time = 480
    # Мета: tempo=47, beat=4, t_time=480, il=1, ml_a=2, ml_b=1, el=1
    # (минимум для загрузчика)
    meta = bytes([
        0x00, 0xFF, 0x7F, 0x0B, 0xBE, 0xEF, 0x01,
        47, 4, (t_time >> 7) & 0x7F, t_time & 0x7F,
        1, 2, 1, 1,
    ])
    body = bytearray()
    body += meta
    # Первый такт тела (delta = t_time от Meta)
    first_delta = t_time
    for blk in range(3):
        for bar in range(1 + 2 + 1 + 1 + 1 + 1):  # il+ml_a+fill+ml_b+fill+el
            # NoteOn ch=10 note=69 в НАЧАЛЕ такта
            body += vlq(first_delta if bar == 0 and blk == 0 else 0)
            body += bytes([0x9A, 69, 100])
            # NoteOn ch=8 note=60 (чтобы было сопровождение)
            body += vlq(0); body += bytes([0x98, 60, 80])
            # NoteOff ch=10 note=69 через 400 тиков (в этом же такте!)
            body += vlq(400); body += bytes([0x8A, 69, 0])
            # NoteOff ch=8 note=60 через 30 тиков
            body += vlq(30);  body += bytes([0x88, 60, 0])
            # Остаток такта — пауза
            body += vlq(480 - 400 - 30)  # 50 тиков
            # Программные изменения (для совместимости)
            body += vlq(0); body += bytes([0xC7, 32])
            body += vlq(0); body += bytes([0xC8, 0])
            body += vlq(0); body += bytes([0xC9, 0])
            body += vlq(0); body += bytes([0xCA, 21])
            body += vlq(0); body += bytes([0xCB, 58])
    body += vlq(0); body += bytes([0xFF, 0x2F, 0x00])
    track = b'MTrk' + struct.pack('>I', len(body)) + bytes(body)
    header = b'MThd' + struct.pack('>IHHH', 6, 0, 1, 120)
    return header + track

if __name__ == '__main__':
    out = sys.argv[1] if len(sys.argv) > 1 else '/tmp/test_long_note.mid'
    data = build()
    with open(out, 'wb') as f:
        f.write(data)
    print(f"Written: {out} ({len(data)} bytes)")