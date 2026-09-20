/* midi_device_win.c — Windows winmm MIDI device management */

#include "midi_device.h"

/* ------------------------------------------------------------------ */
/* Forward declarations for SysEx state (definitions at end of file)   */
/* ------------------------------------------------------------------ */

#define SRA_SYSEX_BUF 256

static char     sysex_buf[SRA_SYSEX_BUF];
static MIDIHDR  sysex_hdr;
static int      sysex_prepared;
static int      sysex_queued;

static void (*sysex_cb)(const unsigned char *data, int len);

static void sysex_prepare(HMIDIIN h);

void midi_device_probe(MidiDevice *dev) {
    dev->in_count  = midiInGetNumDevs();
    dev->out_count = midiOutGetNumDevs();
    dev->in_index  = 0;
    dev->out_index = 0;
    if (dev->in_count  > 0)
        midiInGetDevCaps(0, &dev->in_caps,  sizeof(dev->in_caps));
    if (dev->out_count > 0)
        midiOutGetDevCaps(0, &dev->out_caps, sizeof(dev->out_caps));
}

void midi_device_select_in(MidiDevice *dev, int delta) {
    int next = dev->in_index + delta;
    if (next >= 0 && next < dev->in_count) {
        dev->in_index = next;
        midiInGetDevCaps(dev->in_index, &dev->in_caps, sizeof(dev->in_caps));
    }
}

void midi_device_select_out(MidiDevice *dev, int delta) {
    int next = dev->out_index + delta;
    if (next >= 0 && next < dev->out_count) {
        dev->out_index = next;
        midiOutGetDevCaps(dev->out_index, &dev->out_caps, sizeof(dev->out_caps));
    }
}

/* Windows has no rawmidi addresses; search by device name (szPname). */
int midi_device_find_in(MidiDevice *dev, const char *addr) {
    int i;
    MIDIINCAPS caps;
    if (!addr || !*addr) return -1;
    for (i = 0; i < dev->in_count; i++) {
        if (midiInGetDevCaps(i, &caps, sizeof(caps)) == MMSYSERR_NOERROR &&
            strcmp(caps.szPname, addr) == 0)
            return i;
    }
    return -1;
}

int midi_device_find_out(MidiDevice *dev, const char *addr) {
    int i;
    MIDIOUTCAPS caps;
    if (!addr || !*addr) return -1;
    for (i = 0; i < dev->out_count; i++) {
        if (midiOutGetDevCaps(i, &caps, sizeof(caps)) == MMSYSERR_NOERROR &&
            strcmp(caps.szPname, addr) == 0)
            return i;
    }
    return -1;
}

/* platform_ctx must be an HWND — the window that receives MM_MIM_DATA
   (and, when SysEx input is supported, MM_MIM_LONGDATA). */
int midi_device_open(MidiDevice *dev, void *platform_ctx) {
    HWND hwnd = (HWND)platform_ctx;
    if (midiInOpen(&dev->h_in, dev->in_index,
                   (DWORD_PTR)hwnd, 0, CALLBACK_WINDOW))
        return 888;
    if (midiOutOpen(&dev->h_out, dev->out_index, 0, 0, CALLBACK_NULL))
        return 999;

    /* Prepare the SysEx buffer *before* midiInStart, so incoming
       long messages are captured from the very first byte. */
    sysex_prepare(dev->h_in);

    midiInStart(dev->h_in);
    return 0;
}

void midi_device_close(MidiDevice *dev) {
    if (!dev) return;
    if (dev->h_in) {
        midiInStop(dev->h_in);
        if (sysex_prepared) {
            midiInUnprepareHeader(dev->h_in, &sysex_hdr, sizeof(sysex_hdr));
            sysex_prepared = 0;
            sysex_queued   = 0;
        }
        midiInClose(dev->h_in);
        dev->h_in = NULL;
    }
    if (dev->h_out) {
        midiOutClose(dev->h_out);
        dev->h_out = NULL;
    }
}

/* ------------------------------------------------------------------ */
/* SysEx input (Windows only)                                          */
/* ------------------------------------------------------------------ */

void midi_device_win_set_sysex_cb(void (*cb)(const unsigned char *data,
                                             int len)) {
    sysex_cb = cb;
}

static void sysex_prepare(HMIDIIN h) {
    if (sysex_prepared) return;
    memset(&sysex_hdr, 0, sizeof(sysex_hdr));
    sysex_hdr.lpData         = sysex_buf;
    sysex_hdr.dwBufferLength = SRA_SYSEX_BUF;
    if (midiInPrepareHeader(h, &sysex_hdr, sizeof(sysex_hdr))
            != MMSYSERR_NOERROR)
        return;
    if (midiInAddBuffer(h, &sysex_hdr, sizeof(sysex_hdr))
            != MMSYSERR_NOERROR) {
        midiInUnprepareHeader(h, &sysex_hdr, sizeof(sysex_hdr));
        return;
    }
    sysex_prepared = 1;
    sysex_queued   = 1;
}

void midi_device_win_handle_longdata(MidiDevice *dev, LONG lParam) {
    MIDIHDR *hdr = (MIDIHDR *)lParam;
    if (!hdr || !dev || !dev->h_in) return;

    if (hdr->dwBytesRecorded > 0 && sysex_cb)
        sysex_cb((const unsigned char *)hdr->lpData,
                 (int)hdr->dwBytesRecorded);

    /* Re-queue the same buffer for the next SysEx message. */
    if (hdr->dwFlags & MHDR_DONE || hdr->dwFlags & MHDR_PREPARED) {
        midiInAddBuffer(dev->h_in, hdr, sizeof(MIDIHDR));
        sysex_queued = 1;
    } else {
        sysex_queued = 0;
    }
}
