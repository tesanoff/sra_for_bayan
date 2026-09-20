/* sra_engine_win.c — Windows real-time engine thread; bridges libsracore */

#include "sra_engine.h"
#include "midi_msg.h"

/* ------------------------------------------------------------------ */
/* libsracore callbacks                                                 */
/* ------------------------------------------------------------------ */

static void cb_timer(SraCore *sra, void *userdata) {
    SraEngine     *eng = (SraEngine *)userdata;
    LARGE_INTEGER  tick;
    QueryPerformanceCounter(&tick);
    sracore_set_now_usec(sra,
        (long)((tick.QuadPart * 1000000LL / eng->freq.QuadPart) % 1000000000LL));
}

static void cb_chord(SraCore *sra, void *userdata) {
    SraEngine *eng = (SraEngine *)userdata;
    (void)sra;
    InvalidateRect(eng->hwnd, NULL, TRUE);
    UpdateWindow(eng->hwnd);
}

static void cb_tempo(SraCore *sra, void *userdata) {
    SraEngine *eng = (SraEngine *)userdata;
    (void)sra;
    InvalidateRect(eng->hwnd, NULL, TRUE);
    UpdateWindow(eng->hwnd);
}

static void cb_error(SraCore *sra, int code, void *userdata) {
    char msg[32];
    (void)sra; (void)userdata;
    sprintf(msg, "ERROR(%d)", code);
    MessageBox(0, msg, "SRA Error", MB_OK | MB_ICONERROR);
    exit(code);
}

/* ------------------------------------------------------------------ */
/* Lifecycle                                                            */
/* ------------------------------------------------------------------ */

void engine_init(SraEngine *eng, MidiDevice *midi, void *platform_ctx) {
    eng->midi = midi;
    eng->hwnd = (HWND)platform_ctx;
    eng->sra  = sracore_create();
    if (!eng->sra) {
        MessageBox(0, "Out of memory", "SRA", MB_OK | MB_ICONERROR);
        exit(1);
    }
    InitializeCriticalSection(&eng->cs);
}

void engine_start(SraEngine *eng, int offset, int chord_ch, int silent) {
    SraCallbacks cb;
    (void)silent;   /* daemon mode is not supported on Windows */
    QueryPerformanceFrequency(&eng->freq);

    cb.on_chord  = cb_chord;
    cb.on_tempo  = cb_tempo;
    cb.on_error  = cb_error;
    cb.on_timer  = cb_timer;
    cb.userdata  = eng;

    sracore_set_callbacks(eng->sra, &cb);
    sracore_set_offset(eng->sra, offset);
    sracore_set_chord_channel(eng->sra, chord_ch);
    sracore_init(eng->sra);

    SetPriorityClass(GetCurrentProcess(), REALTIME_PRIORITY_CLASS);
    if (!CreateThread(NULL, 0, engine_thread, eng, 0, &eng->engine_tid))
        exit(0);
}

void engine_destroy(SraEngine *eng) {
    sracore_destroy(eng->sra);
    eng->sra = NULL;
    DeleteCriticalSection(&eng->cs);
}

/* ------------------------------------------------------------------ */
/* Real-time thread                                                     */
/* ------------------------------------------------------------------ */

DWORD WINAPI engine_thread(LPVOID param) {
    SraEngine *eng = (SraEngine *)param;
    long last_usec;

    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_TIME_CRITICAL);

    while (1) {
        last_usec = sracore_get_now_usec(eng->sra);

        EnterCriticalSection(&eng->cs);
        while (last_usec == sracore_get_now_usec(eng->sra))
            sracore_step(eng->sra);
        LeaveCriticalSection(&eng->cs);

        /* Yield 1 ms when the engine is generating events ahead of time. */
        if (sracore_is_ahead(eng->sra)) Sleep(1);

        EnterCriticalSection(&eng->cs);
        midi_out_flush(eng);
        LeaveCriticalSection(&eng->cs);
    }
    return 0;
}

/* ------------------------------------------------------------------ */
/* MIDI output flush (drains libsracore queue → winmm MIDI OUT)        */
/* ------------------------------------------------------------------ */

void midi_out_flush(SraEngine *eng) {
    SRABYTE buf[3];
    int     n, i;
    while ((n = sracore_drain_output(eng->sra, buf)) > 0) {
        DWORD msg = 0;
        for (i = 0; i < n; i++) msg |= (DWORD)buf[i] << (i * 8);
        midiOutShortMsg(eng->midi->h_out, msg);
    }
}
