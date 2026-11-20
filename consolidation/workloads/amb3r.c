/* C-only Amb3r mechanism chain: schematic -> netlist -> transient -> waveform. */
#define MULIB_IMPL_ECAD_SCHEMATIC
#define MULIB_IMPL_ECAD_NETLIST
#define MULIB_IMPL_SPICE_LEXER
#define MULIB_IMPL_SPICE_NETLIST
#define MULIB_IMPL_SPICE_STAMP
#define MULIB_IMPL_SPICE_R
#define MULIB_IMPL_SPICE_C
#define MULIB_IMPL_SPICE_INDEPENDENT
#define MULIB_IMPL_NUM_DENSE
#define MULIB_IMPL_SPICE_OP
#define MULIB_IMPL_SPICE_TRAN
#define MULIB_IMPL_WAVE_SIGNAL
#define MULIB_IMPL_WAVE_MEASURE
#include "mulib-ecad-netlist.h"
#include "mulib-spice-op.h"
#include "mulib-spice-tran.h"
#include "mulib-wave-measure.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

enum { STEPS = 80 };

int
main(int argc, char **argv)
{
    const mulib_ecad_component_t parts[] = {
        {"V1", 'V', "in", "0", 8.0},
        {"R1", 'R', "in", "out", 1000.0},
        {"R2", 'R', "out", "0", 3000.0},
        {"C1", 'C', "out", "0", 1e-6}
    };
    mulib_ecad_schematic_t schematic = {0};
    for (size_t i = 0; i < sizeof parts / sizeof *parts; ++i)
        assert(mulib_ecad_schematic_add(&schematic, &parts[i]));
    assert(!mulib_ecad_schematic_add(&schematic, &parts[1]) && schematic.count == 4);
    char deck[512];
    size_t used = 0;
    assert(mulib_ecad_netlist_write(&schematic, deck, sizeof deck, &used));
    mulib_spice_netlist_t parsed = {0};
    assert(mulib_spice_netlist_parse(deck, &parsed, NULL));
    int probe = mulib_spice_netlist_find_node(&parsed, "out");
    assert(probe > 0 && probe <= UINT16_MAX);

    double op[8], samples[STEPS];
    assert(mulib_spice_op_solve(&parsed, op, sizeof op / sizeof *op));
    assert(fabs(op[probe - 1] - 6.0) < 1e-12); /* 8 * 3k/(1k+3k). */

    const double dt = 0.000075, tau = 0.00075; /* Rth=750 ohms, C=1 uF. */
    assert(mulib_spice_tran_fixed(&parsed, dt, STEPS, (uint16_t)probe, samples));
    mulib_wave_signal_t wave = {.kind = MULIB_WAVE_ANALOG};
    assert(mulib_wave_signal_push(&wave, 0.0, 0.0));
    double sq = 0;
    for (size_t i = 0; i < STEPS; ++i) {
        double step = (double)(i + 1), time = step * dt;
        /* Closed-form discrete RC oracle, independent of the MNA solver. */
        double expected = 6.0 * (1.0 - pow(10.0 / 11.0, step));
        assert(fabs(samples[i] - expected) < 1e-11);
        assert(fabs(samples[i] - 6.0 * (1.0 - exp(-time / tau))) < 0.11);
        sq += expected * expected;
        assert(mulib_wave_signal_push(&wave, time, samples[i]));
    }
    double rms, midpoint;
    size_t crossings;
    assert(mulib_wave_measure_rms(&wave, &rms));
    assert(fabs(rms - sqrt(sq / (STEPS + 1))) < 1e-11);
    assert(mulib_wave_measure_crossings(&wave, 3.0, &crossings) && crossings == 1);
    assert(mulib_wave_signal_interpolate(&wave, 7.5 * dt, &midpoint));
    assert(fabs(midpoint - 6.0 * (1.0 - 0.5 * (pow(10.0 / 11.0, 7) +
                                                pow(10.0 / 11.0, 8)))) < 1e-11);
    mulib_wave_signal_free(&wave);

    /* The floating resistor is a singular system; no partial trace may escape. */
    assert(mulib_spice_netlist_parse("R1 a b 1000\n", &parsed, NULL));
    for (size_t i = 0; i < STEPS; ++i)
        samples[i] = -42.0;
    assert(!mulib_spice_tran_fixed(&parsed, dt, STEPS, 1, samples));
    for (size_t i = 0; i < STEPS; ++i)
        assert(samples[i] == -42.0);

    if (argc == 2 && !strcmp(argv[1], "--deck")) {
        /* Netlist writer emits .end; place batch commands before it. */
        char *end = strstr(deck, ".end");
        assert(end && end > deck && end < deck + used);
        *end = '\0';
        printf("Amb3r RC operating point\n%s.control\nop\nprint v(out)\nquit\n.endc\n.end\n", deck);
    } else {
        assert(argc == 1);
        puts("PASS Amb3r C-only: schematic, DC/transient RC, waveform, singular rollback");
    }
    return 0;
}
