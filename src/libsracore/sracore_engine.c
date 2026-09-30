/* sracore_engine.c — real-time step, style playback, transport commands */

#include "sracore_private.h"

/* Delay (in microseconds) after the last chord note-on before the
   chord is analysed.  Avoids analysing intermediate states (1-2 notes)
   when the keys do not arrive simultaneously. */
#define CHORD_DEBOUNCE_US 30000L

/* ------------------------------------------------------------------ */
/* CountNote: re-apply voices for the current session position          */
/* ------------------------------------------------------------------ */

void sra_count_note(SraCore *sra) {
    int     i, j;
    SRABYTE c, d, e;
    SRABYTE *buf  = sra->style_buf;
    int      kind = (int)sra->chord_k;
    int      sess = (int)sra->session;
    int      stime= (int)sra->session_time;

    DBG("count_note: kind=%d sess=%d stime=%d a_time=%ld sty_ptr=%ld\n",
        kind, sess, stime, sra->a_time, sra->sty_ptr[kind][sess][stime]);

    if (stime >= 1) {
        /* Apply program-change initialisers from the previous measure */
        for (i = 0; i < 16; i++) {
            if (sra->sty_session_init[kind][sess][stime-1][i][0]) {
                sra->prog_t[i][0] = sra->sty_session_init[kind][sess][stime-1][i][0];
                sra->prog_t[i][1] = sra->sty_session_init[kind][sess][stime-1][i][1];
                sra->prog_t[i][2] = sra->sty_session_init[kind][sess][stime-1][i][2];
                sra_prog_change(sra, (SRABYTE)(i & 0x0f));
            }
        }
        /* EXPERIMENT: session snapshot restore DISABLED.
           See if this removes the residual hanging notes on ch=11/12. */
        if (0) {
            for (i = 0; sra->sty_session_note[kind][sess][stime-1][i][0]; i++) {
                c = sra->sty_session_note[kind][sess][stime-1][i][0];
                d = sra->sty_session_note[kind][sess][stime-1][i][1];
                e = sra->sty_session_note[kind][sess][stime-1][i][2];
                sra_inc_voice(sra, c, d, e);
            }
        }
    }

    /* Replay style events up to the current tick position */
    sra->b_time  = 0;
    sra->sty_index = -1;

    while (sra->b_time < sra->a_time) {
        long base = sra->sty_ptr[kind][sess][stime];
        c = buf[base + (++sra->sty_index)];
        if (c >= 0x80) {
            if (c == (SRABYTE)0xff) {
                sra->msg = (SRABYTE)(CMD_START + sra->offset);
                sra_check_com(sra);
                break;
            }
            sra->last_sty_msg = c;
            d = buf[base + (++sra->sty_index)];
        } else {
            d = c; c = sra->last_sty_msg;
        }

        if (sra->last_sty_msg <= (SRABYTE)0xbf ||
            sra->last_sty_msg >= (SRABYTE)0xe0) {
            e = buf[base + (++sra->sty_index)];
            /* NoteOn (0x90) or NoteOff (0x80).  Normalise cmd to
               0x90|ch, force vel = 0 on NoteOff. */
            SRABYTE msg_type = sra->last_sty_msg & 0xf0;
            if (msg_type == 0x80 || msg_type == 0x90) {
                SRABYTE norm_cmd = 0x90 | (sra->last_sty_msg & 0x0f);
                SRABYTE vel_eff  = (msg_type == 0x80) ? 0 : e;
                SRABYTE ch = norm_cmd & 0x0f;
                if (ch==ACC1||ch==ACC2||ch==ACC3||ch==ACC4||
                    ch==ACC5||ch==PHRASE) {
                    vel_eff = (SRABYTE)(vel_eff * sra->acc_vf * sra->chord_c);
                    d += sra->chord_v[d % 12];
                    d += sra->chordd;
                    if (sra->chordd >= 8) d -= 12;
                } else if (ch == ACCBASS) {
                    vel_eff = (SRABYTE)(vel_eff * sra->acc_bass_vf * sra->chord_c);
                    if (sra->chordd == sra->bass) {
                        if (d % 12 != 0) d += sra->chord_v[d % 12];
                        d += sra->chordd;
                        if (sra->chordd >= 4) d -= 12;
                    } else {
                        d = sra->bass + 24;
                    }
                    if (d < 28) d += 12;
                } else if (ch == DRUM) {
                    vel_eff = (SRABYTE)(vel_eff * sra->drum_vf);
                } else {
                    vel_eff = 0;
                }
                if (ch != DRUM) {
#ifdef SRA_DEBUG_VOICE
                    if (ch == 11) {
                        DBG("%s ch=11 %s note=%d vel=%d vc=%d\n",
                            __func__,
                            (msg_type == 0x80) ? "OFF" :
                            (msg_type == 0x90) ? "ON " : "OTH",
                            d, vel_eff, sra->voice_count);
                    }
#endif
                    if (vel_eff) sra_inc_voice(sra, norm_cmd, d, vel_eff);
                    else         sra_dec_voice(sra, norm_cmd, d);
                }
            } else if ((c & 0xb0) == 0xb0) {
                if (d == 0)  sra->prog_t[c & 0x0f][0] = e;
                else if (d == 32) sra->prog_t[c & 0x0f][1] = e;
            }
        } else if ((c & 0xc0) == 0xc0) {
            sra->prog_t[c & 0x0f][2] = d;
            sra_prog_change(sra, c & 0x0f);
        }

        /* Read delta time */
        c = buf[base + (++sra->sty_index)];
        i = (c >= 0x80) ? (int)(c - 128) * 128 +
                          buf[base + (++sra->sty_index)] : (int)c;
        sra->b_time += (long)i;

        if (sra->key_change) sra->bass_lock = 0;
    }

    /* Fix bass voice: redirect to actual bass note */
    for (i = j = 0; j < sra->voice_count; i++) {
        if (sra->bass_lock == 0 && (sra->voice[i][0] & 0x0f) == ACCBASS) {
            sra->bass_o    = sra->voice[i][1];
            sra->voice[i][1] = sra->bass + 24;
            if (sra->voice[i][1] < 28) sra->voice[i][1] += 12;
            sra->bass_lock = 1;
        }
        if (sra->voice[i][0] != 0x00) {
            sra_append(sra, sra->voice[i][0]);
            sra_append(sra, sra->voice[i][1]);
            sra_append(sra, sra->voice[i][2]);
            j++;
        }
    }
}

/* ------------------------------------------------------------------ */
/* DumpSty: play all style events at the current tick                   */
/* ------------------------------------------------------------------ */

void sra_dump_sty(SraCore *sra) {
    SRABYTE c, d, e;
    SRABYTE *buf  = sra->style_buf;
    int      kind = (int)sra->chord_k;
    int      sess = (int)sra->session;
    int      stime= (int)sra->session_time;
    int      i;

    while (sra->a_time == sra->b_time) {
        long base = sra->sty_ptr[kind][sess][stime];
        c = buf[base + (++sra->sty_index)];
        if (c >= 0x80) {
            if (c == (SRABYTE)0xff) {
                sra->msg = (SRABYTE)(CMD_START + sra->offset);
                sra_check_com(sra);
                return;
            }
            sra->last_sty_msg = c;
            d = buf[base + (++sra->sty_index)];
        } else {
            d = c; c = sra->last_sty_msg;
        }

        if (sra->last_sty_msg <= (SRABYTE)0xbf ||
            sra->last_sty_msg >= (SRABYTE)0xe0) {
            e = buf[base + (++sra->sty_index)];
            /* NoteOn (0x90) or NoteOff (0x80).  Normalise to 0x90|ch
               so that sra_dec_voice finds the voice recorded by
               sra_inc_voice at NoteOn time.  NoteOff forces vel = 0. */
            {
                SRABYTE msg_type = sra->last_sty_msg & 0xf0;
                if (msg_type == 0x80 || msg_type == 0x90) {
                    SRABYTE norm_cmd = 0x90 | (sra->last_sty_msg & 0x0f);
                    SRABYTE vel_eff  = (msg_type == 0x80) ? 0 : e;
                    SRABYTE ch = norm_cmd & 0x0f;
                    if (ch==ACC1||ch==ACC2||ch==ACC3||ch==ACC4||
                        ch==ACC5||ch==PHRASE) {
                        vel_eff  = (SRABYTE)(vel_eff * sra->acc_vf * sra->chord_c);
                        d += sra->chord_v[d % 12];
                        d += sra->chordd;
                        if (sra->chordd >= 8) d -= 12;
                    } else if (ch == ACCBASS) {
                        vel_eff = (SRABYTE)(vel_eff * sra->acc_bass_vf * sra->chord_c);
                        if (sra->chordd == sra->bass) {
                            if (d % 12 != 0) d += sra->chord_v[d % 12];
                            d += sra->chordd;
                            if (sra->chordd >= 4) d -= 12;
                        } else {
                            d = sra->bass + 24;
                        }
                        if (d < 28) d += 12;
                        if (sra->bass_lock == 0 && vel_eff) {
                            sra->bass_o = d;
                            d = sra->bass + 24;
                            if (d < 28) d += 12;
                            sra->bass_lock = 1;
                        } else if (sra->bass_lock == 1 && !vel_eff && d == sra->bass_o) {
                            d = sra->bass + 24;
                            if (d < 28) d += 12;
                            sra->bass_lock = -1;
                        }
                    } else {
                        vel_eff = (SRABYTE)(vel_eff * sra->drum_vf);
                    }
                    if (ch != DRUM) {
#ifdef SRA_DEBUG_VOICE
                        if (ch == 11) {
                            DBG("%s ch=11 %s note=%d vel=%d vc=%d\n",
                                __func__,
                                (msg_type == 0x80) ? "OFF" :
                                (msg_type == 0x90) ? "ON " : "OTH",
                                d, vel_eff, sra->voice_count);
                        }
#endif
                        if (vel_eff) sra_inc_voice(sra, norm_cmd, d, vel_eff);
                        else         sra_dec_voice(sra, norm_cmd, d);
                    }
                }
            }
            if ((c & 0xb0) == 0xb0) {
                if      (d == 0)  sra->prog_t[c & 0x0f][0] = e;
                else if (d == 32) sra->prog_t[c & 0x0f][1] = e;
                else { sra_append(sra, c); sra_append(sra, d); sra_append(sra, e); }
            } else {
                sra_append(sra, c); sra_append(sra, d); sra_append(sra, e);
            }
        } else {
            if ((c & 0xc0) == 0xc0) {
                sra->prog_t[c & 0x0f][2] = d;
                sra_prog_change(sra, c & 0x0f);
            } else {
                sra_append(sra, c); sra_append(sra, d);
            }
        }

        /* Read next delta time */
        c = buf[sra->sty_ptr[kind][sess][stime] + (++sra->sty_index)];
        i = (c >= 0x80) ? (int)(c - 128) * 128 +
                          buf[sra->sty_ptr[kind][sess][stime] + (++sra->sty_index)]
                        : (int)c;
        sra->b_time += (long)i;
    }
}

/* ------------------------------------------------------------------ */
/* CheckCom: transport and arrangement commands                          */
/* ------------------------------------------------------------------ */

void sra_check_com(SraCore *sra) {
    int cmd = (int)sra->msg - sra->offset;
    switch (cmd) {
    case CMD_SHIFT:
        sra->shift_f = 1;
        break;
    case CMD_FILLTO:
        if (!sra->shift_f && sra->start_f) sra->fill_f = 1;
        sra->var_f = 0;
        break;
    case CMD_FILLTV:
        if (!sra->shift_f && sra->start_f) sra->fill_f = 1;
        sra->var_f = 1;
        break;
    case CMD_START:
        if (!sra->shift_f || sra->start_f) {
            if (sra->start_f) {
                sra_chord_off(sra);
                sra->key_on_count = 0;
                sra_lower_on(sra);
                sra_all_note_off(sra);
                if (sra->clock_f) sra_append(sra, 0xfc);
            } else {
                sra_reset(sra, 1);
                sra->sty_index = -1;
                sra->a_time = sra->b_time = 0;
                sra->session_time = 0;
                sra->clock3 = 0;
                if (!sra->ief) sra->session = 1 + (2 * sra->var_f);
                else           { sra->session = 0; sra_lower_off(sra); }
                if (sra->clock_f) sra_append(sra, 0xfa);
            }
            sra->start_f = 1 - sra->start_f;
            sra->sync_f = sra->ief = sra->fill_f = 0;
        } else {
            sra->sync_f = 1;
            sra_chord_off(sra);
        }
        break;
    case CMD_IE:
        if (!sra->shift_f) sra->ief = 1;
        else if (!sra->start_f) sra->func = 1;
        break;
    case CMD_FADEOUT:
        if (sra->fadeout_f == -1) sra->fadeout_f = 127;
        else                      sra->fadeout_f = -2;
        break;
    case CMD_INCTEMPO:
        if (sra->tempo < 250) { sra->tempo++; sra_set_clock(sra); }
        break;
    case CMD_DECTEMPO:
        if (sra->tempo > 20)  { sra->tempo--; sra_set_clock(sra); }
        break;
    case CMD_CHMODE:
        if (sra->mode) { sra_chord_off(sra); sra->key_on_count = 0; }
        sra->mode = 1 - sra->mode;
        break;
    case CMD_CHMBASSV:   sra->mbass_vf    = 1 - sra->mbass_vf;    break;
    case CMD_CHACCV:     sra->acc_vf      = 1 - sra->acc_vf;      break;
    case CMD_CHACCBASSV: sra->acc_bass_vf = 1 - sra->acc_bass_vf; break;
    case CMD_CHDRUMV:    sra->drum_vf     = 1 - sra->drum_vf;     break;
    }
}

/* ------------------------------------------------------------------ */
/* Fadeout helpers (used only inside sra_step)                          */
/* ------------------------------------------------------------------ */

static void emit_volume_all(SraCore *sra, SRABYTE vol) {
    static const SRABYTE CH[] = {
        LOWER, MBASS, ACC1, ACC2, ACC3, ACC4, ACC5, PHRASE, ACCBASS, DRUM
    };
    int i;
    if (!sra->voice_lock) {
        sra_append(sra, 0xb0 | LOWER); sra_append(sra, 0x0b); sra_append(sra, vol);
    }
    for (i = 1; i < 10; i++) {
        sra_append(sra, 0xb0 | CH[i]);
        sra_append(sra, 0x0b);
        sra_append(sra, vol);
    }
}

/* ------------------------------------------------------------------ */
/* sra_step: advance one time slice                                     */
/* ------------------------------------------------------------------ */

void sra_step(SraCore *sra) {
    if (sra->que_lock) return;

    /* Advance chord debounce timer and, if it has expired, try to
       recognise the chord.  chord_change is set by sra_check_chord()
       on every note-on, and cleared inside sra_make_chord(). */
    if (sra->chord_change) {
        sra->chord_debounce += sra->wait;
        if (sra->chord_debounce >= CHORD_DEBOUNCE_US && !sra->start_f)
            sra_make_chord(sra);
    }

    /* Update clock via platform timer callback */
    if (sra->cb.on_timer) sra->cb.on_timer(sra, sra->cb.userdata);

    sra->clock4 = (sra->now_usec + 1000000L - sra->clock) % 1000000L;
    if (sra->clock4 < 0) sra->clock4 = 0;
    sra->wait2 += sra->clock4;
    sra->clock  = sra->now_usec;

    if (sra->wait2 < sra->wait) return;
    sra->wait2 -= sra->wait;

    if (sra->clock3 == 0 && sra->chord_change &&
        sra->chord_debounce >= CHORD_DEBOUNCE_US)
        sra_make_chord(sra);

    /* Fadeout */
    if (sra->fadeout_f == 127) { sra->clock2 = 0; sra->fadeout_f--; }
    if (sra->fadeout_f > 0) {
        sra->clock2 = (sra->clock2 + 1) % (sra->tempo / 4);
        if (sra->clock2 == 0) {
            sra->fadeout_f--;
            emit_volume_all(sra, (SRABYTE)sra->fadeout_f);
        }
    } else if (sra->fadeout_f == -2) {
        emit_volume_all(sra, 127);
        sra->fadeout_f = -1;
    }

    if (!sra->start_f) goto check_sync;

    /* MIDI clock pulse every 5 steps */
    if (sra->clock3 == 0) sra_append(sra, 0xf8);
    sra->clock3 = (sra->clock3 + 1) % 5;

    if (sra->key_change) {
        DBG("key_change: kind=%d sess=%ld stime=%ld a_time=%ld vc=%d\n",
            (int)sra->chord_k, sra->session, sra->session_time,
            sra->a_time, sra->voice_count);
        sra_all_note_off(sra);
        sra_count_note(sra);
        sra->key_change = 0;
    }
    if (!sra->start_f) return;

    /* Fill transition */
    if (sra->fill_f &&
        sra->session != 2 && sra->session != 4 &&
        sra->a_time <= (sra->t_time / (2 * sra->beat) * (2 * sra->beat - 1))) {
        sra->session      = 4 - (sra->var_f * 2);
        sra->session_time = 0;
        sra_all_note_off(sra);
        sra_count_note(sra);
        sra->fill_f = 0;
    }

    /* Bar boundary */
    if (sra->a_time == 0) {
        if (sra->ief2)              { sra_all_note_off(sra); sra->ief2 = 0; }
        if (sra->session == 1 && sra->var_f == 1) { sra_all_note_off(sra); sra->session = 3; }
        else if (sra->session == 3 && sra->var_f == 0) { sra_all_note_off(sra); sra->session = 1; }
        if (sra->ief && sra->session != 2 && sra->session != 4) {
            sra_all_note_off(sra);
            sra->ief = 0; sra->session = 5; sra->session_time = 0;
            sra_lower_off(sra);
        }
    }

    sra_dump_sty(sra);
    sra->a_time++;

    if (sra->a_time == sra->t_time) {
        if (sra->session == 0 && sra->session_time == (sra->il - 1)) {
            sra->session = 2 * sra->var_f + 1;
            sra->session_time = 0;
            sra_lower_on(sra); sra->ief2 = 1;
        } else if (sra->session == 1 || sra->session == 3) {
            sra->session_time = (sra->session_time + 1) %
                                ((sra->session == 1) ? sra->ml_a
                                                     : sra->ml_b);
        } else if (sra->session == 2 || sra->session == 4) {
            sra->session = 2 * sra->var_f + 1;
            sra->session_time = 0;
        } else if (sra->session == 5 && sra->session_time == (sra->el - 1)) {
            sra->msg = (SRABYTE)(CMD_START + sra->offset);
            sra_check_com(sra);
            return;
        } else {
            sra->session_time++;
        }
        sra->sty_index = -1;
        sra->a_time = sra->b_time = 0;
    }

check_sync:
    if (!sra->start_f && sra->sync_f && sra->chord_c) {
        sra->msg = (SRABYTE)(CMD_START + sra->offset);
        sra_check_com(sra);
    }
}
