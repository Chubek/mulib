#define MULIB_IMPL_BROWSER_SNAPSHOT
#define MULIB_IMPL_BOUNDED_TEXT
#define MULIB_IMPL_UTF8
#include "mulib-browser-snapshot.h"
#include "mulib-browser-checkpoint.h"
#include "mulib-browser-renderer.h"
#include "mulib-browser-replay.h"
#include <assert.h>

typedef struct {
    uint64_t document, sequence, reject_sequence;
    unsigned value, calls;
} consumer;

static int
apply(void *ctx, const mulib_browser_wire_event *event)
{
    consumer *c = ctx;
    ++c->calls;
    if (event->seq == c->reject_sequence || event->document != c->document
        || event->seq <= c->sequence || event->seq - c->sequence != 1
        || event->channel != 8 || event->kind != 1 || event->payload.len != 1)
        return 0;
    c->value += event->payload.data[0];
    c->sequence = event->seq;
    return 1;
}

static int
deliver(void *ctx, const mulib_browser_journal_event *event)
{
    mulib_browser_wire_event e = {
        event->seq, event->document, event->node, event->channel, event->kind,
        {event->payload.data, event->payload.len}
    };
    muba_buf packet = {0};
    mulib_browser_wire_event decoded;
    size_t used = 0;
    int ok = mulib_browser_wire_encode(e, &packet)
             && mulib_browser_wire_decode((muba_span){packet.data, packet.len},
                                          1, &decoded, &used)
             && used == packet.len
             && mulib_browser_renderer_publish(ctx, &decoded);
    muba_free(&packet);
    return ok;
}

int
main(void)
{
    mulib_browser_journal_event entries[3] = {0};
    mulib_browser_journal_t j = {entries, 0, 3, 0, 3, 0};
    mulib_browser_renderer_t r = {0};
    consumer c = {.document = 5};
    unsigned char delta = 2;
    uint64_t after = 0;

    assert(mulib_browser_renderer_attach(&r, 1, 1, 1, apply, &c));
    assert(mulib_browser_journal_append(&j, 8, 1, 5, 0, (muba_span){&delta, 1}));
    assert(mulib_browser_replay_run_contiguous(&j, &after, deliver, &r)
           == MULIB_BROWSER_REPLAY_COMPLETE);
    assert(after == 1 && c.sequence == 1 && c.value == 2 && c.calls == 1);
    delta = 3;
    assert(mulib_browser_journal_append(&j, 8, 1, 5, 0, (muba_span){&delta, 1}));
    mulib_browser_journal_clear(&j);
    assert(mulib_browser_replay_run_contiguous(&j, &after, deliver, &r)
           == MULIB_BROWSER_REPLAY_GAP);

    /* The producer supplies full state and its sequence from the same
     * quiescent point; the replay layer cannot construct that state. */
    unsigned char full_value = 5;
    mulib_browser_checkpoint_t point;
    mulib_browser_snapshot_t snapshot = {0};
    assert(mulib_browser_checkpoint_capture(&j, 5, 0, &point));
    assert(mulib_browser_snapshot_make(&snapshot, &full_value, 1, point.sequence, 1));
    delta = 7;
    assert(mulib_browser_journal_append(&j, 8, 1, 5, 0, (muba_span){&delta, 1}));
    delta = 11;
    assert(mulib_browser_journal_append(&j, 8, 1, 5, 0, (muba_span){&delta, 1}));
    delta = 99;
    assert(mulib_browser_replay_run_contiguous(&j, &after, deliver, &r)
           == MULIB_BROWSER_REPLAY_GAP);
    assert(after == 1 && c.sequence == 1 && c.value == 2 && c.calls == 1);

    c.value = (unsigned char)snapshot.bytes.data[0];
    c.document = point.document;
    after = c.sequence = snapshot.version;
    mulib_browser_snapshot_free(&snapshot);

    c.reject_sequence = 4;
    assert(mulib_browser_replay_run_contiguous(&j, &after, deliver, &r)
           == MULIB_BROWSER_REPLAY_REJECTED);
    assert(after == 3 && c.sequence == 3 && c.value == 12 && c.calls == 3);
    c.reject_sequence = 0;
    assert(mulib_browser_replay_run_contiguous(&j, &after, deliver, &r)
           == MULIB_BROWSER_REPLAY_COMPLETE);
    assert(after == 4 && c.sequence == 4 && c.value == 23 && c.calls == 4);
    assert(mulib_browser_replay_run_contiguous(&j, &after, deliver, &r)
           == MULIB_BROWSER_REPLAY_COMPLETE && c.calls == 4);
    after = 5;
    assert(mulib_browser_replay_run_contiguous(&j, &after, deliver, &r)
           == MULIB_BROWSER_REPLAY_INVALID && after == 5 && c.calls == 4);
    mulib_browser_journal_clear(&j);

    j.seq = UINT64_MAX;
    after = UINT64_MAX;
    assert(mulib_browser_replay_run_contiguous(&j, &after, deliver, &r)
           == MULIB_BROWSER_REPLAY_COMPLETE);
    --after;
    assert(mulib_browser_replay_run_contiguous(&j, &after, deliver, &r)
           == MULIB_BROWSER_REPLAY_GAP && after == UINT64_MAX - 1);
    return 0;
}
