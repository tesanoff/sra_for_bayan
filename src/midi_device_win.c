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
