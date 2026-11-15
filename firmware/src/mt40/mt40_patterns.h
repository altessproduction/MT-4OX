/* SPDX-License-Identifier: GPL-3.0-only
 * MT-40X - Casio MT-40 clone firmware for the M-VAVE FM-1.
 * Patterns: generated tables (see data/mt40_patterns.json and tools/patterns_to_c.py).
 */
#ifndef MT40_PATTERNS_H
#define MT40_PATTERNS_H

#include "mt40_types.h"

/* All six MT-40 rhythms, in the MT-40 button order:
 * ROCK (Sleng Teng), POPS, SWING, SLOWROCK, SAMBA, WALTZ. */
extern const mt40_pattern_t pat_ROCK, pat_POPS, pat_SWING, pat_SLOWROCK, pat_SAMBA, pat_WALTZ;
extern const mt40_pattern_t *const *mt40_patterns;
extern const uint8_t mt40_npatterns;

/* The 22 timbre slots (draft values, calibrate by ear against a real MT-40). */
extern const mt40_timbre_t mt40_timbres[MT40_NTMB];

/* Chord shapes reused from the MT-40 / Felucca chord.c behaviour. */
extern const mt40_chord_shape_t mt40_shape_maj, mt40_shape_min, mt40_shape_dom7;

#endif /* MT40_PATTERNS_H */
