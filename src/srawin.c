/* srawin.c — Windows WinMain / WndProc (thin assembly layer)
   Compile: see SRA/src/Makefile */

#include "sra_types.h"
#include "midi_device.h"
#include "midi_msg.h"
#include "sra_ui.h"
#include "sra_engine.h"
#include "sra_config.h"

/* ---- module-level singletons ---- */

static MidiDevice  g_midi;
static AppUI       g_ui;
static AppState    g_app;
static SraEngine   g_engine;
static SraConfig   g_cfg;

static const char *const APP_CLASS = "MyWndClass";
static const char *const APP_TITLE =
    "Software-based Real-time Arranger Version 4.06 by ZZ-Denis @ NazoMusic";

/* ---- helpers ---- */

static int center_pos(UINT window_size, UINT screen_size) {
    return (int)((screen_size / 2) - (window_size / 2));
}

/* ---- SysEx callback (called from midi_device_win.c) ---- */

static void on_sysex(const unsigned char *data, int len) {
    EnterCriticalSection(&g_engine.cs);
    sracore_sysex_in(g_engine.sra, (const SRABYTE *)data, len);
    LeaveCriticalSection(&g_engine.cs);
}

/* ---- window procedure ---- */

static long __stdcall WndProc(HWND hwnd, unsigned int wmsg,
                               unsigned int wParam, long lParam) {
    HDC         hdc;
    PAINTSTRUCT ps;
    int         err;
    SRABYTE     status, d1, d2;

    switch (wmsg) {

    case MM_MIM_DATA:
        /* Decode the Windows MIDI message pack before forwarding. */
        status = LOBYTE(LOWORD(lParam));
        d1     = HIBYTE(LOWORD(lParam));
        d2     = LOBYTE(HIWORD(lParam));
        EnterCriticalSection(&g_engine.cs);
        midi_in_process(&g_engine, status, d1, d2);
        LeaveCriticalSection(&g_engine.cs);
        return 0;

    case MM_MIM_LONGDATA:
        midi_device_win_handle_longdata(&g_midi, (LONG)lParam);
        return 0;

    case WM_KEYDOWN:
        if (g_app.status == 1) {
            if (ui_on_keydown(&g_ui, &g_midi, wParam, &g_app)) {
                /* 'S' pressed: open devices and start the engine. */
                midi_device_win_set_sysex_cb(on_sysex);
                err = midi_device_open(&g_midi, (void *)hwnd);
                if (err) {
                    char msg[32];
                    sprintf(msg, "MIDI open error %d", err);
                    MessageBox(0, msg, "SRA", MB_OK | MB_ICONERROR);
                    exit(err);
                }
                engine_start(&g_engine,
                             12 * g_app.ctrl_offset,
                             g_app.chord_channel,
                             0 /* interactive: UI callbacks enabled */);
                g_app.status = 3;
                ui_refresh(&g_ui);
            }
        }
        return 0;

    case WM_CREATE: {
        CREATESTRUCT *cs  = (CREATESTRUCT *)lParam;
        SraConfig    *cfg = (SraConfig *)cs->lpCreateParams;

        midi_device_probe(&g_midi);
        engine_init(&g_engine, &g_midi, (void *)hwnd);
        ui_init(&g_ui, hwnd);

        if (cfg) {
            g_app.chord_channel = cfg->chord_ch - 1;   /* config is 1-based */
            g_app.ctrl_offset   = cfg->ctrl_offset;
        }

        if (g_midi.in_count > 0 && g_midi.out_count > 0)
            g_app.status = 1;
        return 0;
    }

    case WM_PAINT:
        hdc = BeginPaint(hwnd, &ps);
        ui_paint(&g_ui, &g_midi, &g_app, hdc, g_engine.sra);
        EndPaint(hwnd, &ps);
        return 0;

    case WM_CLOSE:
        DestroyWindow(hwnd);
        return 0;

    case WM_DESTROY:
        /* Stop the engine and release MIDI, then quit. */
        g_engine.running = 0;
        if (g_engine.engine_thread)
            WaitForSingleObject(g_engine.engine_thread, 5000);
        midi_device_close(&g_midi);
        engine_destroy(&g_engine);
        PostQuitMessage(0);
        return 0;

    default:
        return DefWindowProc(hwnd, wmsg, wParam, lParam);
    }
}

/* ---- entry point ---- */

int WINAPI WinMain(HINSTANCE hInst, HINSTANCE hPrev,
                   LPSTR lpCmd, int nShow) {
    WNDCLASSEX wc;
    HWND       hwnd;
    MSG        msg;
    const int  wnd_W = 508;
    const int  wnd_H = 178;

    (void)hPrev; (void)lpCmd;

    /* Parse command line.  MinGW provides __argc / __argv. */
    sra_config_defaults(&g_cfg);
    if (sra_config_parse_args(&g_cfg, __argc, __argv) != 0) {
        MessageBox(NULL, "Invalid command line.\n"
                         "Try --help for usage.",
                   "SRA", MB_OK | MB_ICONERROR);
        return 1;
    }

    if (g_cfg.show_help) {
        MessageBox(NULL,
            "sra — Software-based Real-time Arranger\n"
            "\n"
            "Options:\n"
            "  --config PATH       config file path\n"
            "  --chord-ch N        chord channel (1-16)\n"
            "  --ctrl-offset N     command zone shift: -1, 0, +1\n"
            "  --help              show this help\n"
            "  --version           show version\n"
            "\n"
            "(Daemon mode is not supported on Windows.)",
            "SRA Help", MB_OK | MB_ICONINFORMATION);
        return 0;
    }

    if (g_cfg.show_version) {
        MessageBox(NULL, "sra 4.06 (libsracore 1.3)",
                   "SRA Version", MB_OK | MB_ICONINFORMATION);
        return 0;
    }

    if (sra_config_load(&g_cfg) != 0) {
        MessageBox(NULL, "Invalid config file. See console for details.",
                   "SRA", MB_OK | MB_ICONERROR);
        return 1;
    }

    wc.cbSize        = sizeof(WNDCLASSEX);
    wc.style         = 0;
    wc.lpfnWndProc   = WndProc;
    wc.cbClsExtra    = 0;
    wc.cbWndExtra    = 0;
    wc.hInstance     = hInst;
    wc.hIcon         = LoadIcon(NULL, IDI_APPLICATION);
    wc.hCursor       = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.lpszMenuName  = NULL;
    wc.lpszClassName = APP_CLASS;
    wc.hIconSm       = LoadIcon(NULL, IDI_APPLICATION);

    if (!RegisterClassEx(&wc)) {
        MessageBox(NULL, "Cannot register window class!", "Error!",
                   MB_ICONEXCLAMATION | MB_OK);
        return 0;
    }

    hwnd = CreateWindowEx(
        WS_EX_CLIENTEDGE, APP_CLASS, APP_TITLE,
        WS_OVERLAPPEDWINDOW,
        center_pos(wnd_W, GetSystemMetrics(SM_CXSCREEN)),
        center_pos(wnd_H, GetSystemMetrics(SM_CYSCREEN)),
        wnd_W, wnd_H,
        NULL, NULL, hInst, &g_cfg);

    if (!hwnd) {
        MessageBox(NULL, "Cannot create window!", "Error!",
                   MB_ICONEXCLAMATION | MB_OK);
        return 0;
    }

    ShowWindow(hwnd, nShow);
    UpdateWindow(hwnd);

    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    DeleteCriticalSection(&g_engine.cs);
    return (int)msg.wParam;
}
