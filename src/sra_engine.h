#ifndef SRA_ENGINE_H
#define SRA_ENGINE_H

#include "sra_types.h"
#include "midi_device.h"
#include "libsracore/sracore.h"

/* ------------------------------------------------------------------ */
/* Engine context struct                                                */
/* ------------------------------------------------------------------ */

typedef struct {
#ifdef _WIN32
    CRITICAL_SECTION cs;
    DWORD            engine_tid;
    LARGE_INTEGER    freq;       /* QueryPerformanceFrequency result */
    HWND             hwnd;       /* for chord/tempo repaint callbacks */
#else
    pthread_mutex_t  cs;
    pthread_t        engine_tid;
    pthread_t        midi_in_tid;
#endif
    MidiDevice      *midi;       /* non-owning reference */
    SraCore         *sra;        /* libsracore engine instance       */
} SraEngine;

/* ------------------------------------------------------------------ */
/* Lifecycle                                                            */
/* ------------------------------------------------------------------ */

/* Allocate the SraCore instance and initialise the mutex / CS.
   platform_ctx:  HWND on Windows (forwarded to callbacks for repaint),
                  NULL on Linux. */
void engine_init(SraEngine *eng, MidiDevice *midi, void *platform_ctx);

/* Install callbacks, call sracore_init(), launch the real-time thread.
   On Linux also launches the ALSA MIDI-IN reader thread.
   key_ch: MIDI channel 0-15.  offset: semitone transposition. */
void engine_start(SraEngine *eng, int key_ch, int offset);

/* Tear down the engine. */
void engine_destroy(SraEngine *eng);

/* Drain the libsracore output queue to the MIDI OUT device. */
void midi_out_flush(SraEngine *eng);

#ifdef _WIN32
DWORD WINAPI engine_thread(LPVOID param);
#endif

#endif /* SRA_ENGINE_H */
