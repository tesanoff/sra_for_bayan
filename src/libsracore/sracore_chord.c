/* sracore_chord.c — chord type table, detection, on/off voicing */

#include "sracore_private.h"

/* ------------------------------------------------------------------ */
/* Chord type table  (replaces the 22 individual X*() functions)        */
/* ------------------------------------------------------------------ */

typedef struct {
    const char      suffix[8];    /* name suffix after root note */
    int             kind;         /* ChordK: 0=major 1=minor 2=other */
    const signed char voicing[12];/* per-semitone voicing offset */
} ChordDef;

static const ChordDef CHORD_TABLE[CHORD_COUNT] = {
    /* CHORD_X       */ {"",       0, { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}},
    /* CHORD_X6      */ {"6",      2, { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,-1, 0}},
    /* CHORD_X7      */ {"7",      2, { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}},
    /* CHORD_XMAJ7   */ {"maj7",   2, { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,+1, 0}},
    /* CHORD_X9      */ {"9",      2, {+2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}},
    /* CHORD_XADD9   */ {"add9",   2, { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,+4, 0}},
    /* CHORD_XMAJ9   */ {"maj9",   2, {+2, 0, 0, 0, 0, 0, 0, 0, 0, 0,+1, 0}},
    /* CHORD_XSUS4   */ {"sus4",   0, { 0,-1,-2,-3,+1, 0,-1, 0,-1,-2,-3,-4}},
    /* CHORD_X7SUS4  */ {"7sus4",  2, { 0,-1,-2,-3,+1, 0,-1, 0,-1,-2, 0,-1}},
    /* CHORD_XM      */ {"m",      1, { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}},
    /* CHORD_XM6     */ {"m6",     2, { 0, 0, 0, 0,-1, 0, 0, 0, 0, 0,-1, 0}},
    /* CHORD_XM7     */ {"m7",     2, { 0, 0, 0, 0,-1, 0, 0, 0, 0, 0, 0, 0}},
    /* CHORD_XM9     */ {"m9",     2, {+2, 0, 0, 0,-1, 0, 0, 0, 0, 0, 0, 0}},
    /* CHORD_XMADD9  */ {"madd9",  2, { 0, 0, 0, 0,-1, 0, 0, 0, 0, 0,+4, 0}},
    /* CHORD_XMSUS4  */ {"msus4",  1, { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}},
    /* CHORD_XM7SUS4 */ {"m7sus4", 2, { 0, 0, 0, 0,-1, 0, 0, 0, 0, 0, 0, 0}},
    /* CHORD_XAUG    */ {"aug",    0, { 0, 0, 0, 0, 0,+1, 0,+1, 0, 0, 0, 0}},
    /* CHORD_XDIM    */ {"dim",    1, { 0, 0, 0, 0, 0,+1, 0,-1, 0, 0, 0, 0}},
    /* CHORD_XDIM7   */ {"dim7",   2, { 0, 0,+1, 0,-1,+1, 0,-1, 0, 0,-1, 0}},
    /* CHORD_XM75B   */ {"m75b",   2, { 0, 0,+1, 0,-1,+1, 0,-1, 0, 0, 0, 0}},
    /* CHORD_XSUS    */ {"sus",    0, { 0, 0, 0, 0,-4, 0, 0, 0, 0, 0, 0, 0}},
    /* CHORD_X7SUS   */ {"7sus",   2, { 0, 0, 0, 0,-4, 0, 0, 0, 0, 0, 0, 0}},
};

/* ------------------------------------------------------------------ */
/* Note name helper                                                     */
/* ------------------------------------------------------------------ */

void sra_set_note_name(char *buf, SRABYTE note) {
    static const char *const NAMES[12] = {
        "C","C#","D","Eb","E","F","F#","G","Ab","A","Bb","B"
    };
    strcpy(buf, NAMES[note % 12]);
}

/* ------------------------------------------------------------------ */
/* Apply a chord type: update chord_name, chord_k, chord_v, callback    */
/* ------------------------------------------------------------------ */

static void apply_chord_type(SraCore *sra, ChordTypeId type) {
    const ChordDef *def = &CHORD_TABLE[type];
    char notename[4];

    sra_set_note_name(notename, sra->chordd);
    strcpy(sra->chord_name, notename);
    strcat(sra->chord_name, def->suffix);

    if (sra->chordd != sra->bass) {
        sra_set_note_name(notename, sra->bass);
        strcat(sra->chord_name, "/");
        strcat(sra->chord_name, notename);
    }

    sra->chord_k = (SRABYTE)def->kind;
    sra->chord_v = def->voicing;

    if (sra->cb.on_chord) sra->cb.on_chord(sra, sra->cb.userdata);
}

/* ------------------------------------------------------------------ */
/* Chord voicing on/off                                                 */
/* ------------------------------------------------------------------ */

void sra_chord_off(SraCore *sra) {
    int i;
    for (i = 0; i <= sra->key_off_count; i++) {
        sra_append(sra, 0x90 | LOWER);
        sra_append(sra, sra->key_off[i]);
        sra_append(sra, 0x00);
    }
    sra->key_off_count = -1;

    sra_append(sra, 0xb0 | MBASS);
    sra_append(sra, 0x78);
    sra_append(sra, 0x00);

    if (!sra->start_f) {
        sra->chord_c = 0;
        strcpy(sra->chord_name, "");
        if (sra->cb.on_chord) sra->cb.on_chord(sra, sra->cb.userdata);
    }
}

void sra_chord_on(SraCore *sra) {
    int i;
    if (!sra->start_f && sra->mode && !sra->sync_f) {
        sra_append(sra, 0x90 | MBASS);
        sra_append(sra, sra->key_on[0][0] - sra->offset3);
        sra_append(sra, sra->key_on[0][1] * sra->mbass_vf);
    }
    for (i = 0; i < sra->key_on_count; i++) {
        sra->key_off[++sra->key_off_count] =
            sra->key_on[i][0] + 12 + sra->offset2 - sra->offset3;
        sra_append(sra, 0x90 | LOWER);
        sra_append(sra, sra->key_off[sra->key_off_count]);
        sra_append(sra, sra->key_on[i][1] * sra->mode);
    }
    sra->chord_c = 1;
}

/* ------------------------------------------------------------------ */
/* Set chord root, retrigger voicing, apply type                        */
/* ------------------------------------------------------------------ */

void sra_set_chord(SraCore *sra, SRABYTE root, ChordTypeId type) {
    sra->bass      = sra->key_on[0][0] % 12;
    sra->chordd    = root % 12;
    sra->key_change = 1;
    sra_chord_off(sra);
    sra_chord_on(sra);
    apply_chord_type(sra, type);
}

/* ------------------------------------------------------------------ */
/* Chord key press / release                                            */
/* ------------------------------------------------------------------ */

void sra_check_chord(SraCore *sra, int vel) {
    int i, j;
    SRABYTE temp;

    if (vel && sra->key_on_count < 5) {
        /* Ignore duplicate note-on: if the same note is already held
           (e.g. when re-triggering a chord before releasing the old one),
           do not add it a second time, as that would break chord
           recognition. */
        for (i = 0; i < sra->key_on_count; i++) {
            if (sra->key_on[i][0] == sra->msg) return;
        }

        /* insert into KeyOn[] sorted ascending by note */
        sra->key_on[sra->key_on_count][0] = sra->msg;
        sra->key_on[sra->key_on_count][1] = sra->key_v;
        for (i = sra->key_on_count; i > 0; i--) {
            if (sra->msg < sra->key_on[i - 1][0]) {
                sra->key_on[i][0]     = sra->key_on[i - 1][0];
                sra->key_on[i - 1][0] = sra->msg;
                temp                   = sra->key_on[i - 1][1];
                sra->key_on[i - 1][1] = sra->key_on[i][1];
                sra->key_on[i][1]     = temp;
            } else break;
        }
        sra->key_on_count++;
        sra->chord_change = 1;
        sra->chord_debounce = 0;   /* restart debounce timer */
    } else if (!vel) {
        for (i = 0; i < sra->key_on_count; i++) {
            if (sra->key_on[i][0] == sra->msg) {
                for (j = i; j < sra->key_on_count - 1; j++) {
                    sra->key_on[j][0] = sra->key_on[j + 1][0];
                    sra->key_on[j][1] = sra->key_on[j + 1][1];
                }
                sra->key_on_count--;
                break;
            }
        }
    }
}

/* ------------------------------------------------------------------ */
/* CheckKeyOn — legacy note-on path (chord_ch < 0 only)                 */
/*                                                                      */
/* Only used when the engine runs in the legacy "any channel, by        */
/* pitch" mode.  With the default chord-channel routing this function   */
/* is never called: sracore_midi_in() performs chord detection itself.  */
/* ------------------------------------------------------------------ */

void sra_check_key_on(SraCore *sra) {
    /* Read the note that was just appended (2 positions back in queue) */
    SRABYTE note = sra->queue[(sra->que_t - 2 + MAXQUEUE) % MAXQUEUE];
    sra->msg = note;

    if (!sra->func) {
        if (!sra->note_cmd_enabled) {
            /* Note-On commands disabled: only chord keys are processed
               (and only in legacy chord_ch < 0 mode). */
            if (sra->chord_ch < 0 &&
                note < (SRABYTE)(CMD_UPPERD + sra->offset3 + sra->offset4) &&
                (sra->mode || sra->start_f || sra->sync_f)) {
                sra->key_v = sra->queue[(sra->que_t - 1 + MAXQUEUE) % MAXQUEUE];
                sra->queue[(sra->que_t - 1 + MAXQUEUE) % MAXQUEUE] = 0x00;
                sra_check_chord(sra, sra->key_v);
            }
            return;
        }

        if (note <= (SRABYTE)(CMD_COMMANDU + sra->offset) &&
                   note >= (SRABYTE)(CMD_COMMANDD + sra->offset)) {
            sra->queue[(sra->que_t - 1 + MAXQUEUE) % MAXQUEUE] = 0x00;
            sra_check_com(sra);

        } else if (note < (SRABYTE)(CMD_COMMANDD + sra->offset) &&
                   note >= (SRABYTE)(CMD_SHCOMMANDD + sra->offset) &&
                   sra->shift_f) {
            sra->queue[(sra->que_t - 1 + MAXQUEUE) % MAXQUEUE] = 0x00;
            sra_check_com(sra);

        } else if (sra->chord_ch < 0 &&
                   note < (SRABYTE)(CMD_UPPERD + sra->offset3 + sra->offset4) &&
                   (sra->mode || sra->start_f || sra->sync_f)) {
            /* chord key (legacy pitch-based mode only): read velocity,
               suppress it in output queue, feed the chord detector. */
            sra->key_v = sra->queue[(sra->que_t - 1 + MAXQUEUE) % MAXQUEUE];
            sra->queue[(sra->que_t - 1 + MAXQUEUE) % MAXQUEUE] = 0x00;
            sra_check_chord(sra, sra->key_v);
        }

    } else {
        /* Function mode: every key is a command or style selector */
        if (note == (SRABYTE)(CMD_SHIFT + sra->offset)) {
            sra_do_error(sra, 0); /* exit: Shift+Func+Shift */
        } else if (note == (SRABYTE)(CMD_FADEOUT + sra->offset)) {
            sra->offset2 = (sra->offset2 == 12) ? -12 : sra->offset2 + 12;
        } else if (note == (SRABYTE)(CMD_START + sra->offset)) {
            if (sra->offset3 == 12) { sra->offset3 = -12; sra->offset -= 24; }
            else                    { sra->offset3 += 12;  sra->offset += 12; }
        } else if (note == (SRABYTE)(CMD_INCTEMPO + sra->offset)) {
            if (sra->offset4 < 6) { sra->offset4++; sra->offset++; }
        } else if (note == (SRABYTE)(CMD_DECTEMPO + sra->offset)) {
            if (sra->offset4 > -6) { sra->offset4--; sra->offset--; }
        } else if (note == (SRABYTE)(CMD_IE + sra->offset)) {
            sra->clock_f = 1 - sra->clock_f;
            sra->queue[(sra->que_t - 1 + MAXQUEUE) % MAXQUEUE] = 0x00;
            sra_reset(sra, 0);
            sra->func = 0;
            return;
        } else {
            sra_load_style(sra, note - sra->offset3 - sra->offset4);
        }
        sra->queue[(sra->que_t - 1 + MAXQUEUE) % MAXQUEUE] = 0x00;
        sra->func = 0;
    }
}

void sra_check_key_off(SraCore *sra) {
    SRABYTE note = sra->queue[(sra->que_t - 2 + MAXQUEUE) % MAXQUEUE];
    sra->msg = note;

    if (note == (SRABYTE)(CMD_SHIFT + sra->offset)) {
        sra->shift_f = 0;
    } else if (sra->chord_ch < 0 &&
               note < (SRABYTE)(CMD_UPPERD + sra->offset3 + sra->offset4) &&
               (sra->mode || sra->start_f || sra->sync_f)) {
        sra_check_chord(sra, 0);
    }
}

/* ------------------------------------------------------------------ */
/* MakeChord: try to identify chord from held keys                      */
/* ------------------------------------------------------------------ */

void sra_make_chord(SraCore *sra) {
    if (!sra->chord_change) return;

#define K(i) sra->key_on[i][0]

    if (sra->key_on_count == 5) {
        if (!sra_comp_chord(sra,5,K(0),K(1),K(2),K(3),K(4)) &&
            !sra_comp_chord(sra,4,K(1),K(2),K(3),K(4),0x00) &&
            !sra_comp_chord(sra,4,K(2),K(3),K(4),K(1)+12,0x00) &&
            !sra_comp_chord(sra,4,K(3),K(4),K(1)+12,K(2)+12,0x00) &&
            !sra_comp_chord(sra,4,K(4),K(1)+12,K(2)+12,K(3)+12,0x00))
            {}
    } else if (sra->key_on_count == 4) {
        if (!sra_comp_chord(sra,4,K(0),K(1),K(2),K(3),0x00) &&
            !sra_comp_chord(sra,4,K(1),K(2),K(3),K(0)+12,0x00) &&
            !sra_comp_chord(sra,4,K(2),K(3),K(0)+12,K(1)+12,0x00) &&
            !sra_comp_chord(sra,4,K(3),K(0)+12,K(1)+12,K(2)+12,0x00) &&
            !sra_comp_chord(sra,3,K(1),K(2),K(3),0x00,0x00) &&
            !sra_comp_chord(sra,3,K(2),K(3),K(1)+12,0x00,0x00) &&
            !sra_comp_chord(sra,3,K(3),K(1)+12,K(2)+12,0x00,0x00))
            {}
    } else if (sra->key_on_count == 3) {
        if (!sra_comp_chord(sra,3,K(0),K(1),K(2),0x00,0x00) &&
            !sra_comp_chord(sra,3,K(1),K(2),K(0)+12,0x00,0x00) &&
            !sra_comp_chord(sra,3,K(2),K(0)+12,K(1)+12,0x00,0x00))
            {}
    }
#undef K
    sra->chord_change = 0;
}

/* ------------------------------------------------------------------ */
/* CompChord: interval-based chord recognition                          */
/* ------------------------------------------------------------------ */

int sra_comp_chord(SraCore *sra, int n,
                   SRABYTE a, SRABYTE b, SRABYTE c, SRABYTE d, SRABYTE e) {
    if (n == 5) {
        if (e-a==14 && c-a==7) {
            if      (b-a==4 && d-a==10) { sra_set_chord(sra,a,CHORD_X9);     return 1; }
            else if (b-a==4 && d-a==11) { sra_set_chord(sra,a,CHORD_XMAJ9);  return 1; }
            else if (b-a==3 && d-a==10) { sra_set_chord(sra,a,CHORD_XM9);    return 1; }
        } else if (b-a==2 && d-a==7) {
            if      (c-a==4 && e-a==10) { sra_set_chord(sra,a,CHORD_X9);     return 1; }
            else if (c-a==4 && e-a==11) { sra_set_chord(sra,a,CHORD_XMAJ9);  return 1; }
            else if (c-a==3 && e-a==10) { sra_set_chord(sra,a,CHORD_XM9);    return 1; }
        } else if (b-a==3 && c-a==5 && d-a==7 && e-a==10) {
            sra_set_chord(sra,a,CHORD_XM7SUS4); return 1;
        }
    } else if (n == 4) {
        if (c-a==7) {
            if (d-a==9) {
                if      (b-a==4) { sra_set_chord(sra,a,CHORD_X6);    return 1; }
                else if (b-a==3) { sra_set_chord(sra,a,CHORD_XM6);   return 1; }
            } else if (d-a==10) {
                if      (b-a==5) { sra_set_chord(sra,a,CHORD_X7SUS4);return 1; }
                else if (b-a==4) { sra_set_chord(sra,a,CHORD_X7);    return 1; }
                else if (b-a==3) { sra_set_chord(sra,a,CHORD_XM7);   return 1; }
            } else if (d-a==14) {
                if      (b-a==4) { sra_set_chord(sra,a,CHORD_XADD9); return 1; }
                else if (b-a==3) { sra_set_chord(sra,a,CHORD_XMADD9);return 1; }
            } else if (d-a==11 && b-a==4) {
                sra_set_chord(sra,a,CHORD_XMAJ7); return 1;
            }
        } else if (d-a==7) {
            if (b-a==2) {
                if (c-a==4) {
                    if (a == sra->key_on[0][0] + 10) sra_set_chord(sra,a,CHORD_X);
                    else                              sra_set_chord(sra,a,CHORD_XADD9);
                    return 1;
                } else if (c-a==3) {
                    if (a == sra->key_on[0][0] + 10) sra_set_chord(sra,a,CHORD_XM);
                    else                              sra_set_chord(sra,a,CHORD_XMADD9);
                    return 1;
                }
            } else if (b-a==3 && c-a==5) {
                sra_set_chord(sra,a,CHORD_XMSUS4); return 1;
            }
        } else if (d-a==9 && b-a==3 && c-a==6) {
            sra_set_chord(sra,a,CHORD_XDIM7);  return 1;
        } else if (d-a==10 && b-a==3 && c-a==6) {
            sra_set_chord(sra,a,CHORD_XM75B); return 1;
        }
    } else { /* n == 3 */
        if      (b-a==5 && c-a==7)  { sra_set_chord(sra,a,CHORD_XSUS4); return 1; }
        else if (b-a==4 && c-a==7)  { sra_set_chord(sra,a,CHORD_X);     return 1; }
        else if (b-a==4 && c-a==8)  { sra_set_chord(sra,a,CHORD_XAUG);  return 1; }
        else if (b-a==3 && c-a==7)  { sra_set_chord(sra,a,CHORD_XM);    return 1; }
        else if (b-a==3 && c-a==6)  { sra_set_chord(sra,a,CHORD_XDIM);  return 1; }
        else if (b-a==7 && c-a==10) { sra_set_chord(sra,a,CHORD_X7SUS); return 1; }
        else if (b-a==7 && c-a==12) { sra_set_chord(sra,a,CHORD_XSUS);  return 1; }
        /* Bayan left-hand dom7 without the 5th: root, major 3rd, minor 7th
           e.g. C-E-Bb = (0, 4, 10). */
        else if (b-a==4 && c-a==10) { sra_set_chord(sra,a,CHORD_X7);    return 1; }
    }
    return 0;
}
