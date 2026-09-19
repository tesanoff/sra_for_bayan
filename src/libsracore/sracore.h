/* libsracore — Software-based Real-time Arranger core engine by ZZ-Denis @ NazoMusic
   Public API header.  Compile: see libsracore/README or Makefile.
   All internal state is hidden behind an opaque pointer. */

#ifndef SRACORE_H
#define SRACORE_H

#include <stddef.h>

#ifndef SRACORE_BYTE_DEFINED
typedef unsigned char SRABYTE;
#define SRACORE_BYTE_DEFINED
#endif

/* ------------------------------------------------------------------ */
/* Opaque engine object                                                 */
/* ------------------------------------------------------------------ */
typedef struct SraCore SraCore;

/* ------------------------------------------------------------------ */
/* Callback signatures                                                  */
/* ------------------------------------------------------------------ */

/* Called whenever the current chord name changes (display update) */
typedef void (*SraChordCb)(SraCore *sra, void *userdata);

/* Called whenever tempo changes (display update) */
typedef void (*SraTempoCb)(SraCore *sra, void *userdata);

/* Called on fatal error; implementation should show a message and exit */
typedef void (*SraErrorCb)(SraCore *sra, int code, void *userdata);

/* Called from inside sracore_step() to refresh now_usec.
   Implementation must call sracore_set_now_usec() before returning. */
typedef void (*SraTimerCb)(SraCore *sra, void *userdata);

typedef struct {
    SraChordCb on_chord;  /* may be NULL */
    SraTempoCb on_tempo;  /* may be NULL */
    SraErrorCb on_error;  /* must not be NULL */
    SraTimerCb on_timer;  /* must not be NULL */
    void      *userdata;
} SraCallbacks;

/* ------------------------------------------------------------------ */
/* Lifecycle                                                            */
/* ------------------------------------------------------------------ */

/* Allocate and zero-initialise a new engine instance.
   Returns NULL on malloc failure. */
SraCore *sracore_create(void);

/* Free all resources including the style buffer. */
void     sracore_destroy(SraCore *sra);

/* Set MIDI channel (0-15) and semitone transposition offset.
   Must be called before sracore_init(). */
void     sracore_set_channel(SraCore *sra, int channel, int offset);

/* Set the MIDI channel (0-15) reserved for chord input.
   Notes arriving on this channel are always analysed as chords,
   regardless of pitch.  Notes on other channels are always treated
   as melody and forwarded to the main key channel.
   Pass -1 to restore the legacy "any channel, by pitch" behaviour. */
void     sracore_set_chord_channel(SraCore *sra, int chord_ch);

/* Install all four callbacks at once. */
void     sracore_set_callbacks(SraCore *sra, const SraCallbacks *cb);

/* Load the default style (style0.mid) and reset all engine state.
   Calls on_error and does not return on failure.
   sracore_set_channel() and sracore_set_callbacks() must be called first. */
void     sracore_init(SraCore *sra);

/* ------------------------------------------------------------------ */
/* Real-time step  (call from a high-priority thread)                  */
/* ------------------------------------------------------------------ */

/* Advance the arranger by one time slice.
   Internally calls on_timer to refresh now_usec, then processes
   timing, session transitions, and chord voicing. */
void sracore_step(SraCore *sra);

/* ------------------------------------------------------------------ */
/* MIDI input  (call under the same critical section as sracore_step)  */
/* ------------------------------------------------------------------ */

/* Feed one decoded MIDI message from the keyboard into the engine.
   raw_status: status byte as received (channel bits will be replaced).
   data1, data2: data bytes (data2 ignored for single-data messages). */
void sracore_midi_in(SraCore *sra,
                     SRABYTE raw_status, SRABYTE data1, SRABYTE data2);

/* Feed one complete SysEx message (F0 and F7 framing bytes already
   stripped by the platform layer).  data[0] is expected to be the
   Manufacturer ID (0x7D).  len is the number of bytes in data[].

   Unknown CMDs are silently ignored.  Malformed messages that carry
   our Manufacturer ID are reported to stderr but never abort the
   engine.  Safe to call from the same critical section as
   sracore_midi_in(). */
void sracore_sysex_in(SraCore *sra, const SRABYTE *data, int len);

/* ------------------------------------------------------------------ */
/* MIDI output  (call under the same critical section as sracore_step) */
/* ------------------------------------------------------------------ */

/* Read the next MIDI message from the output queue.
   Writes up to 3 bytes into buf[0..2].
   Returns the number of bytes written (1-3), or 0 if queue is empty. */
int sracore_drain_output(SraCore *sra, SRABYTE buf[3]);

/* ------------------------------------------------------------------ */
/* Clock  (call from the on_timer callback)                             */
/* ------------------------------------------------------------------ */

/* Replace the engine's notion of "current time" (in microseconds).
   Typically called by on_timer via QueryPerformanceCounter. */
void sracore_set_now_usec(SraCore *sra, long usec);

/* Return the last value set by sracore_set_now_usec(). */
long sracore_get_now_usec(const SraCore *sra);

/* True when the engine is generating MIDI faster than real time
   (Wait2 > 1000 && Wait2 < Wait).  Caller may sleep(1) when true. */
int  sracore_is_ahead(const SraCore *sra);

/* ------------------------------------------------------------------ */
/* Display queries  (safe to call from any thread)                      */
/* ------------------------------------------------------------------ */

/* Current chord name string, e.g. "Cm7", "F#maj9/C#".
   Points into engine-owned storage; copy if you need to keep it. */
const char *sracore_chord_name(const SraCore *sra);

/* Current tempo value as stored in the style file (raw, not BPM).
   Displayed BPM = tempo. */
SRABYTE sracore_tempo(const SraCore *sra);

#endif /* SRACORE_H */
