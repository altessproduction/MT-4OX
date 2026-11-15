/* SPDX-License-Identifier: GPL-3.0-only
 * MT-40X - the MT-40 auto-accompaniment engine.
 *
 * Reproduces the mechanics of the MT-40 accompaniment section:
 *  - a pattern of 2 bars at 1/16 resolution (drums + bass + chord stabs),
 *  - the bass motif transposed by the root of the held chord (offsets are
 *    relative, exactly like the MT-40 mini-bass-keyboard behaviour),
 *  - a drum fill button (FILL) that takes over the end of the current bar,
 *  - variation A/B, synchro start, swing (MPC-style alternating boundaries).
 *
 * Pure C99, no hardware dependency: it runs in the audio ISR on the FM-1
 * (called once per CTL block, like Felucca's seq.c) and on the host for tests.
 */
#include "mt40_engine.h"
#include "mt40_patterns.h"

#ifndef MT40_ON_DEVICE
#include <string.h>              /* the FM-1 builds get memset from bsp/chip/libc.c, included earlier */
#endif

/* Q20.12 fixed point: one step = MT40_PHASE_ONE */
#define MT40_PHASE_FRAC 12
#define MT40_PHASE_ONE  (1u << MT40_PHASE_FRAC)

void mt40_engine_init(mt40_engine_t *e, const mt40_pattern_t *pat, float bpm,
                      float sample_rate)
{
    memset(e, 0, sizeof(*e));
    e->pat = pat;
    e->bpm = bpm;
    e->sr = sample_rate;
    e->root_midi = 38;              /* D2: the Sleng Teng reference root */
    e->chord_shape = &mt40_shape_maj;
    e->swing = 0.5f;                /* 0.5 = straight, 0.75 = max MPC swing */
    mt40_engine_update_step_len(e);
}

void mt40_engine_update_step_len(mt40_engine_t *e)
{
    /* seconds per 1/16 step at e->bpm: 60 / bpm / 4 */
    e->step_samples = (e->sr * 60.0f) / (e->bpm * 4.0f);
    e->boundary_samples = e->step_samples;
}

void mt40_engine_set_chord(mt40_engine_t *e, int root_midi,
                           const mt40_chord_shape_t *shape)
{
    if (root_midi >= 0 && root_midi <= 127)
        e->root_midi = (uint8_t)root_midi;
    if (shape)
        e->chord_shape = shape;
}

void mt40_engine_start(mt40_engine_t *e)          /* synchro start: armed */
{
    /* Pre-roll so the very first tick fires step 0 (the pattern starts on the
     * downbeat, like the MT-40 when the first chord key triggers the start). */
    e->running = 1;
    if (e->pat) {
        uint8_t len = (uint8_t)(e->pat->steps_per_bar * e->pat->bars);
        e->step = (uint16_t)(len - 1);
        e->phase = e->boundary_samples;
    }
}

void mt40_engine_stop(mt40_engine_t *e)
{
    e->running = 0;
    e->step = 0;
    e->phase = 0;
    e->fill_active = 0;
}

void mt40_engine_set_bpm(mt40_engine_t *e, float bpm)
{
    if (bpm >= 30.0f && bpm <= 300.0f) {
        e->bpm = bpm;
        mt40_engine_update_step_len(e);
    }
}

void mt40_engine_set_swing(mt40_engine_t *e, float swing)   /* 0.5..0.75 */
{
    if (swing >= 0.5f && swing <= 0.75f)
        e->swing = swing;
}

void mt40_engine_set_variation(mt40_engine_t *e, int var_b)
{
    e->var_b = (uint8_t)(var_b ? 1 : 0);
}

/* FILL: take over the drums until the next downbeat (step 0 of a bar). */
void mt40_engine_trigger_fill(mt40_engine_t *e)
{
    e->fill_pending = 1;
}

static uint32_t drum_mask(const mt40_pattern_t *p, int lane, int step, int fill,
                          int var_b)
{
    uint32_t m = p->drum_mask[lane];
    if (var_b && lane == MT40_HAT_OP)
        m |= p->varb_hatop_mask;
    if (fill) {
        /* the fill replaces the base drums on the end of the bar */
        m = 0;
        if (lane == MT40_SNARE) m = p->fill_snare_mask;
        if (lane == MT40_HAT_OP) m |= p->fill_hatop_mask;
    }
    return (step < 32) ? ((m >> step) & 1u) : 0u;
}

static void fire_step(mt40_engine_t *e, int step)
{
    const mt40_pattern_t *p = e->pat;
    int fill = e->fill_active;
    int i;
    /* the riddim bank: a two-chord pattern moves the chord root bar by bar */
    const uint8_t bar = (uint8_t)(step / (p->steps_per_bar ? p->steps_per_bar : 16u));
    const int sh = (p->shift) ? p->shift[bar] : 0;

    /* drums */
    for (i = 0; i < (int)MT40_NDRUMS; i++) {
        if (drum_mask(p, i, step, fill, e->var_b))
            e->on_drum(e->user, i, 1.0f);
    }

    /* bass: chord root + pattern offset (the MT-40 transposition behaviour) */
    for (i = 0; i < p->nbass; i++) {
        const mt40_bass_t *b = &p->bass[i];
        if (b->step == step) {
            int note = (int)e->root_midi + b->off + sh;
            if (note < 0) note = 0;
            if (note > 127) note = 127;
            e->on_bass(e->user, note, b->len);
        }
    }

    /* chord stabs: the held chord, re-attacked on the pattern's rhythm */
    for (i = 0; i < p->nchords; i++) {
        const mt40_chord_t *c = &p->chords[i];
        if (c->step == step && e->chord_shape)
            e->on_chord(e->user, (int)e->root_midi + sh, e->chord_shape->iv,
                        e->chord_shape->n);
    }
}

/* One engine tick: advance by ctl_samples and fire the steps crossed.
 * Swing shifts the odd 1/16s later (MPC style): the even->odd boundary keeps
 * the base length, the odd->even boundary takes (2r - 1) extra share, so
 * swing = 0.5 is straight and 0.75 doubles the second half of each 8th. */
void mt40_engine_tick(mt40_engine_t *e, int ctl_samples)
{
    if (!e->running || !e->pat)
        return;

    e->phase += (float)ctl_samples;

    while (e->phase >= e->boundary_samples) {
        e->phase -= e->boundary_samples;

        /* advance one step */
        e->step++;
        const uint8_t spb = e->pat->steps_per_bar;
        const uint8_t len = (uint8_t)(spb * e->pat->bars);
        if (e->step >= len)
            e->step = 0;

        /* fill ends on the downbeat of a bar */
        if (e->fill_active && e->step == 0)
            e->fill_active = 0;
        if (e->fill_pending) {
            /* a fill pressed mid-pattern waits until the transport reaches the
             * first fill-covered step (fill masks are pattern-global, like the
             * drum masks: bits 28..31 = the end of the second bar) */
            uint32_t fmask = e->pat->fill_snare_mask | e->pat->fill_hatop_mask;
            uint32_t here = (e->step < 32) ? ((fmask >> e->step) & 1u) : 0u;
            if (here) {
                e->fill_pending = 0;
                e->fill_active = 1;
            }
        }

        /* next boundary length: MPC-style swing delays every odd 1/16.
         * even step duration = 2L*r, odd step duration = 2L*(1-r), r = swing.
         * swing = 0.5 is straight (both L), 0.75 is the maximum push. */
        {
            float d = (e->step & 1) ? 2.0f * (1.0f - e->swing)
                                    : 2.0f * e->swing;
            e->boundary_samples = e->step_samples * d;
        }

        fire_step(e, e->step);
    }
}
