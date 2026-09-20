# Building SRA

This document describes how to build SRA from source.

- **Linux**: native build with `gcc`.
- **Windows**: cross-compilation from Linux with MinGW-w64.

The build system is a plain `Makefile` in `src/`.  There is no
`configure` step and no external dependencies beyond the toolchain
listed below.

---

## Linux

### Requirements

- `gcc`
- `make`
- ALSA development headers — on Debian/Ubuntu:

  ```sh
  sudo apt install libasound2-dev
  ```

### Build

```sh
cd src
make
```

### Result

`build/sra` — the Linux binary.

### Run

```sh
cd build
./sra
```

or, with a config file:

```sh
./sra --config /path/to/sra.conf
```

See `README.md` for command-line options and configuration details.

---

## Windows (cross-compilation from Linux)

### Requirements

- `mingw-w64` — on Debian/Ubuntu:

  ```sh
  sudo apt install mingw-w64
  ```

  This installs both `i686-w64-mingw32-gcc` (32-bit) and
  `x86_64-w64-mingw32-gcc` (64-bit).

### Build

```sh
cd src
make win           # 32-bit (default; i686-w64-mingw32-gcc)
make win BITS=64   # 64-bit (x86_64-w64-mingw32-gcc)
```

### Result

`build/srawin.exe` — the Windows binary (PE32 or PE32+ GUI,
depending on `BITS`).

### Notes

- Only the interactive mode is supported on Windows.
  `--daemon` is rejected with an error at startup.
- The default config path is `%APPDATA%\sra\sra.conf`
  (on Linux it is `/etc/sra/sra.conf`).
- SysEx input uses `MM_MIM_LONGDATA`.
- `srawin.exe` is intentionally **never removed** by
  `make clean` / `make win-clean` / `make clean-all`, so a
  working Windows build survives rebuilds of the Linux side.
  Delete the file manually if you need a clean slate.

### Run

Copy `build/srawin.exe` to the Windows machine together with
any style files (`style0.mid`, ...) and run it.  A small setup
window appears; see `README.md` for the UI.

---

## Cleanup

The following targets are available in `src/Makefile`:

| Target | Effect |
|--------|--------|
| `make clean` | Remove Linux intermediate artefacts (`.o`, `libsracore.a`, `libsracore.so`).  The Linux binary `build/sra` is **not** removed. |
| `make win-clean` | Remove Windows intermediate artefacts (`.win.o`, `libsracore.a.win`).  `build/srawin.exe` is **not** removed. |
| `make clean-all` | Run both of the above. |
| `make clean-lib` | Remove only the `libsracore` artefacts (both platforms). |

After `make clean` you can rebuild with `make`; after
`make win-clean` — with `make win`.  Use `make clean-all` only
if you really want to delete the intermediate state for both
platforms.

---

## Troubleshooting

**`gcc: command not found`** — install the compiler:

```sh
sudo apt install build-essential
```

**`fatal error: alsa/asoundlib.h: No such file or directory`** —
install the ALSA development headers:

```sh
sudo apt install libasound2-dev
```

**`i686-w64-mingw32-gcc: command not found`** — install the
MinGW-w64 cross-toolchain:

```sh
sudo apt install mingw-w64
```

**`make win` fails with `cannot find -lsracore`** — this should
not happen with the current `Makefile`; it links
`libsracore.a.win` by full path.  If it does happen, make sure
you did not override `LIB` or `LIBS` from the command line.

**`srawin.exe` does not run under Wine** — Wine's MIDI support
is incomplete, especially for SysEx long messages.  Use a real
Windows host for functional testing.