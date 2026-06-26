/* sracore.c — lifecycle, queue, MIDI I/O, public API wrappers */

#include "sracore_private.h"

/* ------------------------------------------------------------------ */
/* Lifecycle                                                            */
/* ------------------------------------------------------------------ */

SraCore *sracore_create(void) {
    SraCore *sra = (SraCore *)calloc(1, sizeof(SraCore));
    if (!sra) return NULL;
    /* match original default initialisers */
    sra->key_off_count = -1;
    sra->bass_lock     = -1;
    sra->fadeout_f     = -1;
    sra->mbass_vf      = 1;
    sra->acc_vf        = 1;
    sra->acc_bass_vf   = 1;
    sra->drum_vf       = 1;
    strcpy(sra->style_name, "style");
    return sra;
}

void sracore_destroy(SraCore *sra) {
    if (!sra) return;
    free(sra->style_buf);
    free(sra);
}

void sracore_set_channel(SraCore *sra, int channel, int offset) {
    sra->key_ch = channel;
    sra->offset = offset;
}

void sracore_set_callbacks(SraCore *sra, const SraCallbacks *cb) {
    sra->cb = *cb;
}

void sracore_init(SraCore *sra) {
    sra->style_buf = (SRABYTE *)malloc(STYLESIZE);
    if (!sra->style_buf) sra_do_error(sra, 1);
    if (!sra_load_style(sra, 0x00)) sra_do_error(sra, 1);

    sra->chordd  = 0;
    sra->bass    = 0;
    sra->chord_k = 0;
    /* chord_v set to major voicing (all zeros) by sra_set_chord, but
       we need a valid pointer before any chord is detected */
    {
        static const signed char zero_voicing[12] = {0};
        sra->chord_v = zero_voicing;
    }
    sra_reset(sra, 0);
    /* seed the clock — on_timer must set now_usec before returning */
    if (sra->cb.on_timer) sra->cb.on_timer(sra, sra->cb.userdata);
    sra->clock = sra->now_usec;
}

/* ------------------------------------------------------------------ */
/* Internal error helper                                                */
/* ------------------------------------------------------------------ */

void sra_do_error(SraCore *sra, int code) {
    if (sra->cb.on_error)
        sra->cb.on_error(sra, code, sra->cb.userdata);
    /* on_error should not return; if it does, abort */
    exit(code);
}

/* ------------------------------------------------------------------ */
/* Output queue                                                         */
/* ------------------------------------------------------------------ */

void sra_append(SraCore *sra, SRABYTE b) {
    sra->queue[sra->que_t] = b;
    sra->que_t = (sra->que_t + 1) % MAXQUEUE;
}

/* ------------------------------------------------------------------ */
/* MIDI input                                                           */
/* ------------------------------------------------------------------ */

/* Decode one incoming keyboard MIDI message, route it through the
   arranger.  raw_status channel bits are replaced with key_ch. */
void sracore_midi_in(SraCore *sra,
                     SRABYTE raw_status, SRABYTE data1, SRABYTE data2) {
    SRABYTE msg2  = raw_status & (SRABYTE)0xf0;
    SRABYTE msg   = msg2 | (SRABYTE)sra->key_ch;
    SRABYTE msg3  = 0xFF; /* 0xFF = "no key event this message" */

    switch (msg2) {
    case 0x80:
        /* Note-off: convert to note-on vel=0 on key_ch */
        msg3 = 0x00;
        msg  = 0x90 | (SRABYTE)sra->key_ch;
        sra_append(sra, msg);
        sra_append(sra, data1 + sra->offset3 + sra->offset4);
        sra_append(sra, msg3);
        break;
    case 0x90:
        msg3 = data2;
        sra_append(sra, msg);
        sra_append(sra, data1 + sra->offset3 + sra->offset4);
        sra_append(sra, msg3);
        break;
    case 0xb0:
    case 0xe0:
        sra_append(sra, msg);
        sra_append(sra, data1);
        sra_append(sra, data2);
        break;
    case 0xc0:
        sra_append(sra, msg);
        sra_append(sra, data1);
        break;
    default:
        break;
    }

    /* After 0x80 conversion, msg became 0x90|key_ch, so both
       note-on and note-off enter the chord detection path. */
    sra->msg = msg;
    if (msg == (SRABYTE)(0x90 | sra->key_ch) && msg3 != 0xFF) {
        /* key_v is the velocity the chord functions will store */
        sra->key_v = msg3;
        if (msg3) sra_check_key_on(sra);
        else      sra_check_key_off(sra);
    }
}

/* ------------------------------------------------------------------ */
/* MIDI output                                                          */
/* ------------------------------------------------------------------ */

/* Returns number of bytes written (0 = empty, 1–3 = one message). */
int sracore_drain_output(SraCore *sra, SRABYTE buf[3]) {
    SRABYTE status, msg2, d1, d2;
    int     n;

    if (sra->que_h == sra->que_t) return 0;

    status = sra->queue[sra->que_h];
    msg2   = status & (SRABYTE)0xf0;
    sra->que_h = (sra->que_h + 1) % MAXQUEUE;
    d1 = sra->queue[sra->que_h % MAXQUEUE];
    d2 = sra->queue[(sra->que_h + 1) % MAXQUEUE];

    switch (msg2) {
    case 0x90: case 0xa0: case 0xb0: case 0xe0:
        buf[0] = status; buf[1] = d1; buf[2] = d2;
        sra->que_h = (sra->que_h + 2) % MAXQUEUE;
        n = 3;
        break;
    case 0xc0: case 0xd0:
        buf[0] = status; buf[1] = d1;
        sra->que_h = (sra->que_h + 1) % MAXQUEUE;
        n = 2;
        break;
    default: /* 0xf0 and single-byte system messages */
        buf[0] = status;
        n = 1;
        break;
    }
    return n;
}

/* ------------------------------------------------------------------ */
/* Clock                                                                */
/* ------------------------------------------------------------------ */

void sracore_set_now_usec(SraCore *sra, long usec) { sra->now_usec = usec; }
long sracore_get_now_usec(const SraCore *sra)       { return sra->now_usec; }

int sracore_is_ahead(const SraCore *sra) {
    return sra->wait2 > 1000 && sra->wait2 < sra->wait;
}

/* ------------------------------------------------------------------ */
/* Public step (delegates to internal)                                  */
/* ------------------------------------------------------------------ */

void sracore_step(SraCore *sra) { sra_step(sra); }

/* ------------------------------------------------------------------ */
/* Display queries                                                      */
/* ------------------------------------------------------------------ */

const char *sracore_chord_name(const SraCore *sra) { return sra->chord_name; }
SRABYTE     sracore_tempo(const SraCore *sra)       { return sra->tempo; }
