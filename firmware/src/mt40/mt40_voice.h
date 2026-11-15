/* SPDX-License-Identifier: GPL-3.0-only
 * MT-40X - pulse "stairwave" voices (public API).
 */
#ifndef MT40_VOICE_H
#define MT40_VOICE_H

#include "mt40_types.h"

typedef struct mt40_synth_s mt40_synth_t;

mt40_synth_t *mt40_synth_new(float sr);
void          mt40_synth_free(mt40_synth_t *s);
void          mt40_synth_all_off(mt40_synth_t *s);
int           mt40_synth_note_on(mt40_synth_t *s, int voice_index, int note,
                                 const mt40_timbre_t *tm);
void          mt40_synth_note_off(mt40_synth_t *s, int voice_index);
void          mt40_synth_render(mt40_synth_t *s, float *out, int n);

#endif /* MT40_VOICE_H */
