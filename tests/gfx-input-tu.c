/* Independent translation unit: no external implementation switches required. */
#include "mulib-glx-eventloop.h"
#include "mulib-input-compose.h"
#include "mulib-input-gamepad.h"
#include "mulib-input-gesture.h"
#include "mulib-input-seat.h"
#include "mulib-input-tablet.h"

int gfx_input_tu_smoke(void)
{
    mulib_glx_event_queue_t queue = {0};
    mulib_glx_eventloop_t loop = {0};
    mulib_input_seat_t seat;
    mgx_event event = {0}, output;
    int ok;
    mulib_input_seat_init(&seat);
    event.type = MGX_EVENT_KEY; event.code = 30; event.down = 1;
    ok = mulib_glx_event_push(&queue, event)
        && mulib_glx_event_pop(&queue, &output) && output.code == 30
        && mulib_glx_eventloop_poll(&loop, 0) == 0;
    mulib_glx_event_free(&queue);
    mulib_glx_eventloop_free(&loop);
    return ok;
}
