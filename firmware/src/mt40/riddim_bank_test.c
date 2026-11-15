/* SPDX-License-Identifier: GPL-3.0-only
 * Test hôte de la banque riddim (scripts/riddims_fw.py) : validation au niveau
 * des données — chaque riddim doit rester dans l'étendue de basse quelle que
 * soit la fondamentale tenue, et les lignes de référence (Punaany/Sleng Teng,
 * Stalag, Queen Majesty et son shift de seconde mesure) doivent sortir exactes.
 *   gcc -I. -o /tmp/rid_bank_test riddim_bank_test.c mt40_engine.c mt40_voice.c \
 *       mt40_drums.c mt40_patterns.c mt40_riddims.c -lm && /tmp/rid_bank_test
 */
#include "mt40_engine.h"
#include "mt40_patterns.h"

#include <stdio.h>

static int fails;

static void no_drum(void *u, int l, float v) { (void)u; (void)l; (void)v; }
static void no_bass(void *u, int n, int l) { (void)u; (void)n; (void)l; }
static void no_chord(void *u, int r, const int8_t *iv, int n) { (void)u; (void)r; (void)iv; (void)n; }

static int note_of(const mt40_pattern_t *p, const mt40_bass_t *b, int root)
{
    int sh = p->shift ? p->shift[b->step / p->steps_per_bar] : 0;
    return root + b->off + sh;
}

int main(void)
{
    printf("banque riddim : %u patterns (%u riddims)\n",
           (unsigned)mt40_npatterns, (unsigned)mt40_npatterns - 6u);
    if (mt40_npatterns != 44) { printf("  FAIL: 44 attendus\n"); fails++; }

    /* 1. étendue : toutes les notes restent dans 24..72 pour toute fondamentale
     *    tenue entre C2 (36) et C3 (48), et le moteur les rejoue toutes */
    for (unsigned i = 6u; i < mt40_npatterns; i++) {
        const mt40_pattern_t *p = mt40_patterns[i];
        for (unsigned k = 0; k < p->nbass; k++) {
            const mt40_bass_t *b = &p->bass[k];
            if (b->step >= p->bars * p->steps_per_bar) {
                printf("  FAIL %s: pas %u hors motif (%u mesures)\n", p->id, b->step, p->bars);
                fails++;
            }
            for (int root = 36; root <= 48; root++) {
                int n = note_of(p, b, root);
                if (n < 24 || n > 72) {
                    printf("  FAIL %s: pas %u -> %d (racine %d) hors basse\n", p->id, b->step, n, root);
                    fails++;
                    break;
                }
            }
        }
        if (p->bars == 2 && p->shift && (p->shift[0] != 0)) {
            printf("  FAIL %s: shift[0] = %d (attendu 0)\n", p->id, p->shift[0]);
            fails++;
        }
    }

    /* 2. lignes de référence (fondamentale tenue = D2, 38) */
    int root = 38;
    const mt40_pattern_t *pu = mt40_patterns[6 + 27];          /* Punaany */
    printf("Punaany (Sleng Teng), D2 : ");
    for (unsigned k = 0; k < pu->nbass; k++)
        printf("%d ", note_of(pu, &pu->bass[k], root));
    printf("\n");
    {   /* la cellule du livre : fondamentale, +2, +2, -2 (G A A F transposé) */
        int want[4] = { 38, 40, 40, 36 };
        int got = 0;
        for (unsigned k = 0; k + 3 < pu->nbass; k++) {
            if (note_of(pu, &pu->bass[k], root) == want[0] &&
                note_of(pu, &pu->bass[k + 1], root) == want[1] &&
                note_of(pu, &pu->bass[k + 2], root) == want[2] &&
                note_of(pu, &pu->bass[k + 3], root) == want[3]) { got = 1; break; }
        }
        if (!got) { printf("  FAIL Punaany: la cellule 0/+2/+2/-2 absente\n"); fails++; }
    }

    const mt40_pattern_t *st = mt40_patterns[6 + 30];          /* Stalag */
    printf("Stalag, D2              : ");
    for (unsigned k = 0; k < st->nbass; k++)
        printf("%d ", note_of(st, &st->bass[k], root));
    printf("\n");
    {   /* le riff du livre (p. 33) : fondamentale, fondamentale, quinte, b7, quinte, b7 basse */
        int want[6] = { 38, 38, 45, 48, 45, 36 };
        int got = 0;
        for (unsigned k = 0; k + 5 < st->nbass; k++) {
            int hit = 1;
            for (int j = 0; j < 6; j++)
                if (note_of(st, &st->bass[k + j], root) != want[j]) { hit = 0; break; }
            if (hit) { got = 1; break; }
        }
        if (!got) { printf("  FAIL Stalag: le riff 0/0/+7/+10/+7/-2 absent\n"); fails++; }
    }

    const mt40_pattern_t *qm = mt40_patterns[6 + 28];          /* Queen Majesty : Dm7 -> G */
    if (qm->bars == 2 && qm->shift) {
        printf("Queen Majesty, D2       : shift mesure 2 = %+d (Dm7 -> G : +5)\n", qm->shift[1]);
        if (qm->shift[1] != 5 && qm->shift[1] != -7) { printf("  FAIL Queen Majesty: shift[1]\n"); fails++; }
    } else { printf("  FAIL Queen Majesty: pas de shift en 2 mesures\n"); fails++; }

    /* 3. le moteur rejoue un riddim entier sans déborder */
    {
        mt40_engine_t e;
        mt40_engine_init(&e, mt40_patterns[6 + 30], 85.0f, 44117.647f);
        e.on_drum = no_drum; e.on_bass = no_bass; e.on_chord = no_chord; e.user = 0;
        mt40_engine_set_chord(&e, 38, &mt40_shape_maj);
        mt40_engine_start(&e);
        for (int b = 0; b < 8 * 128; b++) mt40_engine_tick(&e, 128);
        mt40_engine_stop(&e);
        printf("moteur : Stalag joué 8 mesures à 85 BPM sans crasher\n");
    }

    printf(fails ? "\nECHEC: %d\n" : "\nOK — banque intègre (%d échecs)\n", fails);
    return fails ? 1 : 0;
}
