#ifndef SRA_UI_H
#define SRA_UI_H

#include "sra_types.h"
#include "midi_device.h"
#include "libsracore/sracore.h"

/* ------------------------------------------------------------------ */
/* App-level state (shared by both platforms)                           */
/* ------------------------------------------------------------------ */

typedef struct {
    int status;          /* 0=no device  1=setup  3=running */
    int melody_channel;  /* 0-15, displayed as 1-16         */
    int chord_channel;   /* 0-15, displayed as 1-16         */
    int ctrl_offset;     /* -1, 0, or +1                    */
} AppState;

/* ------------------------------------------------------------------ */
/* UI context (platform-specific fields)                                */
/* ------------------------------------------------------------------ */

#ifdef _WIN32
typedef struct {
    HWND hwnd;
    char text_buf[80];
} AppUI;
#else
typedef struct {
    char text_buf[80];
} AppUI;
#endif

/* ------------------------------------------------------------------ */
/* Common API                                                           */
/* ------------------------------------------------------------------ */

/* Handle a keydown event during the setup phase (status == 1).
   key: VK code on Windows, raw char value on Linux.
   Returns 1 if the START key ('S') was pressed. */
int ui_on_keydown(AppUI *ui, MidiDevice *dev, unsigned int key,
                  AppState *app);

/* ------------------------------------------------------------------ */
/* Platform-specific API                                                */
/* ------------------------------------------------------------------ */

#ifdef _WIN32

/* Must be called once with the main window handle. */
void ui_init(AppUI *ui, HWND hwnd);

/* Paint the client area (called from WM_PAINT). */
void ui_paint(AppUI *ui, MidiDevice *dev, AppState *app,
              HDC hdc, SraCore *sra);

/* Force a window repaint (called by chord/tempo callbacks). */
void ui_refresh(AppUI *ui);

#else  /* Linux */

void ui_init(AppUI *ui);

/* Print current setup state to stdout (clears screen). */
void ui_print_state(AppUI *ui, MidiDevice *dev, AppState *app);

/* Reprint setup (called after each keypress). */
void ui_refresh(AppUI *ui);

/* Called by engine callbacks to display chord / tempo updates. */
void ui_notify_chord(const char *chord_name, int bpm);

#endif /* _WIN32 */

#endif /* SRA_UI_H */
