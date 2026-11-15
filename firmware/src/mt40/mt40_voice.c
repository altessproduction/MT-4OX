/* SPDX-License-Identifier: GPL-3.0-only
 * MT-40X - pulse "stairwave" voices for the keyboard and the auto-bass.
 *
 * Nine voices like the MT-40 (8 melody + 1 bass). Each voice is a pulse wave
 * with a PolyBLEP anti-aliasing correction, the sustain effect (ADSR with a
 * long sustain like the MT-40 slider), a triangle LFO vibrato and a one-pole
 * low-pass "tone".
 *
 * Pure C99, float32 only: the FM-1's pi32v2 core has no libm and (fast) single
 * precision only, so this file avoids double math and math.h functions
 * entirely (mt40_hz uses a semitone ratio table, the tone coefficient the
 * classic 2*pi*fc/sr approximation). The same code runs on the host for the
 * tests, so what the tests hear is what the device plays.
 */
#include "mt40_voice.h"
#include "mt40_engine.h"

#ifndef MT40_PI
#define MT40_PI 3.14159265358979f
#endif
#define MT40_FABS(x) ((x) < 0.0f ? -(x) : (x))

typedef enum { MT40_V_OFF = 0, MT40_V_ATTACK, MT40_V_DECAY, MT40_V_SUSTAIN,
               MT40_V_RELEASE } mt40_vstate_t;

typedef struct {
    mt40_vstate_t st;
    uint8_t  note;          /* sounding MIDI note */
    uint8_t  gate;          /* key held */
    uint8_t  has_vib;       /* the timbre's vibrato is on (the LFO path costs float muls) */
    float    phase;         /* oscillator phase 0..1 */
    float    inc;           /* phase increment per sample */
    float    lfo_phase;
    float    env;           /* output units: 0..vol_eff (the timbre's vol x 0.5 folded in) */
    float    top, sus_eff;  /* the attack's end and the sustain level, output units */
    float    a_inc, d_eff, r_inc;   /* per-sample envelope steps, output units */
    float    lp;            /* one-pole low-pass state (the tone) */
    const mt40_timbre_t *tm;
} mt40_voice_t;

struct mt40_synth_s {
    float   sr;
    mt40_voice_t v[MT40_NVOICES];
    uint8_t active[MT40_NVOICES];
};

/* 2^(k/12), k = 0..11 (no libm on the FM-1: a table and octave shifts). */
static const float MT40_SEMI[12] = {
    1.000000000f, 1.059463094f, 1.122462048f, 1.189207115f, 1.259921050f,
    1.334839854f, 1.414213562f, 1.498307077f, 1.587401052f, 1.681792831f,
    1.781797436f, 1.887748609f
};

static float mt40_hz(int n)
{
    float hz = 440.0f;
    int r = n - 69;                         /* A4 */
    while (r >= 12) { hz *= 2.0f; r -= 12; }
    while (r <= -12) { hz *= 0.5f; r += 12; }
    hz *= r >= 0 ? MT40_SEMI[r] : 1.0f / MT40_SEMI[-r];
    return hz;
}

static void mt40_voice_start(mt40_voice_t *v, const mt40_timbre_t *tm, int note, float sr)
{
    float hz = mt40_hz(note + tm->oct * 12);
    /* the timbre's vol (and the render's x 0.5) folded into the envelope: the
     * per-sample loop then needs one float multiply less */
    float vol_eff = tm->vol * 0.5f;
    v->tm = tm;
    v->note = (uint8_t)note;
    v->gate = 1;
    v->st = MT40_V_ATTACK;
    v->has_vib = tm->vib_depth > 0.001f;
    v->env = 0.0f;
    v->phase = 0.0f;
    v->inc = hz / sr;
    v->lfo_phase = 0.0f;
    v->lp = 0.0f;
    v->top = vol_eff;
    v->sus_eff = tm->sustain * vol_eff;
    v->a_inc = vol_eff / (tm->attack_ms  * 0.001f * sr + 1.0f);
    v->d_eff = vol_eff * (1.0f - tm->sustain) / (tm->decay_ms * 0.001f * sr + 1.0f);
    v->r_inc = vol_eff / (tm->release_ms * 0.001f * sr + 1.0f);
}

/* PolyBLEP correction for the pulse discontinuities (anti-aliasing). */
static float poly_blep(float t, float dt)
{
    if (t < dt) {
        float x = t / dt;
        return x + x - x * x - 1.0f;
    } else if (t > 1.0f - dt) {
        float x = (t - 1.0f) / dt;
        return x * x + x + x + 1.0f;
    }
    return 0.0f;
}

/* Render one voice into out[n] (mono, -1..1, already scaled by timbre vol). */
static void voice_render(mt40_voice_t *v, float *out, int n, float sr)
{
    const mt40_timbre_t *tm = v->tm;
    if (v->st == MT40_V_OFF || !tm)
        return;

    /* one-pole low-pass coefficient (no libm: the classic 2*pi*fc/sr
     * approximation of 1 - exp(-2*pi*fc/sr), clamped) */
    float a = 2.0f * MT40_PI * tm->cutoff / sr;
    if (a > 0.95f) a = 0.95f;
    float lfo_inc = 2.0f * MT40_PI * tm->vib_hz / sr;

    for (int i = 0; i < n; i++) {
        /* vibrato LFO (triangle): only when the timbre has one (the common
         * case, vibrato off, keeps the loop at two float multiplies) */
        float vib = 1.0f;
        if (v->has_vib) {
            v->lfo_phase += lfo_inc;
            if (v->lfo_phase >= 2.0f * MT40_PI) v->lfo_phase -= 2.0f * MT40_PI;
            float tri = 2.0f * MT40_FABS(v->lfo_phase / MT40_PI - 1.0f) - 1.0f;
            vib = 1.0f + tri * tm->vib_depth * 0.03f;
            v->phase += vib * v->inc;
        } else
            v->phase += v->inc;
        if (v->phase >= 1.0f) v->phase -= 1.0f;

        /* pulse with PolyBLEP at both edges */
        float t = v->phase;
        float s = (t < tm->duty) ? 1.0f : -1.0f;
        s -= poly_blep(t, v->inc);
        if (t >= tm->duty) s += poly_blep(t - tm->duty, v->inc);

        /* envelope (output units, the vol folded in) */
        switch (v->st) {
        case MT40_V_ATTACK:
            v->env += v->a_inc;
            if (v->env >= v->top) { v->env = v->top; v->st = MT40_V_DECAY; }
            break;
        case MT40_V_DECAY:
            v->env -= v->d_eff;
            if (v->env <= v->sus_eff) { v->env = v->sus_eff; v->st = MT40_V_SUSTAIN; }
            break;
        case MT40_V_SUSTAIN:
            v->env = v->sus_eff;
            break;
        case MT40_V_RELEASE:
            v->env -= v->r_inc;
            if (v->env <= 0.0f) { v->env = 0.0f; v->st = MT40_V_OFF; }
            break;
        default:
            break;
        }

        /* tone: one-pole low-pass toward the pulse, then out (the vol is in env) */
        v->lp += a * (s - v->lp);
        out[i] += v->lp * v->env;
    }
}

/* Public API ------------------------------------------------------------ */

#ifdef MT40_ON_DEVICE
/* The FM-1 has no heap: one static instance, constructed in place. */
static mt40_synth_t mt40_synth_instance;
mt40_synth_t *mt40_synth_new(float sr)
{
    mt40_synth_t *s = &mt40_synth_instance;
    for (int k = 0; k < (int)MT40_NVOICES; k++) {
        s->active[k] = 0;
        s->v[k].st = MT40_V_OFF;
        s->v[k].tm = 0;
    }
    s->sr = sr;
    return s;
}
void mt40_synth_free(mt40_synth_t *s) { (void)s; }
#else
#include <stdlib.h>
mt40_synth_t *mt40_synth_new(float sr)
{
    mt40_synth_t *s = (mt40_synth_t *)calloc(1, sizeof(mt40_synth_t));
    if (s) s->sr = sr;
    return s;
}
void mt40_synth_free(mt40_synth_t *s)
{
    free(s);
}
#endif

void mt40_synth_all_off(mt40_synth_t *s)
{
    for (int k = 0; k < (int)MT40_NVOICES; k++) {
        s->active[k] = 0;
        s->v[k].st = MT40_V_OFF;
    }
}

/* Returns the voice index that took the note, or -1. */
int mt40_synth_note_on(mt40_synth_t *s, int voice_index, int note,
                       const mt40_timbre_t *tm)
{
    if (voice_index < 0 || voice_index >= (int)MT40_NVOICES || !tm)
        return -1;
    mt40_voice_start(&s->v[voice_index], tm, note, s->sr);
    s->active[voice_index] = 1;
    return voice_index;
}

void mt40_synth_note_off(mt40_synth_t *s, int voice_index)
{
    if (voice_index >= 0 && voice_index < (int)MT40_NVOICES &&
        s->v[voice_index].st != MT40_V_OFF) {
        s->v[voice_index].gate = 0;
        s->v[voice_index].st = MT40_V_RELEASE;
    }
}

/* Mix all voices into out[n]; sum is bounded by the caller (headroom). */
void mt40_synth_render(mt40_synth_t *s, float *out, int n)
{
    for (int k = 0; k < (int)MT40_NVOICES; k++) {
        if (s->active[k])
            voice_render(&s->v[k], out, n, s->sr);
    }
    for (int k = 0; k < (int)MT40_NVOICES; k++) {
        if (s->active[k] && s->v[k].st == MT40_V_OFF)
            s->active[k] = 0;
    }
}
