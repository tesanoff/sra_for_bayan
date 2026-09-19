/* Internal header — NOT part of the public API.
   Include only inside libsracore .c files. */

#ifndef SRACORE_PRIVATE_H
#define SRACORE_PRIVATE_H

#include "sracore.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SRACORE_VERSION "1.3"

/* ------------------------------------------------------------------ */
/* Sizes / limits                                                        */
/* ------------------------------------------------------------------ */
#define MAXQUEUE   384
#define MAXVOICE   64
#define STYLESIZE  44000

/* ------------------------------------------------------------------ */
/* MIDI channel assignments used by the arranger                        */
/* ------------------------------------------------------------------ */
#define LOWER    0x0f
#define MBASS    0x0d
#define ACC1     0x09
#define ACC2     0x0a
#define ACC3     0x0b
#define ACC4     0x0c
#define ACCBASS  0x07
#define DRUM     0x0e

/* ------------------------------------------------------------------ */
/* Control command note numbers (relative to sra->offset)               */
/* ------------------------------------------------------------------ */
#define CMD_SHIFT       96
#define CMD_FILLTO      95
#define CMD_FILLTV      93
#define CMD_START       94
#define CMD_IE          92
#define CMD_FADEOUT     90
#define CMD_INCTEMPO    87
#define CMD_DECTEMPO    85
#define CMD_CHMODE      91
#define CMD_CHMBASSV    89
#define CMD_CHACCV      88
#define CMD_CHACCBASSV  86
#define CMD_CHDRUMV     84
#define CMD_COMMANDU    96
#define CMD_COMMANDD    92
#define CMD_SHCOMMANDD  84
#define CMD_UPPERD      60
#define CMD_INCPATCH    23
#define CMD_DECPATCH    21

/* ------------------------------------------------------------------ */
/* Chord type identifiers (index into CHORD_TABLE in sracore_chord.c)   */
/* ------------------------------------------------------------------ */
typedef enum {
    CHORD_X = 0,
    CHORD_X6, CHORD_X7, CHORD_XMAJ7, CHORD_X9, CHORD_XADD9, CHORD_XMAJ9,
    CHORD_XSUS4, CHORD_X7SUS4,
    CHORD_XM, CHORD_XM6, CHORD_XM7, CHORD_XM9, CHORD_XMADD9,
    CHORD_XMSUS4, CHORD_XM7SUS4,
    CHORD_XAUG, CHORD_XDIM, CHORD_XDIM7, CHORD_XM75B,
    CHORD_XSUS, CHORD_X7SUS,
    CHORD_COUNT
} ChordTypeId;

/* ------------------------------------------------------------------ */
/* Full engine state struct                                              */
/* ------------------------------------------------------------------ */
struct SraCore {
    /* MIDI channel routing */
    int  key_ch;
    int  chord_ch;              /* channel reserved for chord input, -1 = legacy */
    int  offset, offset2, offset3, offset4;

    /* Key tracking for chord detection */
    SRABYTE key_on[5][2];   /* [slot][0=note, 1=velocity], sorted ascending by note */
    SRABYTE key_off[5];     /* notes to silence on chord change */
    int     key_on_count;
    int     key_off_count;
    int     key_change;

    /* MIDI output ring buffer (also used for chord read-back, see sracore.c) */
    SRABYTE queue[MAXQUEUE];
    int     que_h;          /* consumer head */
    int     que_t;          /* producer tail */
    int     que_lock;

    /* Active polyphonic voices */
    SRABYTE voice[MAXVOICE][3]; /* [slot][0=status, 1=note, 2=velocity] */
    int     voice_lock;
    int     voice_count;

    /* Timing */
    long t_time;        /* ticks per bar                (TTimee) */
    long a_time;        /* current tick in bar          (ATime)  */
    long b_time;        /* style reader tick position   (BTime)  */
    long wait;          /* usec per tick, from tempo    (Wait)   */
    long wait2;         /* accumulated usec             (Wait2)  */
    long now_usec;      /* platform clock, set by timer (NOWUSEC)*/
    long clock;         /* reference usec for delta     (Clock)  */
    long clock2;        /* fadeout sub-counter          (Clock2) */
    long clock3;        /* MIDI clock sub-counter       (Clock3) */
    long clock4;        /* delta scratch                (Clock4) */
    long session;
    long session_time;

    /* Style metadata */
    SRABYTE tempo, beat, il, ml, el;

    /* Chord state */
    SRABYTE          chord_change;
    long             chord_debounce;   /* usec since last chord note-on */
    SRABYTE          chord_c;          /* chord is sounding */
    SRABYTE          chordd;           /* chord root 0-11 */
    SRABYTE          chord_k;          /* kind: 0=major 1=minor 2=other */
    const signed char *chord_v;        /* voicing offset table [12] */
    SRABYTE          bass;             /* bass note 0-11 */
    SRABYTE          bass_o;           /* tracked bass MIDI note */
    int              bass_lock;

    /* Part on/off flags */
    SRABYTE mbass_vf, acc_vf, acc_bass_vf, drum_vf;

    /* Transport / arrangement flags */
    SRABYTE clock_f, var_f, sync_f, start_f, fill_f;
    SRABYTE ief, ief2, shift_f, mode, func;
    int     fadeout_f;

    /* Current MIDI message scratch */
    SRABYTE msg;
    SRABYTE key_v;
    SRABYTE last_sty_msg;
    SRABYTE patch;

    /* SysEx control */
    SRABYTE note_cmd_enabled;   /* 1 = Note-On command notes active */

    /* Per-channel program tracking */
    SRABYTE prog[16][3];
    SRABYTE prog_t[16][3];

    /* Style file data */
    char    style_name[16];
    SRABYTE sty_session_init[3][6][8][16][3];
    SRABYTE sty_session_note[3][6][8][MAXVOICE / 2][3];
    char    chord_name[12];
    long    sty_ptr[3][6][8];
    long    sty_index;      /* current read offset into style_buf */
    SRABYTE *style_buf;     /* malloc'd STYLESIZE bytes */

    /* Callbacks */
    SraCallbacks cb;
};

/* ------------------------------------------------------------------ */
/* Internal function declarations (across all translation units)        */
/* ------------------------------------------------------------------ */

/* sracore.c */
void sra_append(SraCore *sra, SRABYTE b);
void sra_do_error(SraCore *sra, int code);

/* sracore_chord.c */
void sra_chord_off(SraCore *sra);
void sra_chord_on(SraCore *sra);
void sra_set_chord(SraCore *sra, SRABYTE root, ChordTypeId type);
void sra_check_key_on(SraCore *sra);
void sra_check_key_off(SraCore *sra);
void sra_check_chord(SraCore *sra, int vel);
void sra_make_chord(SraCore *sra);
int  sra_comp_chord(SraCore *sra, int n,
                    SRABYTE a, SRABYTE b, SRABYTE c, SRABYTE d, SRABYTE e);
void sra_set_note_name(char *buf, SRABYTE note);

/* sracore_style.c */
void sra_set_clock(SraCore *sra);
int  sra_load_style(SraCore *sra, int style_num);
void sra_clear_session(SraCore *sra);
void sra_make_session_init(SraCore *sra, long ind, int k, int s, int st);
void sra_make_session_note(SraCore *sra, long ind);
void sra_save_session_note(SraCore *sra, int k, int s, int st);
void sra_move_com(SraCore *sra, long a, long b);
void sra_inc_voice(SraCore *sra, SRABYTE cmd, SRABYTE note, SRABYTE vel);
void sra_dec_voice(SraCore *sra, SRABYTE cmd, SRABYTE note);
void sra_all_note_off(SraCore *sra);
void sra_lower_on(SraCore *sra);
void sra_lower_off(SraCore *sra);
void sra_prog_change(SraCore *sra, SRABYTE ch);
void sra_reset(SraCore *sra, int full);

/* sracore_engine.c */
void sra_step(SraCore *sra);
void sra_dump_sty(SraCore *sra);
void sra_count_note(SraCore *sra);
void sra_check_com(SraCore *sra);

#endif /* SRACORE_PRIVATE_H */
