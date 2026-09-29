/* sracore_style.c — style file loading, voice management, session, program */

#include "sracore_private.h"

/* ------------------------------------------------------------------ */
/* Timing                                                               */
/* ------------------------------------------------------------------ */

void sra_set_clock(SraCore *sra) {
    sra->wait = 500000L / (long)sra->tempo;
    if (sra->wait2 >= sra->wait) sra->wait2 = sra->wait - 1;
    if (sra->cb.on_tempo) sra->cb.on_tempo(sra, sra->cb.userdata);
}

/* ------------------------------------------------------------------ */
/* Voice management                                                     */
/* ------------------------------------------------------------------ */

void sra_inc_voice(SraCore *sra, SRABYTE cmd, SRABYTE note, SRABYTE vel) {
    int i;
    for (i = 0; i < MAXVOICE; i++) {
        if (sra->voice[i][0] == 0x00) {
            sra->voice[i][0] = cmd;
            sra->voice[i][1] = note;
            sra->voice[i][2] = vel;
            sra->voice_count++;
            return;
        }
    }
}

void sra_dec_voice(SraCore *sra, SRABYTE cmd, SRABYTE note) {
    int i;
    for (i = 0; i < MAXVOICE; i++) {
        if (sra->voice[i][0] == cmd && sra->voice[i][1] == note) {
            sra->voice[i][0] = 0x00;
            sra->voice_count--;
            return;
        }
    }
}

void sra_all_note_off(SraCore *sra) {
    int i;
    for (i = 0; sra->voice_count; i++) {
        if (sra->voice[i][0] != 0x00) {
            sra_append(sra, sra->voice[i][0]);
            sra_append(sra, sra->voice[i][1]);
            sra_append(sra, 0x00);
            sra->voice[i][0] = 0x00;
            sra->voice_count--;
        }
    }
    sra->bass_lock = -1;
}

void sra_lower_on(SraCore *sra) {
    sra_append(sra, 0xb0 | LOWER);
    sra_append(sra, 0x0b);
    sra_append(sra, sra->fadeout_f == -1 ? 127 :
                     (sra->fadeout_f >= 0 ? (SRABYTE)sra->fadeout_f : 127));
    sra->voice_lock = 0;
}

void sra_lower_off(SraCore *sra) {
    sra_append(sra, 0xb0 | LOWER);
    sra_append(sra, 0x0b);
    sra_append(sra, 0x00);
    sra->voice_lock = 1;
}

/* ------------------------------------------------------------------ */
/* Program change handling                                              */
/* ------------------------------------------------------------------ */

void sra_prog_change(SraCore *sra, SRABYTE ch) {
    if (sra->prog_t[ch][0] == sra->prog[ch][0] &&
        sra->prog_t[ch][1] == sra->prog[ch][1]) {
        if (sra->prog_t[ch][2] != sra->prog[ch][2]) {
            sra_append(sra, (SRABYTE)(ch | 0xc0));
            sra_append(sra, sra->prog[ch][2] = sra->prog_t[ch][2]);
        }
    } else {
        sra_append(sra, (SRABYTE)(ch | 0xb0)); sra_append(sra, 0);
        sra_append(sra, sra->prog[ch][0] = sra->prog_t[ch][0]);
        sra_append(sra, (SRABYTE)(ch | 0xb0)); sra_append(sra, 32);
        sra_append(sra, sra->prog[ch][1] = sra->prog_t[ch][1]);
        sra_append(sra, (SRABYTE)(ch | 0xc0));
        sra_append(sra, sra->prog[ch][2] = sra->prog_t[ch][2]);
    }
}

/* ------------------------------------------------------------------ */
/* Reset: send initialisation messages to arranger-owned channels only. */
/* (ACCBASS, ACC1..ACC5, PHRASE, MBASS, DRUM, LOWER).  Channels used    */
/* for live playing are left untouched.                                 */
/* ------------------------------------------------------------------ */

void sra_reset(SraCore *sra, int full) {
    int i;
    SRABYTE m;
    FILE *f;
    static const SRABYTE INIT_CH[] = {
        ACC1, ACC2, ACC3, ACC4, ACC5, PHRASE, ACCBASS
    };

    for (i = 0; i < 16; i++) sra->prog[i][0] = 0xff;

    /* All Sound Off on every arranger-owned channel: release any
       notes the user might have been playing on them while the
       arranger was stopped. */
    {
        static const SRABYTE ALL_CH[] = {
            LOWER, MBASS, ACC1, ACC2, ACC3, ACC4, ACC5, PHRASE,
            ACCBASS, DRUM
        };
        int k;
        for (k = 0; k < 10; k++) {
            sra_append(sra, (SRABYTE)(0xb0 | ALL_CH[k]));
            sra_append(sra, 0x78);   /* All Sound Off */
            sra_append(sra, 0x00);
        }
    }

    /* Pitch-wheel centre on accent channels */
    for (i = 0; i < 7; i++) {
        sra_append(sra, 0xe0 | INIT_CH[i]);
        sra_append(sra, 0x00);
        sra_append(sra, 0x40);
    }

    if (full) return; /* abbreviated reset: pitch-wheel only */

    /* Volume (CC11) and reverb (CC91) for all parts */
    {
        static const struct { SRABYTE ch; SRABYTE vol; SRABYTE rev; } PARTS[] = {
            {LOWER,   127, 80 }, {MBASS,   127, 30 },
            {ACC1,    127, 60 }, {ACC2,    127, 60 },
            {ACC3,    127, 60 }, {ACC4,    127, 60 },
            {ACC5,    127, 60 }, {PHRASE,  127, 60 },
            {ACCBASS, 127, 40 }, {DRUM,    127, 70 },
        };
        for (i = 0; i < 10; i++) {
            sra_append(sra, 0xb0 | PARTS[i].ch);
            sra_append(sra, 0x0b);
            sra_append(sra, PARTS[i].vol);
            sra_append(sra, 0xb0 | PARTS[i].ch);
            sra_append(sra, 91);
            sra_append(sra, PARTS[i].rev);
            sra_append(sra, 0xb0 | PARTS[i].ch);
            sra_append(sra, 0x07);
            sra_append(sra, sra->master_vol);
        }
        /* Default patches: LOWER=49 (strings), MBASS=35 (fretless bass) */
        sra_append(sra, 0xc0 | LOWER); sra_append(sra, 49);
        sra_append(sra, 0xc0 | MBASS); sra_append(sra, 35);
    }

    /* Load optional init sysex from sra_init.hex */
    if ((f = fopen("sra_init.hex", "rb")) != NULL) {
        while (1) {
            m = (SRABYTE)fgetc(f);
            if (feof(f)) break;
            sra_append(sra, m);
        }
        fclose(f);
    }
}

/* ------------------------------------------------------------------ */
/* Session helpers                                                      */
/* ------------------------------------------------------------------ */

void sra_clear_session(SraCore *sra) {
    memset(sra->sty_session_init, 0, sizeof(sra->sty_session_init));
    memset(sra->sty_session_note, 0, sizeof(sra->sty_session_note));
}

void sra_make_session_init(SraCore *sra, long ind, int k, int s, int st) {
    SRABYTE last = sra->last_sty_msg;
    if ((last & 0xc0) == 0xc0) {
        sra->sty_session_init[k][s][st][last & 0x0f][2] = sra->style_buf[ind];
    } else if ((last & 0xb0) == 0xb0) {
        SRABYTE cc = sra->style_buf[ind - 1];
        if (cc == 0 || cc == 32)
            sra->sty_session_init[k][s][st][last & 0x0f][cc / 32] =
                sra->style_buf[ind];
    }
}

void sra_make_session_note(SraCore *sra, long ind) {
    SRABYTE last = sra->last_sty_msg;
    SRABYTE ch   = last & 0x0f;
    if ((last & 0x90) == 0x90 && (ch == ACC3 || ch == ACC4)) {
        if (sra->style_buf[ind] != 0x00)
            sra_inc_voice(sra, last, sra->style_buf[ind - 1],
                          sra->style_buf[ind]);
        else
            sra_dec_voice(sra, last, sra->style_buf[ind - 1]);
    }
}

void sra_save_session_note(SraCore *sra, int k, int s, int st) {
    int i, j;
    for (i = 0, j = 0; i < MAXVOICE && j < (MAXVOICE / 2 - 1); i++) {
        if (sra->voice[i][0] != 0x00) {
            sra->sty_session_note[k][s][st][j][0] = sra->voice[i][0];
            sra->sty_session_note[k][s][st][j][1] = sra->voice[i][1];
            sra->sty_session_note[k][s][st][j][2] = sra->voice[i][2];
            j++;
        }
    }
}

/* ------------------------------------------------------------------ */
/* MoveCom: relocate CC/PC messages to start of their time slice        */
/* ------------------------------------------------------------------ */

void sra_move_com(SraCore *sra, long a, long b) {
    long i, j, start, end, n;
    SRABYTE data[MAXQUEUE];
    SRABYTE *buf = sra->style_buf;

    for (i = a + 2; i <= b; i++) {
        SRABYTE type = buf[i] & 0xf0;
        if (type == 0xb0 || type == 0xc0) {
            n = (type == 0xb0) ? 3 : 2;
            start = i;
            i++;
            while (1) {
                i += n;
                if (buf[i - 1] != 0x00 || buf[i] >= 0x80) break;
            }
            end = i = i - 2;
            for (j = start; j <= end; j++) data[j - start] = buf[j];
            for (j = start - 2; j >= a + 1; j--)
                buf[j + (end - start + 2)] = buf[j];
            for (j = start; j <= end; j++)
                buf[j - start + a + 1] = data[j - start];
            buf[end - start + a + 2] = 0x00;
        }
    }
}

/* ------------------------------------------------------------------ */
/* Style header parser (v2 format)                                      */
/*                                                                      */
/* Scans MTrk events until the first non-Meta event, looking for        */
/* a Sequencer-Specific Meta (FF 7F) with our signature BE EF and       */
/* version 01.  On success fills sra->tempo/beat/t_time/il/ml_a/        */
/* ml_b/el and leaves the file positioned at the first body event.      */
/* On failure reports Error(2) and returns 0.                           */
/* ------------------------------------------------------------------ */

/* Read a VLQ (up to 2 bytes; v2 does not use longer ones). */
static long read_vlq_2(FILE *f) {
    long v = (SRABYTE)fgetc(f);
    if (v >= 0x80)
        v = (v - 0x80) * 128 + (SRABYTE)fgetc(f);
    return v;
}

int parse_style_header(FILE *f, SraCore *sra) {
    int     i, k;
    SRABYTE status, type, len;
    SRABYTE data[32];

    /* Skip MThd (14 bytes) and MTrk chunk header (8 bytes). */
    if (fseek(f, 14, SEEK_SET) != 0) { sra_do_error(sra, 2); return 0; }
    if (fseek(f, 8,  SEEK_CUR) != 0) { sra_do_error(sra, 2); return 0; }

    for (i = 0; i < 64; i++) {          /* max 64 header events */
        (void)read_vlq_2(f);            /* delta — not used in header */
        status = (SRABYTE)fgetc(f);

        if (status == 0xFF) {
            type = (SRABYTE)fgetc(f);
            len  = (SRABYTE)fgetc(f);
            if (len > sizeof(data)) { sra_do_error(sra, 2); return 0; }
            for (k = 0; k < len; k++) data[k] = (SRABYTE)fgetc(f);

            if (type == 0x7F && len == 11 &&
                data[0] == 0xBE && data[1] == 0xEF) {
                if (data[2] != 0x01) { sra_do_error(sra, 2); return 0; }

                sra->tempo  = data[3];
                sra->beat   = data[4];
                sra->t_time = (long)data[5] * 128 + data[6];
                sra->il     = data[7];
                sra->ml_a   = data[8];
                sra->ml_b   = data[9];
                sra->el     = data[10];

                if (sra->tempo < 20 || sra->tempo > 250) { sra_do_error(sra, 3); return 0; }
                if (sra->beat != 2 && sra->beat != 3 &&
                    sra->beat != 4 && sra->beat != 6)   { sra_do_error(sra, 3); return 0; }
                if (sra->il   < 1 || sra->il   > SESSION_MAX) { sra_do_error(sra, 3); return 0; }
                if (sra->ml_a < 1 || sra->ml_a > SESSION_MAX) { sra_do_error(sra, 3); return 0; }
                if (sra->ml_b < 1 || sra->ml_b > SESSION_MAX) { sra_do_error(sra, 3); return 0; }
                if (sra->el   < 1 || sra->el   > SESSION_MAX) { sra_do_error(sra, 3); return 0; }

                sra->tempo *= 2;
                return 1;   /* file is now at the first body event */
            }
            /* Not ours — skip and continue. */
        } else if (status >= 0x80 && status <= 0xEF) {
            sra_do_error(sra, 2);   /* first body event, no SRA header */
            return 0;
        } else {
            sra_do_error(sra, 2);   /* unexpected byte */
            return 0;
        }
    }
    sra_do_error(sra, 2);           /* too many header events */
    return 0;
}

/* ------------------------------------------------------------------ */
/* Style file loader                                                    */
/* ------------------------------------------------------------------ */

int sra_load_style(SraCore *sra, int style_num) {
    SRABYTE c;
    int     i;
    int     kind = 0;
    long    index_h = -1, index_n = -1;
    int     session = 0, session_time = 0;
    FILE   *f;

    /* Build "<styles_dir>/style<NN>.mid", avoiding a double
       slash when styles_dir already ends with '/'.
       Guard against truncation: if the resulting path would not
       fit in style_name[], refuse to load (caller reports
       Error(1) "Style file not found").  In practice styles_dir
       is short; the check just keeps -Wformat-truncation quiet
       and makes the failure mode explicit. */
    {
        size_t dlen = strlen(sra->styles_dir);
        int    sep  = (dlen > 0 && sra->styles_dir[dlen - 1] == '/') ? 0 : 1;
        size_t need = dlen + (size_t)sep + sizeof("/style127.mid");
        if (need > sizeof(sra->style_name)) {
            /* styles_dir too long to fit in style_name */
            sra->style_name[0] = '\0';
            return 0;
        }
        if (sep) {
            snprintf(sra->style_name, sizeof(sra->style_name),
                     "%s/style%d.mid", sra->styles_dir, style_num);
        } else {
            snprintf(sra->style_name, sizeof(sra->style_name),
                     "%sstyle%d.mid", sra->styles_dir, style_num);
        }
    }

    if (!(f = fopen(sra->style_name, "rb"))) return 0;

    /* Parse v2 header (Meta FF 7F, signature BE EF). */
    if (!parse_style_header(f, sra)) {
        fclose(f);
        return 0;
    }

    /* Skip delta of the first body event.
       In v2, t_time comes from the Meta header, so this delta
       carries no information.  The body parser below expects to
       start at the status byte, not at the delta. */
    {
        SRABYTE d = (SRABYTE)fgetc(f);
        if (d >= 0x80) fgetc(f);   /* 2-byte VLQ */
    }

    sra->a_time = session = session_time = sra->sty_ptr[0][0][0] = 0;
    sra_clear_session(sra);

    while ((c = (SRABYTE)fgetc(f)) != (SRABYTE)0xff) {
        SRABYTE delta;
        if (c >= 0x80) {
            sra->style_buf[++index_n] = sra->last_sty_msg = c;
            c = (SRABYTE)fgetc(f);
        }
        if (index_h == index_n)
            sra->style_buf[++index_n] = sra->last_sty_msg;
        sra->style_buf[++index_n] = c;

        {
            SRABYTE ch = sra->last_sty_msg & 0x0f;
            if (ch != ACC1 && ch != ACC2 && ch != ACC3 &&
                ch != ACC4 && ch != ACC5 && ch != PHRASE &&
                ch != ACCBASS && ch != DRUM) {
                fclose(f); sra_do_error(sra, 4); return 0;
            }
        }
        if (sra->last_sty_msg <= (SRABYTE)0xbf ||
            sra->last_sty_msg >= (SRABYTE)0xe0)
            sra->style_buf[++index_n] = (SRABYTE)fgetc(f);

        if (index_n > (STYLESIZE - 10)) {
            fclose(f); sra_do_error(sra, 7); return 0;
        }
        sra_make_session_note(sra, index_n);
        sra_make_session_init(sra, index_n, kind, session, session_time);

        delta = (SRABYTE)fgetc(f);
        if (delta >= 0x80) {
            sra->style_buf[++index_n] = delta;
            delta = (SRABYTE)fgetc(f);
            sra->b_time = (long)(sra->style_buf[index_n] - 128) * 128 + delta;
        } else {
            sra->b_time = (long)delta;
        }
        sra->style_buf[++index_n] = delta;

        if (sra->b_time) {
            sra_move_com(sra, index_h, index_n);
            index_h = index_n;
        }
        sra->a_time += sra->b_time;
        if (sra->a_time > sra->t_time) { fclose(f); sra_do_error(sra, 5); return 0; }

        if (sra->a_time == sra->t_time) {
            sra_save_session_note(sra, kind, session, session_time);
            sra->a_time = 0;

            if (session == 0) {
                if (session_time < (sra->il - 1)) session_time++;
                else { session_time = 0; session++; }
            } else if (session == 1) {
                if (session_time < (sra->ml_a - 1)) session_time++;
                else { session_time = 0; session++; }
            } else if (session == 3) {
                if (session_time < (sra->ml_b - 1)) session_time++;
                else { session_time = 0; session++; }
            } else if (session == 2 || session == 4) {
                session_time = 0; session++;
            } else if (session == 5) {
                if (session_time < (sra->el - 1)) session_time++;
                else {
                    if (kind == 2) break;
                    kind++; session_time = 0; session = 0;
                }
            }
            sra->sty_ptr[kind][session][session_time] = index_n + 1;
        }
    }

    if (kind != 2 || session != 5 || session_time != (sra->el - 1)) {
        fclose(f); sra_do_error(sra, 6); return 0;
    }
    sra->style_buf[++index_n] = 0xff;
    fclose(f);

    for (i = 0; i < MAXVOICE; i++) sra->voice[i][0] = 0x00;
    sra->voice_count = 0;
    sra_set_clock(sra);
    sra->var_f = sra->sync_f = sra->ief = 0;
    return 1;
}
