/* sra_engine_linux.c — pthreads real-time engine + ALSA MIDI IN reader */

#include "sra_engine.h"
#include "sra_ui.h"
#include "midi_msg.h"

/* ------------------------------------------------------------------ */
/* libsracore callbacks                                                 */
/* ------------------------------------------------------------------ */

static void cb_timer(SraCore *sra, void *userdata) {
    struct timespec ts;
    (void)userdata;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    /* Wrap at 1000 s to stay within a long on 32-bit systems. */
    sracore_set_now_usec(sra,
        (long)((ts.tv_sec % 1000L) * 1000000L + ts.tv_nsec / 1000L));
}

static void cb_chord(SraCore *sra, void *userdata) {
    (void)userdata;
    ui_notify_chord(sracore_chord_name(sra), (int)sracore_tempo(sra));
}

static void cb_tempo(SraCore *sra, void *userdata) {
    (void)userdata;
    ui_notify_chord(sracore_chord_name(sra), (int)sracore_tempo(sra));
}

static void cb_error(SraCore *sra, int code, void *userdata) {
    (void)sra; (void)userdata;
    fprintf(stderr, "\nSRA error %d — aborting.\n", code);
    exit(code);
}

/* ------------------------------------------------------------------ */
/* ALSA MIDI IN reader thread                                           */
/* ------------------------------------------------------------------ */

/* Accumulates raw MIDI bytes into complete messages, then calls
   midi_in_process under the engine mutex.                             */
static void *midi_in_thread_func(void *arg) {
    SraEngine     *eng = (SraEngine *)arg;
    unsigned char  b;
    unsigned char  msg[3];
    int            got      = 0;  /* bytes collected (msg[0]=status) */
    int            expected = 0;  /* total bytes for this message     */
    int            in_sysex = 0;

    while (snd_rawmidi_read(eng->midi->h_in, &b, 1) == 1) {
        /* SysEx handling */
        if (b == 0xf0) { in_sysex = 1; got = 0; continue; }
        if (b == 0xf7) { in_sysex = 0; got = 0; continue; }
        if (in_sysex)  continue;

        if (b >= 0x80) { /* new status byte */
            unsigned char type = b & 0xf0;
            if (b >= 0xf8) continue; /* single-byte system real-time; skip */
            if (b >= 0xf0) { got = 0; continue; } /* other system; skip    */
            msg[0]   = b;
            got      = 1;
            expected = (type == 0xc0 || type == 0xd0) ? 2 : 3;
        } else { /* data byte */
            if (got == 0) continue; /* no status received yet */
            msg[got++] = b;
            if (got == expected) {
                SRABYTE d2 = (expected == 3) ? (SRABYTE)msg[2] : 0;
                pthread_mutex_lock(&eng->cs);
                midi_in_process(eng, (SRABYTE)msg[0], (SRABYTE)msg[1], d2);
                pthread_mutex_unlock(&eng->cs);
                got = 1; /* running status: keep msg[0], reset data count */
            }
        }
    }
    return NULL;
}

/* ------------------------------------------------------------------ */
/* Real-time engine thread                                              */
/* ------------------------------------------------------------------ */

static void *engine_thread_func(void *arg) {
    SraEngine          *eng = (SraEngine *)arg;
    struct sched_param  sp;
    long                last_usec;

    /* Request FIFO real-time scheduling (requires CAP_SYS_NICE or rtkit). */
    sp.sched_priority = sched_get_priority_max(SCHED_FIFO);
    pthread_setschedparam(pthread_self(), SCHED_FIFO, &sp);

    while (1) {
        last_usec = sracore_get_now_usec(eng->sra);

        pthread_mutex_lock(&eng->cs);
        while (last_usec == sracore_get_now_usec(eng->sra))
            sracore_step(eng->sra);
        pthread_mutex_unlock(&eng->cs);

        /* Yield 1 ms when the engine is ahead of real time. */
        if (sracore_is_ahead(eng->sra)) usleep(1000);

        pthread_mutex_lock(&eng->cs);
        midi_out_flush(eng);
        pthread_mutex_unlock(&eng->cs);
    }
    return NULL;
}

/* ------------------------------------------------------------------ */
/* Lifecycle                                                            */
/* ------------------------------------------------------------------ */

void engine_init(SraEngine *eng, MidiDevice *midi, void *platform_ctx) {
    (void)platform_ctx;
    eng->midi = midi;
    eng->sra  = sracore_create();
    if (!eng->sra) {
        fprintf(stderr, "SRA: out of memory\n");
        exit(1);
    }
    pthread_mutex_init(&eng->cs, NULL);
}

void engine_start(SraEngine *eng, int key_ch, int offset) {
    SraCallbacks cb;
    cb.on_chord  = cb_chord;
    cb.on_tempo  = cb_tempo;
    cb.on_error  = cb_error;
    cb.on_timer  = cb_timer;
    cb.userdata  = eng;

    sracore_set_callbacks(eng->sra, &cb);
    sracore_set_channel(eng->sra, key_ch, offset);
    sracore_init(eng->sra);

    pthread_create(&eng->engine_tid,   NULL, engine_thread_func, eng);
    pthread_create(&eng->midi_in_tid,  NULL, midi_in_thread_func, eng);
}

void engine_destroy(SraEngine *eng) {
    sracore_destroy(eng->sra);
    eng->sra = NULL;
    pthread_mutex_destroy(&eng->cs);
}

/* ------------------------------------------------------------------ */
/* MIDI output flush (drains libsracore queue → ALSA rawmidi OUT)      */
/* ------------------------------------------------------------------ */

void midi_out_flush(SraEngine *eng) {
    SRABYTE buf[3];
    int     n;
    while ((n = sracore_drain_output(eng->sra, buf)) > 0)
        snd_rawmidi_write(eng->midi->h_out, buf, (size_t)n);
}
