#ifndef MIDI_DEVICE_H
#define MIDI_DEVICE_H

#include "sra_types.h"

/* ------------------------------------------------------------------ */
/* Platform-specific device handle / info                               */
/* ------------------------------------------------------------------ */

#ifdef _WIN32

typedef struct {
    int         in_index,  out_index;
    int         in_count,  out_count;
    HMIDIIN     h_in;
    HMIDIOUT    h_out;
    MIDIINCAPS  in_caps;
    MIDIOUTCAPS out_caps;
} MidiDevice;

#else  /* Linux / ALSA */

#define ALSA_MAX_PORTS 16

typedef struct {
    char addr[64];   /* ALSA port address, e.g. "hw:1,0" */
    char name[64];   /* human-readable port name          */
} AlsaPort;

typedef struct {
    int        in_index,  out_index;
    int        in_count,  out_count;
    AlsaPort   in_ports [ALSA_MAX_PORTS];
    AlsaPort   out_ports[ALSA_MAX_PORTS];
    snd_rawmidi_t *h_in;
    snd_rawmidi_t *h_out;
} MidiDevice;

#endif /* _WIN32 */

/* ------------------------------------------------------------------ */
/* Common API                                                           */
/* ------------------------------------------------------------------ */

/* Detect available MIDI ports and fill device counts / names.         */
void midi_device_probe(MidiDevice *dev);

/* Shift the selected IN/OUT device index by delta (+1 or -1).         */
void midi_device_select_in (MidiDevice *dev, int delta);
void midi_device_select_out(MidiDevice *dev, int delta);

/* Find an IN/OUT port by its address (e.g. "hw:5,0").
   Returns the index, or -1 if not found.
   On Windows the address is the device name (szPname). */
int  midi_device_find_in (MidiDevice *dev, const char *addr);
int  midi_device_find_out(MidiDevice *dev, const char *addr);

/* Open the selected devices.
   platform_ctx:  HWND on Windows (window receives MM_MIM_DATA),
                  NULL on Linux (ALSA input handled by a thread).
   Returns 0 on success, 888 if IN open fails, 999 if OUT open fails. */
int midi_device_open(MidiDevice *dev, void *platform_ctx);

/* Close the previously opened MIDI ports.  Safe to call if they were
   never opened (does nothing). */
void midi_device_close(MidiDevice *dev);

#endif /* MIDI_DEVICE_H */
