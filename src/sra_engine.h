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
    HANDLE           engine_thread;   /* for WaitForSingleObject */
    LARGE_INTEGER    freq;       /* QueryPerformanceFrequency result */
    HWND             hwnd;       /* for chord/tempo repaint callbacks */
#else
    pthread_mutex_t  cs;
    pthread_t        engine_tid;
    pthread_t        midi_in_tid;
#endif
    MidiDevice      *midi;       /* non-owning reference */
    SraCore         *sra;        /* libsracore engine instance       */

    /* Set to 0 to request a graceful shutdown of the worker threads.
       Written from a signal handler, read from the worker loops. */
    volatile sig_atomic_t running;
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
   offset:   semitone transposition.
   chord_ch: MIDI channel 0-15 reserved for chord input.
             (Internal -1 legacy mode is not exposed here.)
   silent:   non-zero in daemon mode — UI callbacks (chord / tempo)
             are not installed, so nothing is written to stdout. */
void engine_start(SraEngine *eng, int offset, int chord_ch, int silent);

/* Tear down the engine. */
void engine_destroy(SraEngine *eng);

/* Drain the libsracore output queue to the MIDI OUT device. */
void midi_out_flush(SraEngine *eng);

#ifdef _WIN32
DWORD WINAPI engine_thread(LPVOID param);
#endif

#endif /* SRA_ENGINE_H */
