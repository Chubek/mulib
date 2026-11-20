/* Independent storage, numerical, geometry and graph mechanisms. */
#include "mulib-kernel-substrates.h"
#include <assert.h>
#include <stdio.h>

int main(void)
{
    mulib_storage_bytes bytes = {0};
    assert(mulib_storage_write_u64le(&bytes, UINT64_C(42)));
    size_t position = 0;
    uint64_t value = 0;
    assert(mulib_storage_read_u64le(bytes.v, bytes.n, &position, &value));
    assert(value == 42 && position == bytes.n);
    mulib_storage_bytes_free(&bytes);

    const size_t offsets[] = {0, 1, 2}, columns[] = {0, 1};
    const double coefficients[] = {2, 3}, input[] = {4, 5};
    const mulib_num_csr_view matrix = {2, 2, 2, offsets, columns, coefficients};
    double output[2];
    assert(mulib_num_csr_view_valid(&matrix));
    mulib_num_csr_view_matvec(&matrix, input, output);
    assert(output[0] == 8 && output[1] == 15);

    const mulib_geom2_point origin = {0, 0}, right = {1, 0}, above = {0, 1};
    assert(mulib_geom2_orient(origin, right, above) > 0);

    const mulib_graph_weighted_edge edges[] = {{0, 1, 2}, {1, 2, 3}};
    double distance[3];
    size_t previous[3];
    assert(mulib_graph_dijkstra(3, edges, 2, 0, distance, previous));
    assert(distance[2] == 5 && previous[2] == 1);
    puts("Independent kernel substrates ready");
    return 0;
}
