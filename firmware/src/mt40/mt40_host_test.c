/* SPDX-License-Identifier: GPL-3.0-only
 * MT-40X host test - renders the Sleng Teng (ROCK pattern) at 82 BPM without
 * any hardware, prints every engine event and checks the invariants.
 *
 * Build & run:   make run        (see the Makefile)
 * Expected: the 14-note bass motif per 2-bar loop, 4 kicks, 4 snares, the
 * fill taking over the drums for one bar end, and a bounded audio render.
 */
#include "mt40_engine.h"
#include "mt40_patterns.h"
#include "mt40_voice.h"
#include "mt40_drums.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* The FM-1's real audio rate: 24 000 000 / 544 = 44 117.647 Hz. */
#define SR 44117.647f
#define BLOCK 128          /* CTL block, like the Felucca audio ISR */

static int ev_drum[MT40_NDRUMS];
static int ev_bass = 0, ev_chord = 0;
static int bass_seen[32];
static mt40_drums_t *g_drums;

static void on_drum(void *user, int lane, float vel)
{
    (void)user; (void)vel;
    ev_drum[lane]++;
}

static void on_drum_render(void *user, int lane, float vel)
{
    (void)user;
    if (g_drums)
        mt40_drums_hit(g_drums, lane, vel);
}

static void on_bass(void *user, int note, int len)
{
    (void)user; (void)len;
    if (ev_bass < 32)
        bass_seen[ev_bass] = note;
    ev_bass++;
}

static void on_chord(void *user, int root, const int8_t *iv, int n)
{
    (void)user; (void)root; (void)iv; (void)n;
    ev_chord++;
}

int main(void)
{
    const mt40_pattern_t *rock = mt40_patterns[0];
    mt40_engine_t eng;
    mt40_synth_t *synth;
    mt40_chord_shape_t maj = { {0, 4, 7, -1}, 3 };
    float mix[BLOCK];
    float peak = 0.0f;
    int ok = 1;

    /* one bar of 4/4 at BPM b lasts 4 * 60 / b seconds */
    float sec_bar = 4.0f * 60.0f / 82.0f;
    int blocks_per_bar = (int)(sec_bar * SR / BLOCK);

    printf("MT-40X host test\n");
    printf("  pattern    : %s (%s)\n", rock->name, rock->verified ? "verified" : "draft");
    printf("  length     : %u bars x %u steps of 1/16\n", rock->bars, rock->steps_per_bar);
    printf("  bass notes : %u per loop\n", rock->nbass);
    printf("  sample rate: %.3f Hz (FM-1: 24 MHz / 544)\n", SR);

    /* 1. engine: events over two loops at 82 BPM */
    mt40_engine_init(&eng, rock, 82.0f, SR);
    eng.on_drum = on_drum; eng.on_bass = on_bass; eng.on_chord = on_chord;
    mt40_engine_set_chord(&eng, 38, &maj);   /* D2 root, major shape */
    mt40_engine_start(&eng);

    int total_blocks = blocks_per_bar * 4;   /* 2 loops = 4 bars */
    for (int b = 0; b < total_blocks; b++)
        mt40_engine_tick(&eng, BLOCK);

    printf("\nEvents over 2 loops:\n");
    printf("  kick %d, snare %d, hat_cl %d, hat_op %d, bass %d, chords %d\n",
           ev_drum[MT40_KICK], ev_drum[MT40_SNARE], ev_drum[MT40_HAT_CL],
           ev_drum[MT40_HAT_OP], ev_bass, ev_chord);
    printf("  bass line (D2 root):");
    for (int i = 0; i < ev_bass && i < 32; i++)
        printf(" %d", bass_seen[i]);
    printf("\n");

    /* invariants */
    if (ev_bass != 28) { printf("  FAIL: expected 28 bass notes over 2 loops\n"); ok = 0; }
    if (ev_drum[MT40_KICK] != 8)  { printf("  FAIL: expected 8 kicks\n"); ok = 0; }
    if (ev_drum[MT40_SNARE] != 8) { printf("  FAIL: expected 8 snares\n"); ok = 0; }
    if (ev_drum[MT40_HAT_CL] != 32) { printf("  FAIL: expected 32 closed hats\n"); ok = 0; }
    if (ev_drum[MT40_HAT_OP] != 4)  { printf("  FAIL: expected 4 open hats\n"); ok = 0; }
    if (ev_chord != 8) { printf("  FAIL: expected 8 chord stabs\n"); ok = 0; }
    /* the iconic motif: approach note one semitone under the root (D2=38 -> 37) */
    if (bass_seen[0] != 37 || bass_seen[1] != 38) {
        printf("  FAIL: motif must open on 37, 38 (C#2, D2)\n"); ok = 0;
    }

    /* 2. fill: press FILL in the middle of bar 1; the fill takes over the end
     * of the pattern (steps 28..31 of bar 2), so the window spans 2 bars */
    memset(ev_drum, 0, sizeof(ev_drum));
    mt40_engine_stop(&eng);
    mt40_engine_start(&eng);
    for (int b = 0; b < blocks_per_bar / 2; b++)
        mt40_engine_tick(&eng, BLOCK);
    mt40_engine_trigger_fill(&eng);
    for (int b = blocks_per_bar / 2; b < blocks_per_bar * 2; b++)
        mt40_engine_tick(&eng, BLOCK);
    printf("\nWith FILL pressed mid bar 1 (over 2 bars): snare %d, hat_op %d\n",
           ev_drum[MT40_SNARE], ev_drum[MT40_HAT_OP]);
    /* over 2 bars with the fill: base snares at 4, 12, 20 (28 replaced by the
     * fill) + the 4-step fill roll 28..31 = 7; open hats: base 15 + fill 31 = 2 */
    if (ev_drum[MT40_SNARE] != 7) { printf("  FAIL: fill did not roll the snare\n"); ok = 0; }
    if (ev_drum[MT40_HAT_OP] != 2) { printf("  FAIL: fill hat_op missing\n"); ok = 0; }

    /* 3. audio render: synth + drums mixed, bounded, no NaN */
    g_drums = mt40_drums_new(SR);
    synth = mt40_synth_new(SR);
    eng.on_drum = on_drum_render;
    mt40_engine_stop(&eng);
    mt40_engine_start(&eng);
    mt40_synth_note_on(synth, MT40_NVOICES - 1, 38, &mt40_timbres[3]); /* bass voice */

    for (int b = 0; b < blocks_per_bar; b++) {
        memset(mix, 0, sizeof(mix));
        mt40_engine_tick(&eng, BLOCK);
        mt40_synth_render(synth, mix, BLOCK);
        float p = mt40_drums_render(g_drums, mix, BLOCK);
        if (p > peak) peak = p;
        for (int i = 0; i < BLOCK; i++) {
            if (!(mix[i] > -10.0f && mix[i] < 10.0f)) {
                printf("  FAIL: sample out of range (NaN or clipped)\n");
                ok = 0;
                b = blocks_per_bar;
                break;
            }
        }
    }
    printf("\nAudio render of one bar: peak %.3f (voices + drums)\n", peak);
    if (!(peak > 0.01f && peak < 10.0f)) {
        printf("  FAIL: peak out of expected range\n"); ok = 0;
    }

    mt40_synth_free(synth);
    mt40_drums_free(g_drums);
    printf("\n%s\n", ok ? "ALL CHECKS PASSED" : "CHECKS FAILED");
    return ok ? 0 : 1;
}
