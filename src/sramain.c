/* sramain.c — Linux console entry point
   Compile: see SRA/src/Makefile */

#include <termios.h>
#include <signal.h>
#include "sra_types.h"
#include "midi_device.h"
#include "sra_ui.h"
#include "sra_engine.h"

/* ---- module-level singletons ---- */

static MidiDevice g_midi;
static AppUI      g_ui;
static AppState   g_app;
static SraEngine  g_engine;

/* ---- terminal raw-mode helpers ---- */

static struct termios g_orig_termios;

static void restore_terminal(void) {
    tcsetattr(STDIN_FILENO, TCSANOW, &g_orig_termios);
    printf("\n");
}

static void set_raw_mode(void) {
    struct termios raw = g_orig_termios;
    raw.c_lflag &= (tcflag_t)~(ICANON | ECHO);
    raw.c_cc[VMIN]  = 1;
    raw.c_cc[VTIME] = 0;
    tcsetattr(STDIN_FILENO, TCSANOW, &raw);
}

/* ---- signal handler (Ctrl+C) ---- */

static void handle_sigint(int sig) {
    (void)sig;
    restore_terminal();
    exit(0);
}

/* ---- main ---- */

int main(void) {
    int ch, err;

    /* Save terminal state; restore on exit or Ctrl+C. */
    tcgetattr(STDIN_FILENO, &g_orig_termios);
    atexit(restore_terminal);
    signal(SIGINT, handle_sigint);

    midi_device_probe(&g_midi);
    ui_init(&g_ui);
    engine_init(&g_engine, &g_midi, NULL);

    /* Defaults for interactive setup.  SRA sends accompaniment and melody
       on channels 6..15 (1-based: 7..16); channels 0..5 (1-based: 1..6)
       are left free.  chord_channel is an input-only channel and is not
       affected by this reservation. */
    g_app.melody_channel = 6;   /* channel 7 (1-based) for melody */
    g_app.chord_channel  = 2;   /* channel 3 (1-based) for chords */

    if (g_midi.in_count > 0 && g_midi.out_count > 0)
        g_app.status = 1;

    /* ---- setup phase: raw keypresses drive device / channel selection ---- */
    set_raw_mode();
    ui_print_state(&g_ui, &g_midi, &g_app);

    while (g_app.status == 1) {
        ch = getchar();
        if (ch == EOF || ch == 3 /* Ctrl+C */) break;

        if (ui_on_keydown(&g_ui, &g_midi, (unsigned int)ch, &g_app)) {
            /* 'S' pressed: open devices and start the engine. */
            err = midi_device_open(&g_midi, NULL);
            if (err) {
                restore_terminal();
                fprintf(stderr, "MIDI open error %d "
                        "(IN=888, OUT=999)\n", err);
                return 1;
            }
            /* Restore terminal and print banner BEFORE starting engine,
               so the chord line always appears after "Running." and
               ui_notify_chord's \r keeps updating the same line. */
            tcsetattr(STDIN_FILENO, TCSANOW, &g_orig_termios);
            printf("\n\nRunning. Press Enter to quit.\n");
            fflush(stdout);
            engine_start(&g_engine,
                         g_app.melody_channel,
                         12 * g_app.ctrl_offset,
                         g_app.chord_channel);
            g_app.status = 3;
        }
    }

    if (g_app.status != 3) return 0;

    /* ---- running phase: wait for quit (terminal already restored above) ---- */
    /* Engine and MIDI-IN threads run in background. */
    getchar();

    return 0;
}
