/* midi_device_linux.c — ALSA rawmidi port management */

#include "midi_device.h"

/* ------------------------------------------------------------------ */
/* Port enumeration                                                     */
/* ------------------------------------------------------------------ */

static void enumerate_ports(AlsaPort in_ports[],  int *in_count,
                             AlsaPort out_ports[], int *out_count) {
    int card = -1;
    *in_count = *out_count = 0;

    while (snd_card_next(&card) == 0 && card >= 0) {
        snd_ctl_t *ctl;
        char hw[16];
        int  dev = -1;

        snprintf(hw, sizeof(hw), "hw:%d", card);
        if (snd_ctl_open(&ctl, hw, 0) < 0) continue;

        while (snd_ctl_rawmidi_next_device(ctl, &dev) == 0 && dev >= 0) {
            snd_rawmidi_info_t *info;
            char port[32];

            snd_rawmidi_info_alloca(&info);
            snprintf(port, sizeof(port), "hw:%d,%d", card, dev);

            /* Check IN capability */
            if (*in_count < ALSA_MAX_PORTS) {
                snd_rawmidi_info_set_device(info, dev);
                snd_rawmidi_info_set_stream(info, SND_RAWMIDI_STREAM_INPUT);
                if (snd_ctl_rawmidi_info(ctl, info) == 0) {
                    AlsaPort *p = &in_ports[(*in_count)++];
                    snprintf(p->addr, sizeof(p->addr), "%s", port);
                    snprintf(p->name, sizeof(p->name), "%s",
                             snd_rawmidi_info_get_name(info));
                }
            }

            /* Check OUT capability */
            if (*out_count < ALSA_MAX_PORTS) {
                snd_rawmidi_info_set_device(info, dev);
                snd_rawmidi_info_set_stream(info, SND_RAWMIDI_STREAM_OUTPUT);
                if (snd_ctl_rawmidi_info(ctl, info) == 0) {
                    AlsaPort *p = &out_ports[(*out_count)++];
                    snprintf(p->addr, sizeof(p->addr), "%s", port);
                    snprintf(p->name, sizeof(p->name), "%s",
                             snd_rawmidi_info_get_name(info));
                }
            }
        }
        snd_ctl_close(ctl);
    }
}

/* ------------------------------------------------------------------ */
/* Public API                                                           */
/* ------------------------------------------------------------------ */

void midi_device_probe(MidiDevice *dev) {
    memset(dev, 0, sizeof(*dev));
    enumerate_ports(dev->in_ports,  &dev->in_count,
                    dev->out_ports, &dev->out_count);
    /* Default selection: index 0 */
    dev->in_index  = 0;
    dev->out_index = 0;
}

void midi_device_select_in(MidiDevice *dev, int delta) {
    int next = dev->in_index + delta;
    if (next >= 0 && next < dev->in_count)
        dev->in_index = next;
}

void midi_device_select_out(MidiDevice *dev, int delta) {
    int next = dev->out_index + delta;
    if (next >= 0 && next < dev->out_count)
        dev->out_index = next;
}

/* platform_ctx is unused on Linux (MIDI IN handled by a separate thread). */
int midi_device_open(MidiDevice *dev, void *platform_ctx) {
    const char *in_addr  = dev->in_ports [dev->in_index ].addr;
    const char *out_addr = dev->out_ports[dev->out_index].addr;

    (void)platform_ctx;

    if (snd_rawmidi_open(&dev->h_in, NULL, in_addr, 0) < 0)
        return 888;

    /* SND_RAWMIDI_SYNC ensures each write is flushed immediately. */
    if (snd_rawmidi_open(NULL, &dev->h_out, out_addr, SND_RAWMIDI_SYNC) < 0) {
        snd_rawmidi_close(dev->h_in);
        dev->h_in = NULL;
        return 999;
    }
    return 0;
}
