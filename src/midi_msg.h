#ifndef MIDI_MSG_H
#define MIDI_MSG_H

#include "sra_engine.h"
#include "libsracore/sracore.h"

/* Forward one decoded MIDI message into libsracore.
   status: raw MIDI status byte (e.g. 0x90 for Note-On).
   d1, d2: data bytes (d2 is ignored for single-data messages). */
void midi_in_process(SraEngine *eng,
                     SRABYTE status, SRABYTE d1, SRABYTE d2);

#endif /* MIDI_MSG_H */
