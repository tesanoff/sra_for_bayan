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
   midi_in_process under the engine mutex.  Complete SysEx messages
   are forwarded to sracore_sysex_in (also under the mutex).           */
#define SRA_SYSEX_MAX 256

static void *midi_in_thread_func(void *arg) {
    SraEngine     *eng = (SraEngine *)arg;
    unsigned char  b;
    unsigned char  msg[3];
    int            got      = 0;  /* bytes collected (msg[0]=status) */
    int            expected = 0;  /* total bytes for this message     */

    /* SysEx accumulation state */
    unsigned char  sysex[SRA_SYSEX_MAX];
    int            sysex_len  = 0;
    int            in_sysex   = 0;
    int            sysex_over = 0;  /* 1 = overflowed, discard until F7 */

    /* Non-blocking mode: we poll with a timeout so that the thread can
       notice a shutdown request even when no MIDI data is arriving. */
    snd_rawmidi_nonblock(eng->midi->h_in, 1);

    while (eng->running) {
        struct pollfd pfd;
        int rc;

        pfd.fd     = -1;
        pfd.events = POLLIN;
        rc = snd_rawmidi_poll_descriptors(eng->midi->h_in, &pfd, 1);
        if (rc != 1) { usleep(1000); continue; }

        rc = poll(&pfd, 1, 100);
        if (rc <= 0) continue;               /* timeout or EINTR */

        /* Re-check the running flag before touching h_in: the main
           thread may have started a graceful shutdown while we were
           blocked in poll().  We must not call snd_rawmidi_read()
           after h_in has been closed. */
        if (!eng->running) break;

        if (snd_rawmidi_read(eng->midi->h_in, &b, 1) != 1)
            continue;
        /* SysEx handling.  Real-time bytes (0xF8..0xFF) are ignored,
           even inside a SysEx message (kept simple, matches old behaviour). */
        if (b == 0xf0) {
            in_sysex   = 1;
            sysex_len  = 0;
            sysex_over = 0;
            got        = 0;
            continue;
        }
        if (b == 0xf7) {
            if (in_sysex && !sysex_over && sysex_len > 0) {
                pthread_mutex_lock(&eng->cs);
                sracore_sysex_in(eng->sra, sysex, sysex_len);
                pthread_mutex_unlock(&eng->cs);
            }
            in_sysex   = 0;
            sysex_len  = 0;
            sysex_over = 0;
            got        = 0;
            continue;
        }
        if (in_sysex) {
            if (b >= 0xf8) continue;          /* ignore real-time inside SysEx */
            if (sysex_over) continue;         /* still discarding until F7 */
            if (sysex_len >= SRA_SYSEX_MAX) {
                fprintf(stderr,
                        "SRA SysEx error: message too long (> %d bytes)\n",
                        SRA_SYSEX_MAX);
                sysex_over = 1;
                continue;
            }
            sysex[sysex_len++] = b;
            continue;
        }

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

    while (eng->running) {
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
    eng->running = 1;
    pthread_mutex_init(&eng->cs, NULL);
}

void engine_start(SraEngine *eng, int offset, int chord_ch) {
    SraCallbacks cb;
    cb.on_chord  = cb_chord;
    cb.on_tempo  = cb_tempo;
    cb.on_error  = cb_error;
    cb.on_timer  = cb_timer;
    cb.userdata  = eng;

    sracore_set_callbacks(eng->sra, &cb);
    sracore_set_offset(eng->sra, offset);
    sracore_set_chord_channel(eng->sra, chord_ch);
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
