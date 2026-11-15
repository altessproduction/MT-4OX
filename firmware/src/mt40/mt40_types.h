/* SPDX-License-Identifier: GPL-3.0-only
 * MT-40X - Casio MT-40 clone firmware for the M-VAVE FM-1.
 * Shared types: patterns, engine events, presets. Pure C99, no hardware deps.
 *
 * Attribution: the ROCK pattern is the "rock" auto-accompaniment preset of the
 * Casio MT-40 (Hiroko Okuda), source of the Sleng Teng riddim first heard on
 * "Under Mi Sleng Teng" (Wayne Smith / Noel Davey / King Jammy, 1985).
 * Casio asked only that the source be credited - this header does it.
 */
#ifndef MT40_TYPES_H
#define MT40_TYPES_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define MT40_MAX_STEPS      64u   /* 2 bars of 1/16 = 32; waltz 3/4 = 24 */
#define MT40_NDRUMS         4u    /* KICK, SNARE, HAT_CL, HAT_OP (the MT-40 four) */
#define MT40_MAX_BASS       32u   /* bass steps per pattern */
#define MT40_MAX_CHORDS     16u   /* chord stabs per pattern */
#define MT40_NVOICES        9u    /* 8 melody + 1 bass, as the real MT-40 */
#define MT40_NTMB           22u   /* the 22 MT-40 timbre slots */

enum { MT40_KICK = 0, MT40_SNARE = 1, MT40_HAT_CL = 2, MT40_HAT_OP = 3 };

/* One bass note of an accompaniment pattern: semitone offset from the root of
 * the chord currently held (chord.c on the FM-1), placed on a 1/16 step.
 * off -1 = chromatic approach below the root, 0 = root, +5 = fourth, +4 = third. */
typedef struct {
    uint8_t step;   /* 0..steps-1 */
    int8_t  off;    /* semitones above the chord root (can be negative) */
    uint8_t len;    /* length in steps (1 = staccato hit) */
} mt40_bass_t;

/* One chord stab of the accompaniment (the harmony layer the MT-40 plays). */
typedef struct {
    uint8_t step;
    uint8_t len;    /* length in steps */
} mt40_chord_t;

/* One accompaniment rhythm: drums as step bitmasks, bass and chords as lists.
 * verified = 1 only for ROCK (Sleng Teng): note content from two cross-checked
 * tablatures; the step placement is a reconstruction to calibrate by ear.
 * The five other rhythms ship as editable drafts. */
typedef struct {
    const char    *id;            /* "ROCK", "POPS"... */
    const char    *name;          /* display name */
    uint8_t        bars;          /* 2 (the MT-40 preset length) */
    uint8_t        steps_per_bar; /* 16 in 4/4, 12 in 3/4 */
    uint8_t        verified;      /* 1: transcribed from sources; 0: draft */
    uint8_t        bpm_hint;      /* musical reference tempo */
    uint32_t       drum_mask[MT40_NDRUMS];  /* bit s = hit on step s */
    const mt40_bass_t  *bass;
    uint8_t        nbass;
    const mt40_chord_t *chords;
    uint8_t        nchords;
    /* fill: replaces the drums on the last steps of the current bar */
    uint32_t       fill_snare_mask;
    uint32_t       fill_hatop_mask;
    /* variation B (alternative arrangement), masks override the base drums */
    uint32_t       varb_hatop_mask;   /* extra open-hat steps (0 = none) */
    const char    *attribution;
} mt40_pattern_t;

/* A keyboard timbre slot: the MT-40 "stairwave" emulated by a pulse wave with
 * duty, envelope (the sustain effect), vibrato LFO and a soft low-pass tone.
 * The 22 shipped values are starting points to calibrate by ear against a real
 * MT-40 demonstration (the panel names are not published in a verifiable list). */
typedef struct {
    const char *name;     /* slot name (draft until read off a real panel) */
    float       duty;     /* pulse duty cycle 0.1..0.9 */
    float       attack_ms;
    float       decay_ms;
    float       sustain;  /* 0..1 (the MT-40 sustain slider) */
    float       release_ms;
    float       vib_depth;/* 0..1 semitone-ish depth */
    float       vib_hz;
    float       cutoff;   /* tone low-pass, Hz */
    int8_t      oct;      /* -1, 0, +1 */
    float       vol;      /* 0..1 */
} mt40_timbre_t;

/* Engine event callbacks (fired from mt40_engine_tick). */
typedef void (*mt40_drum_fn)(void *user, int lane, float vel);
typedef void (*mt40_bass_fn)(void *user, int midi_note, int len_steps);
typedef void (*mt40_chord_fn)(void *user, int root_midi, const int8_t *iv, int n);

/* Chord shape: semitone intervals above the root (Felucca chord.c shapes).
 * The host test uses MAJ; on the FM-1 the engine subscribes to chord.c. */
typedef struct {
    int8_t  iv[4];
    uint8_t n;
} mt40_chord_shape_t;

#ifdef __cplusplus
}
#endif
#endif /* MT40_TYPES_H */
