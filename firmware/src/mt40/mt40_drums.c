/* SPDX-License-Identifier: GPL-3.0-only
 * MT-40X - the four MT-40 drum lanes.
 *
 * Two sound paths, selectable per lane:
 *  - SAMPLE: a PCM buffer (flash-resident on the FM-1) rendered with linear
 *    resampling - use the free MusicRadar SampleRadar MT-40 pack or the
 *    Goldbaby pack as the source, see the dossier chapter 7;
 *  - SYNTH fallback: no samples needed to test the firmware (sine sweep kick,
 *    noise+tone snare, high-passed noise hats).
 *
 * Pure C99, float32 only, no libm: on the FM-1 the tones come from Felucca's
 * SINE table (dsp.c sine_i, Q32 phase, already built into the image) and the
 * whole bank is one static instance (no heap). The same code runs on the host
 * for the tests (math.h there).
 */
#include "mt40_drums.h"
#include "mt40_types.h"
#ifndef MT40_ON_DEVICE
#include <string.h>              /* the FM-1 builds get memset from bsp/chip/libc.c, included earlier */
#endif
#ifndef MT40_PI
#define MT40_PI 3.14159265358979f
#endif
#define MT40_FABS(x) ((x) < 0.0f ? -(x) : (x))

#ifdef MT40_ON_DEVICE
#define MT40_DRUM_BUF CTL        /* the render scratch: one CTL block at a time */
#else
#define MT40_DRUM_BUF 2048       /* the host tests may render longer blocks */
#endif

typedef struct {
    float pcm_pos;   /* sample-mode playback position (samples) */
    float level;     /* envelope follower of the sounding hit */
} mt40_lane_state_t;

struct mt40_drums_s {
    float sr;
    mt40_lane_state_t lane[MT40_NDRUMS];
    const float *pcm[MT40_NDRUMS];
    int          pcm_len[MT40_NDRUMS];
    float        gain[MT40_NDRUMS];
    float        buf[MT40_DRUM_BUF];   /* scratch for one lane render */
    uint32_t     noise;                /* xorshift state */
    uint32_t     phase_kick;           /* synth fallback oscillators (Q32) */
    uint32_t     phase_sn;
    float        hp_z;                 /* hat high-pass state */
};

static float frand(mt40_drums_t *d)
{
    d->noise ^= d->noise << 13;
    d->noise ^= d->noise >> 17;
    d->noise ^= d->noise << 5;
    return ((float)(int32_t)d->noise / 2147483648.0f);
}

#ifdef MT40_ON_DEVICE
/* Q32 phase -> -1..1, through the SINE table the image already carries. */
static float mt40_sin_q32(uint32_t ph)
{
    return (float)sine_i(ph) / 32768.0f;
}
#else
#include <math.h>
static float mt40_sin_q32(uint32_t ph)
{
    return sinf((float)ph / 4294967296.0f * 2.0f * MT40_PI);
}
#endif

#ifdef MT40_ON_DEVICE
/* The FM-1 has no heap: one static instance, constructed in place. */
static mt40_drums_t mt40_drums_instance;
mt40_drums_t *mt40_drums_new(float sr)
{
    mt40_drums_t *d = &mt40_drums_instance;
    memset(d, 0, sizeof(*d));
    d->sr = sr;
    d->noise = 0x1234567u;
    d->gain[MT40_KICK]   = 1.00f;
    d->gain[MT40_SNARE]  = 0.80f;
    d->gain[MT40_HAT_CL] = 0.45f;
    d->gain[MT40_HAT_OP] = 0.45f;
    return d;
}
void mt40_drums_free(mt40_drums_t *d) { (void)d; }
#else
#include <stdlib.h>
mt40_drums_t *mt40_drums_new(float sr)
{
    mt40_drums_t *d = (mt40_drums_t *)calloc(1, sizeof(mt40_drums_t));
    if (!d) return NULL;
    d->sr = sr;
    d->noise = 0x1234567u;
    d->gain[MT40_KICK]   = 1.00f;
    d->gain[MT40_SNARE]  = 0.80f;
    d->gain[MT40_HAT_CL] = 0.45f;
    d->gain[MT40_HAT_OP] = 0.45f;
    return d;
}
void mt40_drums_free(mt40_drums_t *d)
{
    free(d);
}
#endif

/* Attach a PCM sample to a lane (mono float32, native rate). */
void mt40_drums_set_sample(mt40_drums_t *d, int lane, const float *pcm, int len)
{
    if (lane < 0 || lane >= (int)MT40_NDRUMS) return;
    d->pcm[lane] = pcm;
    d->pcm_len[lane] = len;
}

void mt40_drums_hit(mt40_drums_t *d, int lane, float vel)
{
    if (lane < 0 || lane >= (int)MT40_NDRUMS) return;
    d->lane[lane].pcm_pos = 0.0f;
    d->lane[lane].level = vel * d->gain[lane];
}

/* Render all sounding lanes into out[n]. Returns the peak level seen. */
float mt40_drums_render(mt40_drums_t *d, float *out, int n)
{
    float peak = 0.0f;
    for (int li = 0; li < (int)MT40_NDRUMS; li++) {
        mt40_lane_state_t *L = &d->lane[li];
        if (L->level <= 0.0001f)
            continue;
        memset(d->buf, 0, sizeof(float) * (unsigned)n);
        int done = 0;

        if (d->pcm[li] && d->pcm_len[li] > 0) {
            /* sample mode: linear-interpolated playback */
            for (int i = 0; i < n; i++) {
                int p0 = (int)L->pcm_pos;
                if (p0 >= d->pcm_len[li] - 1) { done = 1; break; }
                float fr = L->pcm_pos - (float)p0;
                d->buf[i] += (d->pcm[li][p0] * (1.0f - fr) +
                              d->pcm[li][p0 + 1] * fr) * L->level;
                L->pcm_pos += 1.0f;
            }
        } else if (li == MT40_KICK) {
            /* synth fallback: descending sine sweep, exponential decay.
             * 2*pi*f/sr as a Q32 phase increment: (f / sr) * 2^32. */
            uint32_t inc = (uint32_t)((45.0f + 85.0f * L->level) / d->sr * 4294967296.0f);
            for (int i = 0; i < n; i++) {
                if (L->level <= 0.0001f) { done = 1; break; }
                d->phase_kick += inc;
                d->buf[i] += mt40_sin_q32(d->phase_kick) * L->level;
                L->level *= 0.99985f;                    /* about 200 ms decay */
            }
        } else if (li == MT40_SNARE) {
            /* synth fallback: 185 Hz tone + noise burst */
            uint32_t inc = (uint32_t)(185.0f / d->sr * 4294967296.0f);
            for (int i = 0; i < n; i++) {
                if (L->level <= 0.0001f) { done = 1; break; }
                d->phase_sn += inc;
                float tone = 0.4f * mt40_sin_q32(d->phase_sn);
                d->buf[i] += (tone + 0.8f * frand(d)) * L->level;
                L->level *= 0.99955f;                    /* about 150 ms decay */
            }
        } else {
            /* synth fallback: hats = first-order high-passed noise */
            for (int i = 0; i < n; i++) {
                if (L->level <= 0.0001f) { done = 1; break; }
                float w = frand(d);
                float hp = w - d->hp_z;
                d->hp_z = w;
                d->buf[i] += hp * L->level;
                L->level *= (li == MT40_HAT_CL) ? 0.9980f : 0.9994f; /* 40 / 250 ms */
            }
        }

        if (done)
            L->level = 0.0f;

        for (int i = 0; i < n; i++) {
            out[i] += d->buf[i];
            float a = MT40_FABS(d->buf[i]);
            if (a > peak) peak = a;
        }
    }
    return peak;
}
