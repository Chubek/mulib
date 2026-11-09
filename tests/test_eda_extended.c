/* Boundary, failure-atomicity and cross-representation regressions for the EDA modules. */
#include "amb3r_implementation.c"
#include <assert.h>
#include <float.h>

static double square(double x, void *u) {
    (void)u;
    return x * x;
}
static double cubic(double x, void *u) {
    (void)u;
    return x * x * x - 1;
}
static double dcubic(double x, void *u) {
    (void)u;
    return 3 * x * x;
}
static double identity(double x, void *u) {
    (void)u;
    return x;
}
static double invalid(double x, void *u) {
    (void)x;
    (void)u;
    return NAN;
}

static void numerics(void) {
    mulib_num_sparse_t s = {0};
    assert(mulib_num_sparse_init(&s, 3, 3));
    assert(mulib_num_sparse_add(&s, 2, 0, 4));
    assert(mulib_num_sparse_add(&s, 0, 2, 3));
    assert(mulib_num_sparse_add(&s, 0, 0, 2));
    assert(mulib_num_sparse_add(&s, 1, 1, 5));
    assert(mulib_num_sparse_add(&s, 1, 1, -5));
    assert(mulib_num_sparse_canonicalize(&s));
    assert(s.count == 3 && s.triplets[0].row == 0 && s.triplets[0].col == 0);
    mulib_num_csr_t csr = {0}, transposed = {0};
    mulib_num_csc_t csc = {0};
    assert(mulib_num_csr_from_triplets(&s, &csr));
    assert(mulib_num_csc_from_triplets(&s, &csc));
    assert(csr.column[0] == 0 && csr.column[1] == 2);
    double a[] = {1, 2, 3}, b[] = {1, 2, 3}, c[] = {1, 2, 3};
    assert(mulib_num_sparse_matvec(&s, a, a));
    assert(mulib_num_csr_matvec(&csr, b, b));
    assert(mulib_num_csc_matvec(&csc, c, c));
    assert(a[0] == 11 && a[1] == 0 && a[2] == 4);
    assert(!memcmp(a, b, sizeof a) && !memcmp(a, c, sizeof a));
    assert(mulib_num_csr_transpose(&csr, &transposed));
    double x[] = {1, 2, 3}, y[] = {-1, -1, -1};
    assert(mulib_num_csr_matvec(&transposed, x, y) && y[0] == 14 && y[2] == 3);
    csr.column[0] = 3;
    y[0] = 42;
    assert(!mulib_num_csr_matvec(&csr, x, y) && y[0] == 42);
    csr.column[0] = 0;
    csc.offset[0] = 1;
    assert(!mulib_num_csc_validate(&csc));
    csc.offset[0] = 0;
    s.triplets[0].row = 3;
    assert(!mulib_num_csr_from_triplets(&s, &csr));
    s.triplets[0].row = 0;
    mulib_num_sparse_free(&s);
    mulib_num_csr_free(&csr);
    mulib_num_csr_free(&transposed);
    mulib_num_csc_free(&csc);
    double result = 42, error = 42;
    assert(mulib_num_integrate_adaptive(square, NULL, 0, 1, 1e-7, 16, &result, &error));
    assert(fabs(result - 1.0 / 3) < 1.1e-7 && error <= 1e-7);
    assert(mulib_num_integrate_adaptive(square, NULL, 1, 0, 1e-7, 16, &result, &error));
    assert(fabs(result + 1.0 / 3) < 1.1e-7);
    result = 42;
    assert(!mulib_num_integrate_adaptive(invalid, NULL, 0, 1, 1e-6, 8, &result, &error) &&
           result == 42);
    assert(!mulib_num_integrate_trapezoid(square, NULL, 0, 1, SIZE_MAX, &result));
    assert(mulib_num_newton(cubic, dcubic, NULL, .1, 1e-10, 30, &result) &&
           fabs(result - 1) < 1e-9);
    assert(mulib_num_bisect(identity, NULL, -DBL_MAX, DBL_MAX, 1e-6, 2, &result) && result == 0);
    double matrix[] = {1, NAN, 0, 1}, rhs[] = {1, 2}, sol[] = {42, 43};
    assert(!mulib_num_dense_solve(2, matrix, rhs, sol, 1e-12) && sol[0] == 42 && sol[1] == 43);
    mulib_num_stat_t first = {0}, second = {0}, all = {0};
    for (int i = 0; i < 100; i++) {
        assert(mulib_num_stat_push(&all, i));
        assert(mulib_num_stat_push(i < 40 ? &first : &second, i));
    }
    assert(mulib_num_stat_merge(&first, &second));
    assert(first.n == all.n && fabs(first.mean - all.mean) < 1e-10 &&
           fabs(first.m2 - all.m2) < 1e-8);
    first.n = SIZE_MAX;
    assert(!mulib_num_stat_merge(&first, &second));
}

static void logic(void) {
    const char symbols[] = "01xz";
    for (size_t i = 0; i < 4; i++)
        for (size_t j = 0; j < 4; j++) {
            char left[] = {symbols[i], 0}, right[] = {symbols[j], 0}, text[2];
            mulib_dsim_bitvec_t a = {0}, b = {0}, r = {0};
            assert(mulib_dsim_bitvec_parse(&a, left) && mulib_dsim_bitvec_parse(&b, right));
            assert(mulib_dsim_bitvec_or(a, b, &r) &&
                   mulib_dsim_bitvec_format(r, text, sizeof text));
            assert(text[0] == mulib_dsim_logic4_char(
                                  mulib_dsim_or((mulib_dsim_logic4_t)i, (mulib_dsim_logic4_t)j)));
            assert(mulib_dsim_bitvec_xor(a, b, &r) &&
                   mulib_dsim_bitvec_format(r, text, sizeof text));
            assert(text[0] == mulib_dsim_logic4_char(
                                  mulib_dsim_xor((mulib_dsim_logic4_t)i, (mulib_dsim_logic4_t)j)));
        }
    mulib_dsim_bitvec_t a = {UINT64_MAX, UINT64_MAX, 0, 64}, b = {1, UINT64_MAX, 0, 64}, r = {0};
    assert(mulib_dsim_bitvec_add(a, b, &r) && r.value == 0 && r.known == UINT64_MAX);
    assert(mulib_dsim_bitvec_shift(a, 64, 0, &r) && r.value == 0 && r.known == UINT64_MAX);
    assert(mulib_dsim_bitvec_shift(a, 63, 1, &r) && r.value == 1 && r.known == UINT64_MAX);
    assert(mulib_dsim_bitvec_not(a, &r) && r.value == 0);
    assert(mulib_dsim_bitvec_parse(&a, "1xz") && mulib_dsim_bitvec_parse(&b, "01"));
    char text[65];
    assert(mulib_dsim_bitvec_concat(a, b, &r) && mulib_dsim_bitvec_format(r, text, sizeof text) &&
           !strcmp(text, "1xz01"));
    b = a;
    assert(mulib_dsim_bitvec_add(a, b, &r) && r.known == 0);
    a.width = 255;
    assert(mulib_dsim_bitvec_bit(a, 100) == MULIB_DSIM_X);
    for (unsigned vars = 1; vars <= 6; vars++)
        for (unsigned index = 0; index < vars; index++)
            for (int value = 0; value <= 1; value++) {
                mulib_synth_truth_t t = {0}, cofactor = {0};
                assert(mulib_synth_truth_variable(vars, index, &t));
                assert(mulib_synth_truth_cofactor(t, index, value, &cofactor));
                for (unsigned i = 0; i < (1u << cofactor.variables); i++) {
                    int bit = -1;
                    assert(mulib_synth_truth_eval(cofactor, (uint8_t)i, &bit) && bit == value);
                }
            }
    mulib_dsim_scheduler_t scheduler = {0};
    mulib_dsim_event_t event = {0};
    for (unsigned i = 0; i < 100; i++)
        assert(mulib_dsim_scheduler_push(&scheduler, 99 - i, 0, 0, i % 2, MULIB_DSIM_1));
    assert(mulib_dsim_scheduler_cancel(&scheduler, 1) == 50);
    assert(mulib_dsim_scheduler_peek(&scheduler, &event) && event.time == 1);
    uint64_t last = 0;
    while (mulib_dsim_scheduler_pop(&scheduler, &event)) {
        assert(event.signal == 0 && event.time >= last);
        last = event.time;
    }
    assert(!mulib_dsim_scheduler_push(&scheduler, 0, 0, 0, 0, (mulib_dsim_logic4_t)-1));
    mulib_dsim_scheduler_free(&scheduler);
}

static void formal(void) {
    mulib_formal_cnf_t c = {0};
    int8_t bad[] = {32, 0};
    assert(!mulib_formal_cnf_add(&c, bad, 2) && c.vars == 0 && c.count == 0);
    assert(mulib_formal_cnf_and(&c, 3, 1, 2));
    for (int a = 0; a < 2; a++)
        for (int b = 0; b < 2; b++)
            for (int out = 0; out < 2; out++) {
                mulib_formal_cnf_t trial = c;
                int8_t literals[] = {a ? 1 : -1, b ? 2 : -2, out ? 3 : -3};
                for (size_t i = 0; i < 3; i++)
                    assert(mulib_formal_cnf_add(&trial, literals + i, 1));
                int8_t assignment[33] = {0};
                assert(mulib_formal_sat_solve(&trial, assignment, 100) ==
                       (out == (a && b) ? MULIB_FORMAL_SAT_SAT : MULIB_FORMAL_SAT_UNSAT));
            }
    int8_t assignment[33] = {0};
    assert(mulib_formal_cnf_add(&c, NULL, 0));
    assert(mulib_formal_sat_solve(&c, assignment, 100) == MULIB_FORMAL_SAT_UNSAT);
    c.clauses[0].size = 255;
    assert(mulib_formal_sat_solve(&c, assignment, 100) == MULIB_FORMAL_SAT_UNKNOWN);
    mulib_hwil_module_t m = {0};
    uint16_t id = 0;
    assert(!mulib_hwil_module_add(&m, (mulib_hwil_gate_t)-1, 0, 0, 0, &id));
    assert(mulib_hwil_module_add(&m, MULIB_HWIL_INPUT, 0, 0, 0, &id));
    assert(mulib_hwil_module_add(&m, MULIB_HWIL_NOT, 0, 0, 0, &id));
    m.nodes[1].a = 500;
    mulib_dsim_logic4_t in[] = {MULIB_DSIM_1}, values[] = {MULIB_DSIM_Z, MULIB_DSIM_Z};
    assert(!mulib_hwil_module_eval(&m, in, 1, values, 2) && values[0] == MULIB_DSIM_Z);
    FILE *fp = tmpfile();
    assert(fp);
    assert(!mulib_fmt_sv_emit(fp, &m, 1) && ftell(fp) == 0);
    fclose(fp);
    mulib_synth_aig_t g = {0};
    mulib_synth_aig_lit_t lit = 0;
    int value = -1;
    assert(mulib_synth_aig_input(&g, &lit));
    g.inputs = 0;
    assert(!mulib_synth_aig_eval(&g, NULL, 0, lit, &value) && value == -1);
}

static void storage(void) {
    mulib_eda_design_db_t db = {0}, snapshot = {0};
    mulib_eda_transaction_t a = {0}, b = {0};
    assert(mulib_eda_db_put(&db, (mulib_eda_db_record_t){1, 1, 5}));
    assert(mulib_eda_db_clone(&db, &snapshot));
    assert(mulib_eda_transaction_begin(&a, &db) && mulib_eda_transaction_begin(&b, &db));
    assert(mulib_eda_transaction_commit(&a));
    assert(!mulib_eda_transaction_commit(&b));
    mulib_eda_transaction_rollback(&b);
    assert(mulib_eda_db_put(&db, (mulib_eda_db_record_t){1, 1, 8}));
    mulib_eda_db_record_t record = {0};
    assert(mulib_eda_db_get(&snapshot, 1, &record) && record.value == 5);
    mulib_eda_db_free(&db);
    mulib_eda_db_free(&snapshot);
    mulib_eda_hierarchy_t h = {0};
    uint32_t id = UINT32_MAX;
    for (size_t i = 0; i < 200; i++)
        assert(mulib_eda_hierarchy_add(&h, id, "a", &id));
    char path[400];
    assert(mulib_eda_hierarchy_path(&h, id, path, sizeof path) && strlen(path) == 399);
    strcpy(path, "unchanged");
    assert(!mulib_eda_hierarchy_path(&h, id, path, 10) && !strcmp(path, "unchanged"));
    h.nodes[0].parent = id;
    assert(!mulib_eda_hierarchy_path(&h, id, path, sizeof path));
    mulib_eda_hierarchy_free(&h);
    mulib_soc_memorymap_t map = {0};
    uint32_t device = 0;
    uint64_t offset = 0;
    assert(mulib_soc_memorymap_add(&map, UINT64_MAX, 1, 1));
    assert(mulib_soc_memorymap_lookup(&map, UINT64_MAX, &device, &offset) && device == 1 &&
           offset == 0);
    assert(!mulib_soc_memorymap_add(&map, UINT64_MAX - 1, 2, 2));
    mulib_eda_attrs_t attrs = {0};
    mulib_eda_attr_t attr = {0};
    attr.owner = 1;
    attr.type = MULIB_EDA_ATTR_INTEGER;
    strcpy(attr.key, "a");
    assert(mulib_eda_attrs_set(&attrs, &attr) && mulib_eda_attrs_remove(&attrs, 1, "a") &&
           !mulib_eda_attrs_get(&attrs, 1, "a"));
    mulib_eda_attrs_free(&attrs);
    mulib_eda_netgraph_t graph = {0};
    uint32_t n0, n1, t;
    assert(mulib_eda_netgraph_net(&graph, &n0) && mulib_eda_netgraph_net(&graph, &n1));
    assert(mulib_eda_netgraph_terminal(&graph, 1, &t) && mulib_eda_netgraph_connect(&graph, t, n1));
    assert(mulib_eda_netgraph_merge(&graph, n0, n1) && mulib_eda_netgraph_degree(&graph, n0) == 1 &&
           mulib_eda_netgraph_degree(&graph, n1) == 0);
    mulib_eda_netgraph_free(&graph);
}

static void wave_and_physics(void) {
    mulib_wave_signal_t s = {0};
    s.kind = MULIB_WAVE_ANALOG;
    double v = 42;
    assert(mulib_wave_signal_push(&s, 0, 0) && mulib_wave_signal_push(&s, 1, 2) &&
           mulib_wave_signal_push(&s, 2, 0));
    assert(mulib_wave_signal_interpolate(&s, .5, &v) && v == 1);
    assert(mulib_wave_measure_integral(&s, &v) && v == 2);
    assert(mulib_wave_signal_push(&s, 2, 4) && mulib_wave_signal_interpolate(&s, 2, &v) && v == 4);
    s.kind = MULIB_WAVE_DIGITAL;
    assert(mulib_wave_signal_interpolate(&s, .5, &v) && v == 0);
    s.kind = MULIB_WAVE_ANALOG;
    for (size_t buckets = 1; buckets <= s.count; buckets++) {
        mulib_wave_sample_t lo[4], hi[4];
        assert(mulib_wave_decimate_minmax(&s, buckets, lo, hi));
        for (size_t i = 0; i < buckets; i++)
            assert(lo[i].value <= hi[i].value);
    }
    s.samples[1].value = NAN;
    v = 42;
    assert(!mulib_wave_measure_rms(&s, &v) && v == 42);
    mulib_wave_signal_free(&s);
    assert(mulib_field_thermal_flux_1d(2, 5, 0, 300, 310, 1, &v) && v == -25);
    assert(mulib_field_poisson_field_1d(2, 1, 0, 0, 10, 1, &v) && v == -5);
    mulib_rf_complex_t z = {DBL_MAX, DBL_MAX}, gamma = {0}, back = {0};
    assert(mulib_rf_s11_from_impedance(z, 50, &gamma) && isfinite(gamma.re));
    z.re = 75;
    z.im = 25;
    assert(mulib_rf_s11_from_impedance(z, 50, &gamma) &&
           mulib_rf_impedance_from_s11(gamma, 50, &back));
    assert(fabs(back.re - z.re) < 1e-10 && fabs(back.im - z.im) < 1e-10);
    assert(mulib_rf_s11_vswr((mulib_rf_complex_t){.5, 0}, &v) && v == 3);
    assert(!mulib_rf_s11_vswr((mulib_rf_complex_t){1, 0}, &v));
    mulib_eda_quantity_t a = {10, .5, {{1, 0, 0, 0, 0, 0, 0}}},
                         b = {2, .1, {{0, 0, 1, 0, 0, 0, 0}}}, r = {0};
    assert(mulib_eda_quantity_div(a, b, &r) && r.value == 5 && r.dim.exp[0] == 1 &&
           r.dim.exp[2] == -1);
    b.value = 0;
    assert(!mulib_eda_quantity_div(a, b, &r));
}

static void invalid_sources(void) {
    const char *comments[] = {"/*", "/**", "/*x", "/*xx"};
    for (size_t i = 0; i < 4; i++) {
        mulib_hdl_lexer_t lex = {0};
        mulib_hdl_lexer_init(&lex, comments[i], strlen(comments[i]));
        assert(mulib_hdl_lexer_next(&lex).kind == MULIB_HDL_ERROR);
        assert(lex.cursor == lex.length);
    }
    mulib_hdl_lexer_t lex = {0};
    mulib_hdl_lexer_init(&lex, "\\odd.name ", 10);
    mulib_hdl_token_t tok = mulib_hdl_lexer_next(&lex);
    assert(tok.kind == MULIB_HDL_ID && tok.length == 9);
    mulib_spice_netlist_t net = {0};
    net.nodes = 1;
    strcpy(net.node_names[0], "a");
    net.parts = 1;
    net.parts_data[0] = (mulib_spice_part_t){"R1", 'R', 500, 0, 10};
    double samples[2] = {42, 43};
    mulib_spice_complex_t ac[1] = {{42, 43}};
    assert(!mulib_spice_op_solve(&net, samples, 2) && samples[0] == 42);
    assert(!mulib_spice_tran_fixed(&net, 1, 2, 1, samples) && samples[0] == 42);
    assert(!mulib_spice_ac_solve(&net, 1, ac, 1) && ac[0].re == 42);
    mulib_ecad_schematic_t schematic = {0};
    mulib_ecad_component_t component = {"R1", 'C', "a", "0", 1};
    assert(!mulib_ecad_schematic_add(&schematic, &component));
    component.device = 'R';
    component.value = -1;
    assert(!mulib_ecad_schematic_add(&schematic, &component));
    component.value = 1;
    assert(mulib_ecad_schematic_add(&schematic, &component));
    char buffer[64] = "unchanged";
    assert(!mulib_ecad_netlist_write(&schematic, buffer, 2, NULL) && !strcmp(buffer, "unchanged"));
    FILE *fp = tmpfile();
    assert(fp);
    mulib_pcb_segment_t traces[2] = {{{0, 0}, {1, 1}, 1, 1}, {{0, 0}, {1, 1}, NAN, 2}};
    assert(!mulib_pcb_gerber_write(fp, traces, 2) && ftell(fp) == 0);
    assert(mulib_pcb_drc_segments(traces, 2, 1, NULL, 0) == SIZE_MAX);
    fclose(fp);
    mulib_asic_timing_node_t timing[2] = {{1, 42, {0}, 0}, {1, 43, {1}, 1}};
    double critical = 42;
    assert(!mulib_asic_sta_propagate(timing, 2, &critical) && timing[0].arrival == 42 &&
           critical == 42);
    mulib_hls_operation_t ops[2] = {{1, {0}, 0, 42}, {1, {1}, 1, 43}};
    uint32_t span = 42;
    assert(!mulib_hls_schedule_asap(ops, 2, &span) && ops[0].start == 42 && span == 42);
}
int main(void) {
    numerics();
    logic();
    formal();
    storage();
    wave_and_physics();
    invalid_sources();
    puts("PASS EDA extensions and hostile inputs");
    return 0;
}
