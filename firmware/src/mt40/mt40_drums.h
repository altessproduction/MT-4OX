/* SPDX-License-Identifier: GPL-3.0-only
 * MT-40X - the four MT-40 drum lanes (public API).
 */
#ifndef MT40_DRUMS_H
#define MT40_DRUMS_H

#include "mt40_types.h"

typedef struct mt40_drums_s mt40_drums_t;

mt40_drums_t *mt40_drums_new(float sr);
void          mt40_drums_free(mt40_drums_t *d);
void          mt40_drums_set_sample(mt40_drums_t *d, int lane, const float *pcm, int len);
void          mt40_drums_hit(mt40_drums_t *d, int lane, float vel);
float         mt40_drums_render(mt40_drums_t *d, float *out, int n);

#endif /* MT40_DRUMS_H */
