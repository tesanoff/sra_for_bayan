/* sra_ui_win.c — Win32 GDI user interface */

#include "sra_ui.h"

static AppUI *g_ui = NULL; /* for ui_refresh from any context */

void ui_init(AppUI *ui, HWND hwnd) {
    ui->hwnd = hwnd;
    g_ui     = ui;
}

void ui_refresh(AppUI *ui) {
    InvalidateRect(ui->hwnd, NULL, TRUE);
    UpdateWindow(ui->hwnd);
}

/* ------------------------------------------------------------------ */
/* Painting helpers                                                     */
/* ------------------------------------------------------------------ */

static void paint_str(AppUI *ui, HDC hdc, int x, int y,
                      const char *s) {
    strncpy(ui->text_buf, s, sizeof(ui->text_buf) - 1);
    ui->text_buf[sizeof(ui->text_buf) - 1] = '\0';
    TextOut(hdc, x, y, ui->text_buf, lstrlen(ui->text_buf));
}

static void paint_int(AppUI *ui, HDC hdc, int x, int y, int value) {
    sprintf(ui->text_buf, "%d", value);
    TextOut(hdc, x, y, ui->text_buf, lstrlen(ui->text_buf));
}

static void paint_setup_rows(AppUI *ui, MidiDevice *dev, AppState *app,
                              HDC hdc) {
    TextOut(hdc, 10, 10, "[Q][Z] MIDI IN:", 15);
    paint_str(ui, hdc, 160, 10, dev->in_caps.szPname);

    TextOut(hdc, 10, 30, "[E][C] MIDI OUT:", 16);
    paint_str(ui, hdc, 160, 30, dev->out_caps.szPname);

    TextOut(hdc, 10, 50, "[W][X] Channel:", 15);
    paint_int(ui, hdc, 160, 50, app->melody_channel + 1);

    TextOut(hdc, 10, 70, "[O] Ctrl Offset:", 16);
    paint_int(ui, hdc, 160, 70, app->ctrl_offset);
}

void ui_paint(AppUI *ui, MidiDevice *dev, AppState *app,
              HDC hdc, SraCore *sra) {
    if (app->status == 1) {
        paint_setup_rows(ui, dev, app, hdc);
        TextOut(hdc, 10, 90, "[S] START", 9);
    } else if (app->status >= 2) {
        paint_setup_rows(ui, dev, app, hdc);

        TextOut(hdc, 10, 90, "Tempo:", 6);
        paint_int(ui, hdc, 160, 90, (int)sracore_tempo(sra));

        TextOut(hdc, 10, 110, "Chord:", 6);
        paint_str(ui, hdc, 160, 110, sracore_chord_name(sra));
    }
}

/* ------------------------------------------------------------------ */
/* Keyboard handling (setup phase)                                      */
/* ------------------------------------------------------------------ */

int ui_on_keydown(AppUI *ui, MidiDevice *dev, unsigned int vkey,
                  AppState *app) {
    int start = 0;

    switch (vkey) {
    case 'Z': midi_device_select_in (dev, -1); break;
    case 'Q': midi_device_select_in (dev, +1); break;
    case 'C': midi_device_select_out(dev, -1); break;
    case 'E': midi_device_select_out(dev, +1); break;

    case 'X': if (app->melody_channel > 0)  app->melody_channel--; break;
    case 'W': if (app->melody_channel < 15) app->melody_channel++; break;

    case 'O':
        app->ctrl_offset++;
        if (app->ctrl_offset == 2) app->ctrl_offset = -1;
        break;

    case 'S': start = 1; break;
    }

    if (!start) ui_refresh(ui);
    return start;
}
