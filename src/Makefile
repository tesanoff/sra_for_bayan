# SRA/src/Makefile — cross-platform build for srawin.exe (Windows) / sra (Linux)
#
# Usage:
#   make                  — build, static link (all platforms)
#   make LINK=shared      — build, dynamic link against libsracore (Linux only)
#   make BITS=32          — build 32-bit Windows executable (MinGW-w64)
#   make run              — build and launch (from ../build/)
#   make clean            — remove all build artefacts
#   make clean-lib        — remove only the library artefacts
#
# Linux dynamic linking (LINK=shared):
#   Produces libsracore.so; copied to ../build/ with RPATH=$ORIGIN embedded.
#   Run 'make clean-lib' before switching between static and shared.
#
# Windows 32-bit: requires i686-w64-mingw32-gcc in PATH.
#   make BITS=32
#   make BITS=32 LDFLAGS="-mwindows -static-libgcc"  (no DLL dependency)
#
# Requires on Linux:  gcc, libasound-dev (ALSA), and build-essential.
# Requires on Windows: MinGW-w64 in PATH.

# ── Compiler selection (Windows only) ───────────────────────────────
# BITS=32 → i686-w64-mingw32-gcc;  default → system gcc (64-bit)
ifeq ($(OS),Windows_NT)
    ifeq ($(BITS),32)
        CC = i686-w64-mingw32-gcc
    else
        CC = gcc
    endif
else
    CC = gcc
endif

CFLAGS  = -Wall -Wextra -O2 -std=gnu99 -I.

LIBDIR         = libsracore
LIB            = $(LIBDIR)/libsracore.a
LIBMAKE_TARGET = all
OUTDIR         = ../build

# ── Platform detection ───────────────────────────────────────────────
ifeq ($(OS),Windows_NT)
    PLATFORM    = win
    TARGET      = $(OUTDIR)/srawin.exe
    LDFLAGS     = -mwindows
    LIBS        = -L$(LIBDIR) -lsracore -lwinmm
    SRCS_PLAT   = midi_device_win.c \
                  sra_engine_win.c  \
                  sra_ui_win.c      \
                  srawin.c
    RUN_CMD     = cd $(OUTDIR) && ./srawin.exe
else
    PLATFORM    = linux
    TARGET      = $(OUTDIR)/sra
    LDFLAGS     =
    LIBS        = -L$(LIBDIR) -lsracore -lasound -lpthread
    SRCS_PLAT   = midi_device_linux.c \
                  sra_engine_linux.c  \
                  sra_ui_linux.c      \
                  sra_config.c        \
                  sramain.c
    RUN_CMD     = cd $(OUTDIR) && ./sra
endif

# ── Dynamic linking (Linux only) ────────────────────────────────────
ifneq ($(OS),Windows_NT)
LINK ?= static
ifeq ($(LINK),shared)
    LIB            = $(LIBDIR)/libsracore.so
    LIBMAKE_TARGET = shared
    LDFLAGS       += -Wl,-rpath,\$$ORIGIN
    SHARED_COPY    = $(OUTDIR)/libsracore.so
endif
endif

# ── Sources ──────────────────────────────────────────────────────────
SRCS_COMMON = midi_msg.c
SRCS        = $(SRCS_COMMON) $(SRCS_PLAT)
OBJS        = $(SRCS:.c=.o)

# ────────────────────────────────────────────────────────────────────
.PHONY: all libsracore run clean clean-lib clean-all win win-clean

all: libsracore $(TARGET)

# ── Sub-make for libsracore ──────────────────────────────────────────
libsracore:
	$(MAKE) -C $(LIBDIR) $(LIBMAKE_TARGET) CC="$(CC)"

# ── Link ─────────────────────────────────────────────────────────────
$(TARGET): $(OBJS) $(LIB) | $(OUTDIR)
	@echo "  LD  $@"
	$(CC) $(LDFLAGS) $(OBJS) $(LIBS) -o $@
ifdef SHARED_COPY
	@echo "  CP  $(SHARED_COPY)"
	cp $(LIB) $(SHARED_COPY)
endif

$(OUTDIR):
	mkdir -p $@

# ── Compile rules ────────────────────────────────────────────────────
%.o: %.c
	@echo "  CC  $<"
	$(CC) $(CFLAGS) -c $< -o $@

# ── Per-file header dependencies (common) ────────────────────────────
midi_msg.o: midi_msg.c midi_msg.h sra_engine.h \
            $(LIBDIR)/sracore.h

# ── Per-file header dependencies (Windows) ───────────────────────────
ifeq ($(PLATFORM),win)
midi_device_win.o: midi_device_win.c midi_device.h sra_types.h

sra_engine_win.o:  sra_engine_win.c sra_engine.h midi_device.h \
                   midi_msg.h $(LIBDIR)/sracore.h

sra_ui_win.o:      sra_ui_win.c sra_ui.h midi_device.h sra_types.h \
                   $(LIBDIR)/sracore.h

srawin.o:          srawin.c sra_types.h midi_device.h midi_msg.h \
                   sra_ui.h sra_engine.h
endif

# ── Per-file header dependencies (Linux) ─────────────────────────────
ifeq ($(PLATFORM),linux)
midi_device_linux.o: midi_device_linux.c midi_device.h sra_types.h

sra_engine_linux.o:  sra_engine_linux.c sra_engine.h midi_device.h \
                     midi_msg.h sra_ui.h $(LIBDIR)/sracore.h

sra_ui_linux.o:      sra_ui_linux.c sra_ui.h midi_device.h sra_types.h \
                     $(LIBDIR)/sracore.h

sramain.o:           sramain.c sra_types.h midi_device.h \
                     sra_ui.h sra_engine.h
endif

# ── Run ──────────────────────────────────────────────────────────────
# Launches from build/ so the executable finds style0.mid and sra_init.hex.
run: all
	$(RUN_CMD)

# ── Clean ────────────────────────────────────────────────────────────
clean: clean-lib
	rm -f $(OBJS) $(TARGET)
ifdef SHARED_COPY
	rm -f $(SHARED_COPY)
endif

clean-lib:
	$(MAKE) -C $(LIBDIR) clean

# ── Windows cross-compilation (from Linux via MinGW) ─────────────────
# Usage:  make win            (32-bit, requires i686-w64-mingw32-gcc)
#         make win BITS=64    (64-bit, requires x86_64-w64-mingw32-gcc)
#
# Output: ../build/srawin.exe
# Does not interfere with the normal Linux build.

ifeq ($(BITS),64)
    WIN_CC = x86_64-w64-mingw32-gcc
else
    WIN_CC = i686-w64-mingw32-gcc
endif

WIN_TARGET  = $(OUTDIR)/srawin.exe
WIN_SRCS    = midi_msg.c \
              midi_device_win.c \
              sra_engine_win.c \
              sra_ui_win.c \
              sra_config.c \
              srawin.c
WIN_OBJS    = $(WIN_SRCS:.c=.win.o)

.PHONY: win win-clean

win: $(WIN_TARGET)

$(WIN_TARGET): $(WIN_OBJS) $(LIBDIR)/libsracore.a.win | $(OUTDIR)
	@echo "  LD(win)  $@"
	$(WIN_CC) -mwindows $(WIN_OBJS) $(LIBDIR)/libsracore.a.win -lwinmm -o $@

%.win.o: %.c
	@echo "  CC(win)  $<"
	$(WIN_CC) -Wall -Wextra -O2 -std=gnu99 -I. -c $< -o $@

# Build a Windows-targeted libsracore.a under a separate name so it
# does not clash with the Linux one.
$(LIBDIR)/libsracore.a.win: $(LIBDIR)/sracore.c \
                            $(LIBDIR)/sracore_chord.c \
                            $(LIBDIR)/sracore_style.c \
                            $(LIBDIR)/sracore_engine.c \
                            $(LIBDIR)/sracore_sysex.c
	@echo "  AR(win)  $@"
	@rm -f $(LIBDIR)/*.win.o
	$(WIN_CC) -Wall -Wextra -O2 -std=gnu99 -I$(LIBDIR) \
	    -c $(LIBDIR)/sracore.c       -o $(LIBDIR)/sracore.win.o
	$(WIN_CC) -Wall -Wextra -O2 -std=gnu99 -I$(LIBDIR) \
	    -c $(LIBDIR)/sracore_chord.c -o $(LIBDIR)/sracore_chord.win.o
	$(WIN_CC) -Wall -Wextra -O2 -std=gnu99 -I$(LIBDIR) \
	    -c $(LIBDIR)/sracore_style.c -o $(LIBDIR)/sracore_style.win.o
	$(WIN_CC) -Wall -Wextra -O2 -std=gnu99 -I$(LIBDIR) \
	    -c $(LIBDIR)/sracore_engine.c -o $(LIBDIR)/sracore_engine.win.o
	$(WIN_CC) -Wall -Wextra -O2 -std=gnu99 -I$(LIBDIR) \
	    -c $(LIBDIR)/sracore_sysex.c -o $(LIBDIR)/sracore_sysex.win.o
	ar rcs $@ $(LIBDIR)/sracore.win.o $(LIBDIR)/sracore_chord.win.o \
	         $(LIBDIR)/sracore_style.win.o $(LIBDIR)/sracore_engine.win.o \
	         $(LIBDIR)/sracore_sysex.win.o

win-clean:
	rm -f $(WIN_OBJS) $(LIBDIR)/*.win.o \
	      $(LIBDIR)/libsracore.a.win

# Remove both Linux and Windows build artefacts.
clean-all: clean win-clean
