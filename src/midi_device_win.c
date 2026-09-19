/* midi_device_win.c — Windows winmm MIDI device management */

#include "midi_device.h"

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
    midiInStart(dev->h_in);
    return 0;
}

void midi_device_close(MidiDevice *dev) {
    if (!dev) return;
    if (dev->h_in)  { midiInStop(dev->h_in);  midiInClose(dev->h_in);  dev->h_in  = NULL; }
    if (dev->h_out) { midiOutClose(dev->h_out);                          dev->h_out = NULL; }
}
