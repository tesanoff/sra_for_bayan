/* sra_ui_linux.c — Console-mode user interface (Linux) */

#include "sra_ui.h"
#include <termios.h>

/* Module-level state kept for ui_refresh / ui_notify_chord */
static MidiDevice *g_dev = NULL;
static AppState   *g_app = NULL;
static char        g_chord[16]  = "-";
static int         g_bpm        = 0;

/* ------------------------------------------------------------------ */
/* Init                                                                 */
/* ------------------------------------------------------------------ */

void ui_init(AppUI *ui) {
    (void)ui;
}

/* ------------------------------------------------------------------ */
/* Setup display                                                        */
/* ------------------------------------------------------------------ */

void ui_print_state(AppUI *ui, MidiDevice *dev, AppState *app) {
    (void)ui;   /* AppUI carries no fields on Linux */
    g_dev = dev;
    g_app = app;

    /* ANSI: clear screen, cursor home */
    printf("\033[2J\033[H");
    printf("Software-based Real-time Arranger v4.06 by ZZ-Denis @ NazoMusic\n");
    printf("===============================================================\n\n");

    if (dev->in_count == 0 && dev->out_count == 0) {
        printf("  No ALSA rawmidi ports found.\n");
        printf("  Check that a MIDI interface is connected and\n");
        printf("  'snd_rawmidi' kernel module is loaded.\n\n");
    } else {
        int in_ok  = dev->in_count  > 0;
        int out_ok = dev->out_count > 0;

        printf("  [Q/Z] MIDI IN  [%d/%d]: %s\n",
               in_ok  ? dev->in_index  + 1 : 0, dev->in_count,
               in_ok  ? dev->in_ports [dev->in_index ].addr : "(none)");
        if (in_ok)
            printf("         %s\n",
                   dev->in_ports[dev->in_index].name);

        printf("  [E/C] MIDI OUT [%d/%d]: %s\n",
               out_ok ? dev->out_index + 1 : 0, dev->out_count,
               out_ok ? dev->out_ports[dev->out_index].addr : "(none)");
        if (out_ok)
            printf("         %s\n",
                   dev->out_ports[dev->out_index].name);

        printf("  [A/D] Chord Ch: %d\n", app->chord_channel + 1);
        printf("  [O]   Ctrl Offset: %d\n\n", app->ctrl_offset);

        if (in_ok && out_ok)
            printf("  [S] START\n");
        else
            printf("  (Need both IN and OUT devices to start)\n");
    }
    fflush(stdout);
}

void ui_refresh(AppUI *ui) {
    if (g_dev && g_app) ui_print_state(ui, g_dev, g_app);
}

/* ------------------------------------------------------------------ */
/* Running phase: chord / tempo display                                 */
/* ------------------------------------------------------------------ */

void ui_notify_chord(const char *chord_name, int bpm) {
    strncpy(g_chord, chord_name ? chord_name : "-", 15);
    g_chord[15] = '\0';
    g_bpm = bpm;
    /* \r overwrites the current line without scrolling. */
    printf("\r  Chord: %-14s  Tempo: %3d BPM  ", g_chord, g_bpm);
    fflush(stdout);
}

/* ------------------------------------------------------------------ */
/* Keyboard handling (setup phase)                                      */
/* ------------------------------------------------------------------ */

int ui_on_keydown(AppUI *ui, MidiDevice *dev, unsigned int ch,
                  AppState *app) {
    int start = 0;

    switch (ch | 0x20u) { /* fold to lowercase */
    case 'q': midi_device_select_in (dev, +1); break;
    case 'z': midi_device_select_in (dev, -1); break;
    case 'e': midi_device_select_out(dev, +1); break;
    case 'c': midi_device_select_out(dev, -1); break;
    case 'a': if (app->chord_channel  < 15) app->chord_channel++;  break;
    case 'd': if (app->chord_channel  > 0)  app->chord_channel--;  break;
    case 'o':
        app->ctrl_offset++;
        if (app->ctrl_offset == 2) app->ctrl_offset = -1;
        break;
    case 's': start = 1; break;
    }

    if (!start) ui_refresh(ui);
    return start;
}
