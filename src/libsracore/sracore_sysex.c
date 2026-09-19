/* sracore_sysex.c — SysEx control protocol (Manufacturer ID 0x7D)
 *
 * Wire format (framing F0/F7 already stripped by the platform layer):
 *     data[0] = 0x7D          (Manufacturer ID)
 *     data[1] = CMD           (command byte)
 *     data[2..len-1] = DATA   (optional parameter bytes, 7-bit)
 *
 * Unknown CMDs are silently ignored.
 * Malformed *own* messages are reported to stderr, never fatal.
 */

#include "sracore_private.h"

/* ------------------------------------------------------------------ */
/* Command codes (keep in sync with README)                             */
/* ------------------------------------------------------------------ */

#define SX_START            0x01
#define SX_STOP             0x02
#define SX_SYNC_START       0x03
#define SX_FILL_TO_ORIG     0x04
#define SX_FILL_TO_VAR      0x05
#define SX_INTRO_ENDING     0x06
#define SX_TEMPO_PLUS       0x07
#define SX_TEMPO_MINUS      0x08
#define SX_TOGGLE_MBASS     0x09
#define SX_TOGGLE_ACC       0x0A
#define SX_TOGGLE_ACCBASS   0x0B
#define SX_TOGGLE_DRUM      0x0C
#define SX_FADE_OUT         0x0D
#define SX_CHANGE_MODE      0x0E
#define SX_LOAD_STYLE       0x20
#define SX_NOTE_CMD_ENABLE  0x50

/* ------------------------------------------------------------------ */
/* Error reporting (non-fatal)                                          */
/* ------------------------------------------------------------------ */

static void sx_error(const char *what, SRABYTE cmd, int len) {
    fprintf(stderr, "SRA SysEx error: %s (CMD=0x%02X, len=%d)\n",
            what, (unsigned)cmd, len);
}

/* ------------------------------------------------------------------ */
/* Dispatch                                                             */
/* ------------------------------------------------------------------ */

void sra_sysex_dispatch(SraCore *sra, SRABYTE cmd,
                        const SRABYTE *data, int datalen) {
    SRABYTE d0 = (datalen >= 1) ? data[0] : 0;

    switch (cmd) {

    /* ---- Transport ------------------------------------------------ */
    case SX_START:
        if (datalen != 0) { sx_error("CMD 0x01 takes no data", cmd, datalen); return; }
        if (sra->start_f) return;            /* already running: ignore */
        sra->msg = (SRABYTE)(CMD_START + sra->offset);
        sra->shift_f = 0;                    /* normal start, not sync */
        sra_check_com(sra);
        break;

    case SX_STOP:
        if (datalen != 0) { sx_error("CMD 0x02 takes no data", cmd, datalen); return; }
        if (!sra->start_f) return;           /* already stopped: ignore */
        sra->msg = (SRABYTE)(CMD_START + sra->offset);
        sra->shift_f = 0;
        sra_check_com(sra);
        break;

    case SX_SYNC_START:
        if (datalen != 0) { sx_error("CMD 0x03 takes no data", cmd, datalen); return; }
        if (sra->start_f) return;
        sra->msg = (SRABYTE)(CMD_START + sra->offset);
        sra->shift_f = 1;                    /* triggers sync path */
        sra_check_com(sra);
        sra->shift_f = 0;                    /* do not leak the flag */
        break;

    /* ---- Sections / fills ---------------------------------------- */
    case SX_FILL_TO_ORIG:
        if (datalen != 0) { sx_error("CMD 0x04 takes no data", cmd, datalen); return; }
        sra->msg = (SRABYTE)(CMD_FILLTO + sra->offset);
        sra->shift_f = 0;
        sra_check_com(sra);
        break;

    case SX_FILL_TO_VAR:
        if (datalen != 0) { sx_error("CMD 0x05 takes no data", cmd, datalen); return; }
        sra->msg = (SRABYTE)(CMD_FILLTV + sra->offset);
        sra->shift_f = 0;
        sra_check_com(sra);
        break;

    case SX_INTRO_ENDING:
        if (datalen != 0) { sx_error("CMD 0x06 takes no data", cmd, datalen); return; }
        sra->msg = (SRABYTE)(CMD_IE + sra->offset);
        sra->shift_f = 0;
        sra_check_com(sra);
        break;

    /* ---- Tempo ---------------------------------------------------- */
    case SX_TEMPO_PLUS:
        if (datalen != 0) { sx_error("CMD 0x07 takes no data", cmd, datalen); return; }
        if (sra->tempo < 250) { sra->tempo++; sra_set_clock(sra); }
        break;

    case SX_TEMPO_MINUS:
        if (datalen != 0) { sx_error("CMD 0x08 takes no data", cmd, datalen); return; }
        if (sra->tempo > 20)  { sra->tempo--; sra_set_clock(sra); }
        break;

    /* ---- Part toggles -------------------------------------------- */
    case SX_TOGGLE_MBASS:   sra->mbass_vf    = 1 - sra->mbass_vf;    break;
    case SX_TOGGLE_ACC:     sra->acc_vf      = 1 - sra->acc_vf;      break;
    case SX_TOGGLE_ACCBASS: sra->acc_bass_vf = 1 - sra->acc_bass_vf; break;
    case SX_TOGGLE_DRUM:    sra->drum_vf     = 1 - sra->drum_vf;     break;

    /* ---- Misc ----------------------------------------------------- */
    case SX_FADE_OUT:
        if (datalen != 0) { sx_error("CMD 0x0D takes no data", cmd, datalen); return; }
        sra->msg = (SRABYTE)(CMD_FADEOUT + sra->offset);
        sra->shift_f = 0;
        sra_check_com(sra);
        break;

    case SX_CHANGE_MODE:
        if (datalen != 0) { sx_error("CMD 0x0E takes no data", cmd, datalen); return; }
        sra->msg = (SRABYTE)(CMD_CHMODE + sra->offset);
        sra->shift_f = 0;
        sra_check_com(sra);
        break;

    /* ---- Style loading ------------------------------------------- */
    case SX_LOAD_STYLE:
        if (datalen != 1) { sx_error("CMD 0x20 requires 1 data byte", cmd, datalen); return; }
        if (d0 > 127)     { sx_error("CMD 0x20 data out of range",   cmd, datalen); return; }
        sra_load_style(sra, (int)d0);
        break;

    /* ---- Note-On command enable ---------------------------------- */
    case SX_NOTE_CMD_ENABLE:
        if (datalen != 1)        { sx_error("CMD 0x50 requires 1 data byte", cmd, datalen); return; }
        if (d0 != 0 && d0 != 1)  { sx_error("CMD 0x50 data must be 0 or 1",  cmd, datalen); return; }
        sra->note_cmd_enabled = d0;
        break;

    /* ---- Unknown CMD: silently ignore ---------------------------- */
    default:
        break;
    }
}

/* ------------------------------------------------------------------ */
/* Entry point                                                          */
/* ------------------------------------------------------------------ */

void sracore_sysex_in(SraCore *sra, const SRABYTE *data, int len) {
    SRABYTE cmd;

    if (!sra || !data || len <= 0) return;

    /* Not ours? Ignore silently. */
    if (data[0] != 0x7D) return;

    /* Bare manufacturer ID with no command byte. */
    if (len < 2) {
        fprintf(stderr, "SRA SysEx error: missing CMD byte (len=%d)\n", len);
        return;
    }

    cmd = data[1];

    /* Unknown CMD -> silent (dispatcher's default case).
       Known CMD -> validate data length/range inside the dispatcher. */
    sra_sysex_dispatch(sra, cmd, data + 2, len - 2);
}
