/* SPDX-License-Identifier: GPL-3.0-only
 * MT-40X - Casio MT-40 clone firmware for the M-VAVE FM-1.
 * Integration shim: glues the pure-C99 MT-40 accompaniment (mt40_engine.c,
 * mt40_voice.c, mt40_drums.c) onto Felucca's audio path.
 *
 * Included by felucca.c after slicer.c and BEFORE fx.c, so mix_block (fx.c)
 * can call mt40_block, and before seq.c, whose transport (song.playing), tempo
 * (song.g[G_BPM]) and chord engine (chord.c chord_last) it follows through the
 * two accessors defined at the end of seq.c. seq.c's FX layer calls
 * mt40_perf_key for the free black keys (place 4 and up, no effect assigned).
 *
 * What it does per CTL block (32 samples, in the audio ISR):
 *   - follows the transport: the accompaniment starts and stops with PLAY,
 *   - follows the song tempo (G_BPM) and the chord of the selected track:
 *     the bass motif is transposed by the chord root, like the MT-40's
 *     mini-bass keyboard,
 *   - fires the pattern's drums into mt40_drums (PCM lanes or synth fallback)
 *     and the bass / chord stabs into mt40_voice (pulse "stairwave"),
 *   - mixes the result, Q15, into the master buses before the FX.
 *
 * The keyboard keeps playing through Felucca's own engines: MT-40X adds the
 * accompaniment, it does not take the parts over.
 *
 * Build flag FELUCCA_MT40 (default 1, 0 = plain Felucca).
 */
#ifndef FELUCCA_MT40
#define FELUCCA_MT40 1
#endif

#if FELUCCA_MT40

#define MT40_ON_DEVICE 1
#include "mt40_types.h"
#include "mt40_patterns.c"
#include "mt40_riddims.c"          /* la banque des 38 riddims du Riddim Real Book */
#include "mt40_engine.c"
#include "mt40_voice.c"
#include "mt40_drums.c"

/* seq.c provides (defined after chord.c): the transport's state and the
 * selected track's last chord (root, tones above the root, a generation
 * counter that increments whenever a new chord is played). */
static uint8_t mt40_seq_playing(void);
static void mt40_seq_chord(int *root, uint16_t *mask, uint32_t *gen);

/* the engine, its voices and its drums (one instance each, no heap) */
static mt40_engine_t mt40;
static mt40_synth_t *mt40_syn;                /* the instance mt40_synth_new returns */
static mt40_drums_t *mt40_drm;                /* the instance mt40_drums_new returns */
static uint8_t mt40_inited;
static uint8_t mt40_rhythm;                   /* index into mt40_patterns, 0 = ROCK */
static uint32_t mt40_chord_gen = 0xFFFFFFFFu; /* the chord generation seen last */
static int mt40_bass_left;                    /* samples until the bass note releases */
static int mt40_stab_root = 60;               /* the chord root at the keyboard register */
static mt40_chord_shape_t mt40_stab_shape;    /* built from chord_last's mask */

/* mix levels (Q15 scale below): calibrate by ear against a real MT-40 */
#define MT40_DRUM_LEVEL  0.50f
#define MT40_VOICE_LEVEL 0.50f

/* voice plan: 0..3 the chord stabs, 8 (the 9th) the auto-bass, like the MT-40 */
#define MT40_STAB_VOICES 4
#define MT40_BASS_VOICE  ((int)MT40_NVOICES - 1)
#define MT40_BASS_TIMBRE  3    /* SLOT 04: narrow pulse, one octave down */
#define MT40_CHORD_TIMBRE 1    /* SLOT 02 */

static float mt40_buf[CTL];                   /* the voices' render scratch */
static float mt40_dbuf[CTL];                  /* the drums' render scratch */

static void mt40_cb_drum(void *user, int lane, float vel)
{
    (void)user;
    mt40_drums_hit(mt40_drm, lane, vel);
}

static void mt40_cb_bass(void *user, int note, int len_steps)
{
    (void)user;
    mt40_synth_note_on(mt40_syn, MT40_BASS_VOICE, note, &mt40_timbres[MT40_BASS_TIMBRE]);
    mt40_bass_left = (int)(mt40.step_samples * (float)(len_steps > 0 ? len_steps : 1));
}

static void mt40_cb_chord(void *user, int root, const int8_t *iv, int n)
{
    int k;
    (void)user;
    (void)root;                               /* the stab keeps the keyboard register */
    if (n <= 0)
        return;
    for (k = 0; k < MT40_STAB_VOICES; k++) {
        if (k < n && mt40_stab_shape.n > 0) {
            int note = mt40_stab_root + iv[k];
            if (note < 0) note = 0;
            if (note > 127) note = 127;
            mt40_synth_note_on(mt40_syn, k, note, &mt40_timbres[MT40_CHORD_TIMBRE]);
        } else
            mt40_synth_note_off(mt40_syn, k);
    }
}

/* the chord_last mask (tones above the root) into a stab shape: the first four
 * tones, ascending (a DIA chord may carry more: the stab takes the triad-ish
 * lowest four, close enough for the stab layer) */
static void mt40_chord_apply(int root, uint16_t mask)
{
    int i, n = 0;
    if (root < 0 || root > 127 || !mask)
        return;
    mt40_stab_root = root;
    for (i = 0; i < 12 && n < 4; i++)
        if ((mask >> i) & 1u)
            mt40_stab_shape.iv[n++] = (int8_t)i;
    mt40_stab_shape.n = (uint8_t)n;
    /* the bass follows the root wrapped near D2 (38), the Sleng Teng register:
     * 38 + ((root - 38) wrapped into -6..5) */
    {
        int pc = ((root - 38) % 12 + 18) % 12 - 6;
        mt40_engine_set_chord(&mt40, 38 + pc, &mt40_stab_shape);
    }
}

static void mt40_init(void)
{
    mt40_engine_init(&mt40, mt40_patterns[mt40_rhythm], 120.0f, (float)FS);
    /* keep the instances the constructors return: the synth's sample rate and
     * the drums' gains live there. The 0.1.1 package threw them away and left
     * these two structs zeroed, so the accompaniment played silently. */
    mt40_syn = mt40_synth_new((float)FS);
    mt40_synth_all_off(mt40_syn);
    mt40_drm = mt40_drums_new((float)FS);
    mt40.on_drum = mt40_cb_drum;
    mt40.on_bass = mt40_cb_bass;
    mt40.on_chord = mt40_cb_chord;
    mt40_inited = 1;
}

/* the FX layer's free black keys (place 4 and up, no effect assigned):
 * 0 FILL, 1 VAR A/B, 2 next rhythm, 3 previous rhythm (seq.c perf_key_press) */
static void mt40_perf_key(int idx)
{
    if (idx == 0)
        mt40_engine_trigger_fill(&mt40);
    else if (idx == 1)
        mt40_engine_set_variation(&mt40, !mt40.var_b);
    else if (idx == 2 || idx == 3) {
        int i = (int)mt40_rhythm + (idx == 2 ? 1 : -1);
        if (i < 0) i = (int)mt40_npatterns - 1;
        if (i >= (int)mt40_npatterns) i = 0;
        mt40_rhythm = (uint8_t)i;
        mt40.pat = mt40_patterns[mt40_rhythm];   /* live, like the MT-40's buttons */
    }
}

/* the editor's riddim-bank commands (0x52 set, 0x53 info): the web page loads
 * a riddim as the accompaniment motif, like the OCT+/OCT- black keys do */
int mt40_ridb_count(void)
{
    return (int)mt40_npatterns;
}

int mt40_ridb_set(int idx)
{
    if (idx < 0 || idx >= (int)mt40_npatterns)
        return 0;
    mt40_rhythm = (uint8_t)idx;
    mt40.pat = mt40_patterns[mt40_rhythm];   /* live, like the MT-40's buttons */
    return 1;
}

/* one CTL block: transport, tempo, chord, engine events, render, mix */
static void mt40_block(int32_t *mix_l, int32_t *mix_r, uint32_t n)
{
    uint32_t i;
    if (!mt40_inited)
        mt40_init();
    if (!mt40_syn || !mt40_drm)
        return;
    if (mt40_seq_playing()) {
        if (!mt40.running) {
            mt40.fill_pending = 0;
            mt40_engine_start(&mt40);         /* pre-rolled: step 0 fires this block */
        }
    } else if (mt40.running) {
        mt40_engine_stop(&mt40);
        mt40_synth_all_off(mt40_syn);
        mt40_bass_left = 0;
    }
    if (!mt40.running)
        return;
    mt40_engine_set_bpm(&mt40, (float)song.g[G_BPM]);
    {
        int root;
        uint16_t mask;
        uint32_t gen;
        mt40_seq_chord(&root, &mask, &gen);
        if (gen != mt40_chord_gen) {
            mt40_chord_gen = gen;
            mt40_chord_apply(root, mask);
        }
    }
    for (i = 0; i < CTL; i++)
        mt40_buf[i] = mt40_dbuf[i] = 0.0f;
    mt40_engine_tick(&mt40, (int)n);
    if (mt40_bass_left > 0) {
        mt40_bass_left -= (int)n;
        if (mt40_bass_left <= 0)
            mt40_synth_note_off(mt40_syn, MT40_BASS_VOICE);
    }
    mt40_synth_render(mt40_syn, mt40_buf, (int)CTL);
    mt40_drums_render(mt40_drm, mt40_dbuf, (int)CTL);
    for (i = 0; i < n && i < CTL; i++) {
        int32_t s = (int32_t)(mt40_buf[i] * (MT40_VOICE_LEVEL * 32767.0f));
        int32_t d = (int32_t)(mt40_dbuf[i] * (MT40_DRUM_LEVEL * 32767.0f));
        mix_l[i] += s + d;
        mix_r[i] += s + d;
    }
}

#else  /* FELUCCA_MT40 = 0: plain Felucca, the hooks are empty */

static void mt40_perf_key(int idx) { (void)idx; }
static void mt40_block(int32_t *mix_l, int32_t *mix_r, uint32_t n)
{
    (void)mix_l; (void)mix_r; (void)n;
}

#endif /* FELUCCA_MT40 */
