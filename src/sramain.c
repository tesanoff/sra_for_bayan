/* sramain.c — Linux console entry point
   Compile: see SRA/src/Makefile */

#include <termios.h>
#include <signal.h>
#include <sys/select.h>
#include "sra_types.h"
#include "midi_device.h"
#include "sra_ui.h"
#include "sra_engine.h"
#include "sra_config.h"

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
    /* Request a graceful shutdown; the main loop will clean up. */
    g_engine.running = 0;
}

/* ---- main ---- */

int main(int argc, char **argv) {
    int ch, err;
    SraConfig cfg;

    sra_config_defaults(&cfg);
    if (sra_config_parse_args(&cfg, argc, argv) != 0)
        return 1;

    if (cfg.show_help)    { sra_config_print_help();    return 0; }
    if (cfg.show_version) { sra_config_print_version(); return 0; }

    if (sra_config_load(&cfg) != 0)
        return 1;

    /* Save terminal state; restore on exit or Ctrl+C. */
    tcgetattr(STDIN_FILENO, &g_orig_termios);
    atexit(restore_terminal);
    signal(SIGINT, handle_sigint);

    midi_device_probe(&g_midi);
    ui_init(&g_ui);
    engine_init(&g_engine, &g_midi, NULL);

    /* Apply values from config / command line.  Remaining defaults
       (chord_channel = 2, ctrl_offset = 0) are set by
       sra_config_defaults(). */
    g_app.chord_channel = cfg.chord_ch;
    g_app.ctrl_offset   = cfg.ctrl_offset;

    /* If --in / --out (or in/out from config) are set, resolve them
       to port indices now.  Both are optional; if absent, we keep
       index 0 (first port). */
    if (cfg.in_addr[0] != '\0') {
        int idx = midi_device_find_in(&g_midi, cfg.in_addr);
        if (idx < 0) {
            fprintf(stderr, "sra: MIDI IN port '%s' not found\n",
                    cfg.in_addr);
            return 1;
        }
        g_midi.in_index = idx;
    }
    if (cfg.out_addr[0] != '\0') {
        int idx = midi_device_find_out(&g_midi, cfg.out_addr);
        if (idx < 0) {
            fprintf(stderr, "sra: MIDI OUT port '%s' not found\n",
                    cfg.out_addr);
            return 1;
        }
        g_midi.out_index = idx;
    }

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
                         12 * g_app.ctrl_offset,
                         g_app.chord_channel);
            g_app.status = 3;
        }
    }

    if (g_app.status != 3) return 0;

    /* ---- running phase ---- */
    /* Wait until Ctrl+C (or a future stop condition) sets running = 0.
       In a terminal, Enter also stops the engine for convenience. */
    while (g_engine.running) {
        fd_set fds;
        struct timeval tv;
        FD_ZERO(&fds);
        FD_SET(STDIN_FILENO, &fds);
        tv.tv_sec  = 0;
        tv.tv_usec = 100000;   /* 100 ms poll */
        if (select(STDIN_FILENO + 1, &fds, NULL, NULL, &tv) > 0) {
            int c = getchar();
            if (c == EOF || c == '\n') break;
        }
    }

    /* Request graceful shutdown.  The worker threads check this flag
       and exit on their own; they need h_in to stay valid until they
       do, so we close the ports only after the joins. */
    g_engine.running = 0;

    pthread_join(g_engine.midi_in_tid, NULL);
    pthread_join(g_engine.engine_tid,  NULL);
    midi_device_close(&g_midi);
    engine_destroy(&g_engine);

    restore_terminal();
    return 0;
}
