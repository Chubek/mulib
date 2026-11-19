#define _POSIX_C_SOURCE 200809L
#define MULIB_IMPL_INPUT
#include "mulib-input.h"
#include "mulib-input-seat.h"
#include <assert.h>
#include <fcntl.h>
#include <unistd.h>
#ifdef __linux__
#include <linux/input.h>

static void send_record(int fd, const struct input_event *native)
{
    assert(write(fd, native, sizeof *native) == (ssize_t)sizeof *native);
}
#endif

int gfx_input_evdev_smoke(void)
{
#ifdef __linux__
    int p[2];
    mulib_input_t borrowed;
    mulib_input_event_t raw, sentinel = {123, 1, 30, 1};
    struct input_event native = {0};
    mulib_input_seat_t seat;
    mulib_glx_event_queue_t queue = {0};
    mgx_event event;
    assert(pipe(p) == 0);
    assert(fcntl(p[0], F_SETFL, O_NONBLOCK) == 0);
    borrowed.fd = p[0];
    mulib_input_seat_init(&seat);
    assert(mulib_input_read(&borrowed, &raw) == MULIB_INPUT_AGAIN);
    native.input_event_sec = 1; native.input_event_usec = 2;
    native.type = EV_KEY; native.code = KEY_A; native.value = 1;
    send_record(p[1], &native);
    native.input_event_usec = 3; native.value = 0;
    send_record(p[1], &native);
    for (int i = 0; i < 2; ++i) {
        assert(mulib_input_read(&borrowed, &raw) == MULIB_INPUT_OK);
        assert(mulib_input_seat_feed(&seat, &raw, 100, 100, &event) == 1);
        assert(mulib_glx_event_push(&queue, event));
    }
    assert(seat.keyboard.pressed[KEY_A] && seat.keyboard.released[KEY_A]);
    assert(mulib_glx_event_pop(&queue, &event) && event.down == 1 && event.time_us == 1000002);
    assert(mulib_glx_event_pop(&queue, &event) && event.down == 0 && event.time_us == 1000003);
    assert(mulib_input_read(&borrowed, &raw) == MULIB_INPUT_AGAIN);

    raw = sentinel;
    native.input_event_usec = 1000000;
    send_record(p[1], &native);
    assert(mulib_input_read(&borrowed, &raw) == MULIB_INPUT_EIO && errno == EOVERFLOW);
    assert(!memcmp(&raw, &sentinel, sizeof raw));
    native.input_event_usec = 4; native.code = 768;
    send_record(p[1], &native);
    assert(mulib_input_read(&borrowed, &raw) == MULIB_INPUT_EIO && errno == EINVAL);
    assert(!memcmp(&raw, &sentinel, sizeof raw));
    native.code = KEY_A; native.value = 3;
    send_record(p[1], &native);
    assert(mulib_input_read(&borrowed, &raw) == MULIB_INPUT_EIO && errno == EINVAL);
    assert(!memcmp(&raw, &sentinel, sizeof raw));
    /* The upstream reader explicitly rejects short native records, rather
     * than pretending pipe/socket fragmentation is an evdev device record. */
    assert(write(p[1], &native, sizeof native - 1) == (ssize_t)sizeof native - 1);
    assert(mulib_input_read(&borrowed, &raw) == MULIB_INPUT_EIO && errno == EIO);
    assert(!memcmp(&raw, &sentinel, sizeof raw));
    assert(close(p[1]) == 0);
    assert(mulib_input_read(&borrowed, &raw) == MULIB_INPUT_EIO && errno == EIO);
    assert(close(p[0]) == 0);
    mulib_glx_event_free(&queue);
#endif
    return 1;
}
