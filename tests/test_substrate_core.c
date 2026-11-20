/* Direct contracts for the independent, static-inline kernel substrates.
 * CSR ownership/capacity and separate-TU checks live in test_num_csr.c and
 * test_cpp_link.cpp; this suite covers the remaining foundational operations. */
#include "mulib-kernel-substrates.h"
#include <assert.h>
#include <stdio.h>

static void storage(void)
{
    size_t product = 19;
    assert(!mulib_storage_size_mul(SIZE_MAX, 2, &product) && product == 19);
    assert(mulib_storage_size_mul(0, SIZE_MAX, &product) && product == 0);
    assert(mulib_storage_size_mul(7, 8, &product) && product == 56);
    assert(!mulib_storage_alloc(SIZE_MAX, 2));
    mulib_storage_bytes bytes = {0};
    assert(mulib_storage_bytes_append(&bytes, NULL, 0));
    assert(!mulib_storage_bytes_append(&bytes, NULL, 1) && bytes.n == 0);
    assert(mulib_storage_write_u64le(&bytes, UINT64_C(0x123456789abcdef0)));
    const unsigned char expected[] = {0xf0, 0xde, 0xbc, 0x9a, 0x78, 0x56, 0x34, 0x12};
    assert(bytes.n == sizeof expected && !memcmp(bytes.v, expected, sizeof expected));
    size_t pos = 0;
    uint64_t word = 23;
    assert(!mulib_storage_read_u64le(bytes.v, 7, &pos, &word));
    assert(pos == 0 && word == 23);
    assert(mulib_storage_read_u64le(bytes.v, bytes.n, &pos, &word));
    assert(pos == 8 && word == UINT64_C(0x123456789abcdef0));
    assert(!mulib_storage_read_u64le(bytes.v, bytes.n, &pos, &word) && pos == 8);
    mulib_storage_bytes_free(&bytes);
    assert(!bytes.v && !bytes.n && !bytes.cap);
    mulib_storage_bytes_free(&bytes);
    assert(mulib_storage_identifier_valid("_a19"));
    assert(!mulib_storage_identifier_valid("") && !mulib_storage_identifier_valid("9a"));
    assert(!mulib_storage_identifier_valid("a-b"));
}

static void numerics(void)
{
    mulib_num_rng rng = {0}, replay = {0};
    for (size_t i = 0; i < 1024; ++i) {
        assert(mulib_num_rng_next(&rng) == mulib_num_rng_next(&replay));
        double uniform = mulib_num_rng_uniform(&rng);
        assert(uniform >= 0 && uniform < 1);
        assert(uniform == mulib_num_rng_uniform(&replay));
    }
    mulib_num_complex a = {3, 4}, b = {1, -2};
    mulib_num_complex c = mulib_num_complex_mul(a, b);
    assert(c.re == 11 && c.im == -2);
    c = mulib_num_complex_div(c, b);
    assert(c.re == 3 && c.im == 4 && mulib_num_complex_abs(c) == 5);
    c = mulib_num_complex_sub(mulib_num_complex_add(a, b), b);
    assert(c.re == a.re && c.im == a.im);
    const double v[] = {3, 4}, w[] = {2, -1};
    assert(mulib_num_dot(v, w, 2) == 2 && mulib_num_norm(v, 2) == 5);
    const double large[] = {1e300, 1e300}, invalid[] = {NAN};
    assert(isfinite(mulib_num_norm(large, 2)));
    assert(mulib_num_all_finite(NULL, 0));
    assert(!mulib_num_all_finite(NULL, 1) && !mulib_num_all_finite(invalid, 1));
    /* Reference solver input copies and failure atomicity, not just its wrapper. */
    const double matrix[] = {4, 1, 2, 3}, rhs[] = {6, 8}, singular[] = {0, 0, 0, 0};
    double x[] = {-1, -1};
    assert(mulib_num_dense_reference_solve(2, matrix, rhs, x, 1e-12));
    assert(fabs(x[0] - 1) < 1e-12 && fabs(x[1] - 2) < 1e-12);
    assert(!mulib_num_dense_reference_solve(2, singular, rhs, x, 0));
    assert(!mulib_num_dense_reference_solve(2, matrix, rhs, x, NAN));
    assert(!mulib_num_dense_reference_solve(2, matrix, rhs, x, -1));
    assert(fabs(x[0] - 1) < 1e-12 && fabs(x[1] - 2) < 1e-12);
    assert(matrix[0] == 4 && rhs[0] == 6);
}

static void geometry(void)
{
    const mulib_geom2_point polygon[] = {{0, 0}, {2, 0}, {2, 2}, {0, 2}};
    const mulib_geom2_point origin = {0, 0}, end = {2, 0};
    const mulib_geom2_point boundary = {0, 1}, inside = {1, 1}, outside = {3, 1};
    assert(mulib_geom2_polygon_contains(polygon, 4, boundary));
    assert(mulib_geom2_polygon_contains(polygon, 4, inside));
    assert(!mulib_geom2_polygon_contains(polygon, 4, outside));
    assert(!mulib_geom2_polygon_contains(polygon, 2, inside));
    assert(mulib_geom2_segment_distance(inside, origin, end) == 1);
    assert(mulib_geom2_segment_distance(inside, origin, origin) == sqrt(2));
    assert(mulib_geom2_orient(origin, end, inside) == 2);
    assert(mulib_geom2_distance(origin, end) == 2);
    const mulib_geom2_box box = {{0, 0}, {2, 2}}, touching = {{2, 2}, {3, 3}};
    const mulib_geom2_box inverted = {{2, 2}, {0, 0}};
    const mulib_geom2_point nonfinite = {INFINITY, 0};
    assert(mulib_geom2_box_valid(box) && mulib_geom2_box_overlap(box, touching));
    assert(!mulib_geom2_box_valid(inverted) && !mulib_geom2_point_valid(nonfinite));
}

static void graphs(void)
{
    const mulib_graph_weighted_edge edges[] = {{0, 1, 2}, {1, 2, 3}, {0, 2, 9}};
    size_t order[4], previous[4];
    double distance[4];
    assert(mulib_graph_edges_toposort(4, edges, 3, order));
    assert(order[0] == 0 && order[1] == 3 && order[2] == 1 && order[3] == 2);
    assert(mulib_graph_dijkstra(4, edges, 3, 0, distance, previous));
    assert(distance[2] == 5 && previous[2] == 1);
    assert(!isfinite(distance[3]) && previous[3] == MULIB_STORAGE_NONE);
    const mulib_graph_weighted_edge cycle[] = {{0, 1, 1}, {1, 0, 1}};
    const mulib_graph_weighted_edge negative[] = {{0, 1, -1}};
    assert(!mulib_graph_edges_toposort(2, cycle, 2, order));
    assert(!mulib_graph_dijkstra(2, negative, 1, 0, distance, previous));
    size_t parents[] = {0, 1, 2, 3};
    mulib_graph_set_union(parents, 1, 3);
    assert(mulib_graph_set_find(parents, 3) == mulib_graph_set_find(parents, 1));
    assert(mulib_graph_set_find(parents, 0) != mulib_graph_set_find(parents, 3));
    unsigned char blocked[] = {0, 0, 0, 0, 1, 0, 0, 0, 0};
    size_t path[9], length = 0;
    assert(!mulib_graph_grid_route(3, 3, blocked, 0, 8, NULL, 0, &length));
    assert(length == 5);
    assert(mulib_graph_grid_route(3, 3, blocked, 0, 8, path, 9, &length));
    assert(length == 5 && path[0] == 0 && path[4] == 8);
    for (size_t i = 1; i < length; ++i) {
        size_t a = path[i - 1], b = path[i];
        assert(!blocked[b]);
        assert((a / 3 == b / 3 && (a + 1 == b || b + 1 == a)) ||
               (a % 3 == b % 3 && (a + 3 == b || b + 3 == a)));
    }
    blocked[1] = blocked[3] = 1;
    assert(!mulib_graph_grid_route(3, 3, blocked, 0, 8, path, 9, &length));
    assert(length == 0);
}

int main(void)
{
    storage();
    numerics();
    geometry();
    graphs();
    puts("PASS independent storage/numerical/geometry/graph substrate contracts");
    return 0;
}
