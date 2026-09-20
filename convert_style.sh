#!/bin/bash
# convert_style.sh — migrate SRA style files to the new MIDI channel
# layout.  Old channels are remapped as follows:
#
#   channel 1 -> 7   (ACCBASS)
#   channel 4 -> 8   (ACC1)
#   channel 5 -> 10  (ACC2)
#   channel 6 -> 11  (ACC3)
#   channel 7 -> 12  (ACC4)
#
# Channels 0 and 9 are left untouched:
#   channel 9  = DRUM (GM standard, unchanged)
#   channel 0  = SRA style header (only in the first bar)
#
# Usage:
#   ./convert_style.sh style0.mid
#   ./convert_style.sh style*.mid
#
# For each file:
#   * a backup <file>.bak is created (if not already present)
#   * the file is converted to CSV, channels are remapped, then
#     converted back to MIDI
#   * the result is verified by re-converting to CSV and diffing
#   * if anything goes wrong, the original is restored
#
# Requires: midicsv, csvmidi  (Debian/Ubuntu: sudo apt install midicsv)

set -u

# ── Check for required tools ────────────────────────────────────────
for tool in midicsv csvmidi; do
    if ! command -v "$tool" >/dev/null 2>&1; then
        echo "error: '$tool' not found in PATH" >&2
        echo "install it with:  sudo apt install midicsv" >&2
        exit 1
    fi
done

if [ $# -eq 0 ]; then
    echo "usage: $0 <file.mid> [<file.mid> ...]" >&2
    exit 1
fi

exit_code=0

# ── Process each file ───────────────────────────────────────────────
for src in "$@"; do
    if [ ! -f "$src" ]; then
        echo "skip: $src (not a file)" >&2
        exit_code=1
        continue
    fi

    echo "==> $src"

    bak="$src.bak"
    csv="$src.csv"
    check="$src.check.csv"
    tmp="$src.tmp.mid"

    # Backup (do not overwrite an existing backup).
    if [ ! -f "$bak" ]; then
        if ! cp -p "$src" "$bak"; then
            echo "  error: cannot create backup $bak" >&2
            exit_code=1
            continue
        fi
        echo "  backup: $bak"
    else
        echo "  backup already exists: $bak"
    fi

    # MIDI -> CSV
    if ! midicsv "$src" "$csv"; then
        echo "  error: midicsv failed" >&2
        rm -f "$csv"
        exit_code=1
        continue
    fi

    # Sanity check: which channels are present?
    # We look at all *_c events with a channel field.  Channel 0 is
    # allowed (header); 1, 4, 5, 6, 7, 9 are the known old channels.
    unknown_channels=$(awk -F', *' '
        /_c,/ {
            ch = $4
            if (ch == 0 || ch == 1 || ch == 4 || ch == 5 || ch == 6 || ch == 7 || ch == 9) next
            if (ch ~ /^[0-9]+$/) print ch
        }
    ' "$csv" | sort -n -u)

    if [ -n "$unknown_channels" ]; then
        echo "  error: unexpected MIDI channels present: $unknown_channels" >&2
        echo "         file not migrated (may not be a valid SRA style)" >&2
        rm -f "$csv"
        exit_code=1
        continue
    fi

    # Channel remapping.  We use letter placeholders first to avoid
    # cascading substitutions (e.g. 1 -> 7 and then 7 -> 12).
    sed -i -E 's/^([0-9]+, *[0-9]+, *[A-Za-z_]+_c), *1,/\1, X,/' "$csv"
    sed -i -E 's/^([0-9]+, *[0-9]+, *[A-Za-z_]+_c), *4,/\1, Y,/' "$csv"
    sed -i -E 's/^([0-9]+, *[0-9]+, *[A-Za-z_]+_c), *5,/\1, Z,/' "$csv"
    sed -i -E 's/^([0-9]+, *[0-9]+, *[A-Za-z_]+_c), *6,/\1, W,/' "$csv"
    sed -i -E 's/^([0-9]+, *[0-9]+, *[A-Za-z_]+_c), *7,/\1, V,/' "$csv"
    sed -i -E 's/^([0-9]+, *[0-9]+, *[A-Za-z_]+_c), *X,/\1, 7,/'  "$csv"
    sed -i -E 's/^([0-9]+, *[0-9]+, *[A-Za-z_]+_c), *Y,/\1, 8,/'  "$csv"
    sed -i -E 's/^([0-9]+, *[0-9]+, *[A-Za-z_]+_c), *Z,/\1, 10,/' "$csv"
    sed -i -E 's/^([0-9]+, *[0-9]+, *[A-Za-z_]+_c), *W,/\1, 11,/' "$csv"
    sed -i -E 's/^([0-9]+, *[0-9]+, *[A-Za-z_]+_c), *V,/\1, 12,/' "$csv"

    # CSV -> MIDI (temporary file)
    if ! csvmidi "$csv" "$tmp"; then
        echo "  error: csvmidi failed" >&2
        rm -f "$csv" "$tmp"
        exit_code=1
        continue
    fi

    # Verify: re-convert to CSV and diff against our modified CSV.
    if ! midicsv "$tmp" "$check"; then
        echo "  error: verification midicsv failed" >&2
        rm -f "$csv" "$check" "$tmp"
        exit_code=1
        continue
    fi

    if ! diff -q "$csv" "$check" >/dev/null; then
        echo "  error: verification failed (csvmidi did not round-trip cleanly)" >&2
        echo "         file not migrated; original restored" >&2
        rm -f "$csv" "$check" "$tmp"
        exit_code=1
        continue
    fi

    # Replace original with the migrated file.
    if ! mv "$tmp" "$src"; then
        echo "  error: cannot replace $src" >&2
        rm -f "$csv" "$check" "$tmp"
        exit_code=1
        continue
    fi

    echo "  migrated: channels 1->7, 4->8, 5->10, 6->11, 7->12"

    # Clean up intermediate files.
    rm -f "$csv" "$check"
done

exit $exit_code