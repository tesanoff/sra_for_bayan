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
#define SX_TO_ORIGINAL      0x0F
#define SX_TO_VARIATION     0x10
#define SX_TOGGLE_LOWER     0x53
#define SX_LOAD_STYLE       0x20
#define SX_NOTE_CMD_ENABLE  0x50
#define SX_SET_CHORD_CH     0x51
#define SX_MASTER_VOLUME    0x52

/* ------------------------------------------------------------------ */
/* Command name table                                                   */
/* ------------------------------------------------------------------ */

/* Return a human-readable name for a known SysEx CMD, or NULL. */
static const char *sysex_cmd_name(SRABYTE cmd) {
    switch (cmd) {
    case SX_START:           return "Start";
    case SX_STOP:            return "Stop";
    case SX_SYNC_START:      return "Sync Start";
    case SX_FILL_TO_ORIG:    return "Fill to Original";
    case SX_FILL_TO_VAR:     return "Fill to Variation";
    case SX_INTRO_ENDING:    return "Intro / Ending";
    case SX_TEMPO_PLUS:      return "Tempo +";
    case SX_TEMPO_MINUS:     return "Tempo -";
    case SX_TOGGLE_MBASS:    return "Toggle M.Bass";
    case SX_TOGGLE_ACC:      return "Toggle Acc.";
    case SX_TOGGLE_ACCBASS:  return "Toggle Acc.Bass";
    case SX_TOGGLE_DRUM:     return "Toggle Drum";
    case SX_FADE_OUT:        return "Fade Out";
    case SX_CHANGE_MODE:     return "Change Mode";
    case SX_TO_ORIGINAL:     return "To Original";
    case SX_TO_VARIATION:    return "To Variation";
    case SX_TOGGLE_LOWER:    return "Toggle Lower";
    case SX_LOAD_STYLE:      return "Load Style";
    case SX_NOTE_CMD_ENABLE: return "Enable/disable Note-On";
    case SX_SET_CHORD_CH:    return "Set Chord Ch";
    case SX_MASTER_VOLUME:   return "Master Volume";
    default:                 return NULL;
    }
}

/* Format data bytes as a space-separated hex string into buf. */
static void sysex_format_hex(const SRABYTE *data, int len,
                             char *buf, size_t buflen) {
    size_t pos = 0;
    int    i;

    if (buflen == 0) return;
    buf[0] = '\0';

    for (i = 0; i < len && pos + 3 < buflen; i++) {
        int n = snprintf(buf + pos, buflen - pos, "%s%02X",
                         (i == 0) ? "" : " ", (unsigned)data[i]);
        if (n < 0) break;
        pos += (size_t)n;
    }
}

/* ------------------------------------------------------------------ */
/* Error reporting (non-fatal)                                          */
/* ------------------------------------------------------------------ */

/* Report a malformed SysEx message.  Includes the command name
   (if known) and a caller-supplied detail string. */
static void sx_error(SraCore *sra, SRABYTE cmd, const char *detail) {
    const char *name = sysex_cmd_name(cmd);

    if (name)
        sra_log_error(sra, "SRA SysEx error: CMD 0x%02X %s: %s",
                      (unsigned)cmd, name, detail);
    else
        sra_log_error(sra, "SRA SysEx error: CMD 0x%02X: %s",
                      (unsigned)cmd, detail);
}

/* ------------------------------------------------------------------ */
/* Dispatch                                                             */
/* ------------------------------------------------------------------ */

void sra_sysex_dispatch(SraCore *sra, SRABYTE cmd,
                        const SRABYTE *data, int datalen) {
    SRABYTE d0 = (datalen >= 1) ? data[0] : 0;

    /* Debug log: print the raw command as received. */
    if (sra->debug) {
        const char *name = sysex_cmd_name(cmd);
        char hex[512];
        char hexcmd[8];
        char hexdata[512];

        snprintf(hexcmd, sizeof(hexcmd), "%02X", (unsigned)cmd);
        sysex_format_hex(data, datalen, hexdata, sizeof(hexdata));

        if (hexdata[0])
            snprintf(hex, sizeof(hex), "%s %s", hexcmd, hexdata);
        else
            snprintf(hex, sizeof(hex), "%s", hexcmd);

        if (name)
            sra_log_debug(sra, "[SRA] SysEx 7D %s -> CMD 0x%02X %s",
                          hex, (unsigned)cmd, name);
        else
            sra_log_debug(sra, "[SRA] SysEx 7D %s -> CMD 0x%02X Unknown",
                          hex, (unsigned)cmd);
    }

    switch (cmd) {

    /* ---- Transport ------------------------------------------------ */
    case SX_START:
        if (datalen != 0) {
            char detail[64];
            snprintf(detail, sizeof(detail),
                     "unexpected data byte 0x%02X (expected none)",
                     (unsigned)d0);
            sx_error(sra, cmd, detail);
            return;
        }
        if (sra->start_f) return;            /* already running: ignore */
        sra->msg = (SRABYTE)(CMD_START + sra->offset);
        sra->shift_f = 0;                    /* normal start, not sync */
        sra_check_com(sra);
        break;

    case SX_STOP:
        if (datalen != 0) {
            char detail[64];
            snprintf(detail, sizeof(detail),
                     "unexpected data byte 0x%02X (expected none)",
                     (unsigned)d0);
            sx_error(sra, cmd, detail);
            return;
        }
        if (!sra->start_f) return;           /* already stopped: ignore */
        sra->msg = (SRABYTE)(CMD_START + sra->offset);
        sra->shift_f = 0;
        sra_check_com(sra);
        break;

    case SX_SYNC_START:
        if (datalen != 0) {
            char detail[64];
            snprintf(detail, sizeof(detail),
                     "unexpected data byte 0x%02X (expected none)",
                     (unsigned)d0);
            sx_error(sra, cmd, detail);
            return;
        }
        if (sra->start_f) return;
        sra->msg = (SRABYTE)(CMD_START + sra->offset);
        sra->shift_f = 1;                    /* triggers sync path */
        sra_check_com(sra);
        sra->shift_f = 0;                    /* do not leak the flag */
        break;

    /* ---- Sections / fills ---------------------------------------- */
    case SX_FILL_TO_ORIG:
        if (datalen != 0) {
            char detail[64];
            snprintf(detail, sizeof(detail),
                     "unexpected data byte 0x%02X (expected none)",
                     (unsigned)d0);
            sx_error(sra, cmd, detail);
            return;
        }
        sra->msg = (SRABYTE)(CMD_FILLTO + sra->offset);
        sra->shift_f = 0;
        sra_check_com(sra);
        break;

    case SX_FILL_TO_VAR:
        if (datalen != 0) {
            char detail[64];
            snprintf(detail, sizeof(detail),
                     "unexpected data byte 0x%02X (expected none)",
                     (unsigned)d0);
            sx_error(sra, cmd, detail);
            return;
        }
        sra->msg = (SRABYTE)(CMD_FILLTV + sra->offset);
        sra->shift_f = 0;
        sra_check_com(sra);
        break;

    case SX_INTRO_ENDING:
        if (datalen != 0) {
            char detail[64];
            snprintf(detail, sizeof(detail),
                     "unexpected data byte 0x%02X (expected none)",
                     (unsigned)d0);
            sx_error(sra, cmd, detail);
            return;
        }
        sra->msg = (SRABYTE)(CMD_IE + sra->offset);
        sra->shift_f = 0;
        sra_check_com(sra);
        break;

    case SX_TO_ORIGINAL:
        if (datalen != 0) {
            char detail[64];
            snprintf(detail, sizeof(detail),
                     "unexpected data byte 0x%02X (expected none)",
                     (unsigned)d0);
            sx_error(sra, cmd, detail);
            return;
        }
        sra->var_f = 0;
        break;

    case SX_TO_VARIATION:
        if (datalen != 0) {
            char detail[64];
            snprintf(detail, sizeof(detail),
                     "unexpected data byte 0x%02X (expected none)",
                     (unsigned)d0);
            sx_error(sra, cmd, detail);
            return;
        }
        sra->var_f = 1;
        break;

    /* ---- Tempo ---------------------------------------------------- */
    case SX_TEMPO_PLUS:
        if (datalen != 0) {
            char detail[64];
            snprintf(detail, sizeof(detail),
                     "unexpected data byte 0x%02X (expected none)",
                     (unsigned)d0);
            sx_error(sra, cmd, detail);
            return;
        }
        if (sra->tempo < 250) { sra->tempo++; sra_set_clock(sra); }
        break;

    case SX_TEMPO_MINUS:
        if (datalen != 0) {
            char detail[64];
            snprintf(detail, sizeof(detail),
                     "unexpected data byte 0x%02X (expected none)",
                     (unsigned)d0);
            sx_error(sra, cmd, detail);
            return;
        }
        if (sra->tempo > 20)  { sra->tempo--; sra_set_clock(sra); }
        break;

    /* ---- Part toggles -------------------------------------------- */
    case SX_TOGGLE_MBASS:   sra->mbass_vf    = 1 - sra->mbass_vf;    break;
    case SX_TOGGLE_ACC:     sra->acc_vf      = 1 - sra->acc_vf;      break;
    case SX_TOGGLE_ACCBASS: sra->acc_bass_vf = 1 - sra->acc_bass_vf; break;
    case SX_TOGGLE_DRUM:    sra->drum_vf     = 1 - sra->drum_vf;     break;

    /* ---- Misc ----------------------------------------------------- */
    case SX_FADE_OUT:
        if (datalen != 0) {
            char detail[64];
            snprintf(detail, sizeof(detail),
                     "unexpected data byte 0x%02X (expected none)",
                     (unsigned)d0);
            sx_error(sra, cmd, detail);
            return;
        }
        sra->msg = (SRABYTE)(CMD_FADEOUT + sra->offset);
        sra->shift_f = 0;
        sra_check_com(sra);
        break;

    case SX_CHANGE_MODE:
        if (datalen != 0) {
            char detail[64];
            snprintf(detail, sizeof(detail),
                     "unexpected data byte 0x%02X (expected none)",
                     (unsigned)d0);
            sx_error(sra, cmd, detail);
            return;
        }
        sra->msg = (SRABYTE)(CMD_CHMODE + sra->offset);
        sra->shift_f = 0;
        sra_check_com(sra);
        break;

    /* ---- Lower toggle -------------------------------------------- */
    case SX_TOGGLE_LOWER:
        if (datalen != 0) {
            char detail[64];
            snprintf(detail, sizeof(detail),
                     "unexpected data byte 0x%02X (expected none)",
                     (unsigned)d0);
            sx_error(sra, cmd, detail);
            return;
        }
        sra->lower_vf = 1 - sra->lower_vf;
        /* Refresh the sounding chord so LOWER notes are cleanly
           replaced (same approach as CMD_CHMODE). */
        sra_chord_off(sra);
        sra_chord_on(sra);
        break;

    /* ---- Style loading ------------------------------------------- */
    case SX_LOAD_STYLE:
        if (datalen != 1) {
            char detail[64];
            snprintf(detail, sizeof(detail),
                     "missing data byte (expected 1, got %d)", datalen);
            sx_error(sra, cmd, detail);
            return;
        }
        if (d0 > 127) {
            char detail[64];
            snprintf(detail, sizeof(detail),
                     "data 0x%02X out of range (0x00..0x7F)",
                     (unsigned)d0);
            sx_error(sra, cmd, detail);
            return;
        }
        sra_load_style(sra, (int)d0);
        break;

    /* ---- Chord channel ------------------------------------------- */
    case SX_SET_CHORD_CH:
        if (datalen != 1) {
            char detail[64];
            snprintf(detail, sizeof(detail),
                     "missing data byte (expected 1, got %d)", datalen);
            sx_error(sra, cmd, detail);
            return;
        }
        if (d0 > 15) {
            char detail[64];
            snprintf(detail, sizeof(detail),
                     "data 0x%02X out of range (0x00..0x0F)",
                     (unsigned)d0);
            sx_error(sra, cmd, detail);
            return;
        }
        sra->chord_ch = d0;
        break;

    /* ---- Master volume ------------------------------------------- */
    case SX_MASTER_VOLUME:
        if (datalen != 1) {
            char detail[64];
            snprintf(detail, sizeof(detail),
                     "missing data byte (expected 1, got %d)", datalen);
            sx_error(sra, cmd, detail);
            return;
        }
        if (d0 > 127) {
            char detail[64];
            snprintf(detail, sizeof(detail),
                     "data 0x%02X out of range (0x00..0x7F)",
                     (unsigned)d0);
            sx_error(sra, cmd, detail);
            return;
        }
        sra->master_vol = d0;
        /* Apply immediately: CC7 = <VOL> on all arranger-owned channels
           (same list as emit_volume_all in sracore_engine.c). */
        {
            static const SRABYTE CH[] = {
                LOWER, MBASS, ACC1, ACC2, ACC3, ACC4, ACC5, PHRASE, ACCBASS, DRUM
            };
            int i;
            for (i = 0; i < 10; i++) {
                sra_append(sra, (SRABYTE)(0xb0 | CH[i]));
                sra_append(sra, 0x07);          /* CC7 = Channel Volume */
                sra_append(sra, d0);
            }
        }
        break;

    /* ---- Note-On command enable ---------------------------------- */
    case SX_NOTE_CMD_ENABLE:
        if (datalen != 1) {
            char detail[64];
            snprintf(detail, sizeof(detail),
                     "missing data byte (expected 1, got %d)", datalen);
            sx_error(sra, cmd, detail);
            return;
        }
        if (d0 != 0 && d0 != 1) {
            char detail[64];
            snprintf(detail, sizeof(detail),
                     "data 0x%02X invalid (expected 0x00 or 0x01)",
                     (unsigned)d0);
            sx_error(sra, cmd, detail);
            return;
        }
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
        sra_log_error(sra,
            "SRA SysEx error: missing CMD byte (message too short)");
        return;
    }

    cmd = data[1];

    /* Unknown CMD -> silent (dispatcher's default case).
       Known CMD -> validate data length/range inside the dispatcher. */
    sra_sysex_dispatch(sra, cmd, data + 2, len - 2);
}
