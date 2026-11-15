/* SPDX-License-Identifier: GPL-3.0-only
 * MT-40X - the MT-40 auto-accompaniment engine (public API).
 */
#ifndef MT40_ENGINE_H
#define MT40_ENGINE_H

#include "mt40_types.h"

typedef struct {
    const mt40_pattern_t *pat;      /* the 6 rhythms (mt40_patterns.h) */
    float   bpm;                    /* 60..160 typical, knob-driven */
    float   sr;                     /* 44117.647f on the FM-1 (24 MHz / 544) */
    float   step_samples;           /* base length of one 1/16 step */
    float   boundary_samples;       /* current boundary (swing-adjusted) */
    float   phase;                  /* samples accumulated in the current step */
    uint16_t step;                  /* current 1/16 step in the pattern */
    uint8_t running;                /* transport (synchro start arms it) */
    uint8_t fill_active;            /* FILL takes over the drums */
    uint8_t fill_pending;           /* FILL pressed: starts at the next step */
    uint8_t var_b;                  /* variation A/B */
    float   swing;                  /* 0.5 straight .. 0.75 */
    uint8_t root_midi;              /* chord root (from Felucca chord.c) */
    const mt40_chord_shape_t *chord_shape;
    /* event sinks (on the FM-1: mt40_drums / mt40_voice / chord voices) */
    mt40_drum_fn  on_drum;
    mt40_bass_fn  on_bass;
    mt40_chord_fn on_chord;
    void    *user;
} mt40_engine_t;

void mt40_engine_init(mt40_engine_t *e, const mt40_pattern_t *pat, float bpm,
                      float sample_rate);
void mt40_engine_tick(mt40_engine_t *e, int ctl_samples);
void mt40_engine_start(mt40_engine_t *e);
void mt40_engine_stop(mt40_engine_t *e);
void mt40_engine_set_bpm(mt40_engine_t *e, float bpm);
void mt40_engine_set_swing(mt40_engine_t *e, float swing);   /* 0.5..0.75 */
void mt40_engine_set_variation(mt40_engine_t *e, int var_b);
void mt40_engine_trigger_fill(mt40_engine_t *e);
void mt40_engine_set_chord(mt40_engine_t *e, int root_midi,
                           const mt40_chord_shape_t *shape);
void mt40_engine_update_step_len(mt40_engine_t *e);

#endif /* MT40_ENGINE_H */
