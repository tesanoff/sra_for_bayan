# libsracore/Makefile
# Builds libsracore.a (static) and optionally libsracore.so (shared, Linux only).
#
# Usage:
#   make              — build libsracore.a (default)
#   make static       — build libsracore.a
#   make shared       — build libsracore.so  (Linux only)
#   make both         — build both            (Linux only)
#   make clean        — remove all build artefacts
#
# CC is inherited from the parent make
# (supports BITS=32 → i686-w64-mingw32-gcc without changes here).

CC     ?= gcc
CFLAGS  = -Wall -Wextra -O2 -std=gnu99
AR      = ar
ARFLAGS = rcs

# ── Sources ─────────────────────────────────────────────────────────
SRCS = sracore.c \
       sracore_chord.c \
       sracore_style.c \
       sracore_engine.c \
       sracore_sysex.c

HDRS = sracore.h sracore_private.h

OBJS_STATIC = $(SRCS:.c=.o)
OBJS_SHARED = $(SRCS:.c=.pic.o)

# ────────────────────────────────────────────────────────────────────
.PHONY: all static shared both clean

all: static

static: libsracore.a

shared: libsracore.so

both: libsracore.a libsracore.so

# ── Static archive ───────────────────────────────────────────────────
libsracore.a: $(OBJS_STATIC)
	$(AR) $(ARFLAGS) $@ $^
	@echo "  AR  $@"

# ── Shared library (Linux) ───────────────────────────────────────────
libsracore.so: $(OBJS_SHARED)
	@echo "  LD  $@"
	$(CC) -shared $(OBJS_SHARED) -o $@

# ── Pattern rules ────────────────────────────────────────────────────
$(OBJS_STATIC): %.o: %.c $(HDRS)
	@echo "  CC      $<"
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJS_SHARED): %.pic.o: %.c $(HDRS)
	@echo "  CC(pic) $<"
	$(CC) $(CFLAGS) -fPIC -c $< -o $@

# ── Clean ────────────────────────────────────────────────────────────
clean:
	rm -f $(OBJS_STATIC) $(OBJS_SHARED) libsracore.a libsracore.so
