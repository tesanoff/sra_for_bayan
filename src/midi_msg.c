/* midi_msg.c — thin cross-platform bridge: raw MIDI bytes → libsracore */

#include "midi_msg.h"

void midi_in_process(SraEngine *eng,
                     SRABYTE status, SRABYTE d1, SRABYTE d2) {
    sracore_midi_in(eng->sra, status, d1, d2);
}
