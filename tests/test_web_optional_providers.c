#include <assert.h>

#include "mulib-web-clipboard.h"
#include "mulib-web-device.h"
#include "mulib-web-geolocation.h"
#include "mulib-web-mediacap.h"
#include "mulib-web-mediacodec.h"
#include "mulib-web-notification.h"
#include "mulib-web-webgl.h"
#include "mulib-web-webgpu.h"

/* Negative controls: a tripwire records dispatch, then fails. None of these
 * callbacks produces a capability result, device reply, or other output. */
static int called(void *context)
{
    ++*(unsigned *)context;
    return 0;
}

static int clipboard(void *context, muba_span text)
{
    (void)text;
    return called(context);
}

static int device(void *context, uint64_t handle, unsigned op,
                  muba_span request, muba_buf *reply)
{
    (void)handle;
    (void)op;
    (void)request;
    (void)reply;
    return called(context);
}

static int geolocation(void *context, mulib_web_geolocation_position *position)
{
    (void)position;
    return called(context);
}

static int mediacap(void *context, muba_span codec, unsigned width,
                    unsigned height)
{
    (void)codec;
    (void)width;
    (void)height;
    return called(context);
}

static int mediacodec(void *context, int64_t timestamp, int key, muba_span chunk)
{
    (void)timestamp;
    (void)key;
    (void)chunk;
    return called(context);
}

static int notification(void *context, muba_span title, muba_span body)
{
    (void)title;
    (void)body;
    return called(context);
}

static int webgl(void *context, unsigned target, muba_span bytes)
{
    (void)target;
    (void)bytes;
    return called(context);
}

static int webgpu(void *context, uint64_t buffer, size_t offset, muba_span bytes)
{
    (void)buffer;
    (void)offset;
    (void)bytes;
    return called(context);
}

int main(void)
{
    const muba_span bytes = muba_str("test");
    const muba_span invalid = {NULL, 1};
    mulib_web_geolocation_position position = {1, 2, 3};
    muba_buf reply = {0};
    assert(muba_append(&reply, bytes));
    const muba_buf original = reply;
    unsigned calls = 0;

    /* Valid arguments and (where applicable) explicit permission do not
     * substitute for a missing provider. */
    assert(!mulib_web_clipboard_write(1, NULL, &calls, bytes, bytes.len));
    assert(!mulib_web_device_call(1, NULL, &calls, 1, 0, bytes, &reply));
    assert(!mulib_web_geolocation_get(1, NULL, &calls, &position));
    assert(!mulib_web_mediacap_supports(NULL, &calls, bytes, 1, 1));
    assert(!mulib_web_mediacodec_submit(NULL, &calls, 0, 0, bytes, bytes.len));
    assert(!mulib_web_notification_show(1, NULL, &calls, bytes, bytes));
    assert(!mulib_web_webgl_buffer(NULL, &calls, 0x8892, bytes, bytes.len));
    assert(!mulib_web_webgpu_write(NULL, &calls, 1, bytes.len, 0, bytes));

    /* For flag-gated entry points, a populated callback does not override
     * denial. The flag is trusted caller data, not a browser credential. */
    assert(!mulib_web_clipboard_write(0, clipboard, &calls, bytes, bytes.len));
    assert(!mulib_web_device_call(0, device, &calls, 1, 0, bytes, &reply));
    assert(!mulib_web_geolocation_get(0, geolocation, &calls, &position));
    assert(!mulib_web_notification_show(0, notification, &calls, bytes, bytes));
    assert(calls == 0);

    /* Check representative pre-dispatch validation on each adapter. */
    assert(!mulib_web_clipboard_write(1, clipboard, &calls, bytes, bytes.len - 1));
    assert(!mulib_web_device_call(1, device, &calls, 0, 0, bytes, &reply));
    assert(!mulib_web_geolocation_get(1, geolocation, &calls, NULL));
    assert(!mulib_web_mediacap_supports(mediacap, &calls, bytes, 0, 1));
    assert(!mulib_web_mediacodec_submit(mediacodec, &calls, 0, 0, bytes,
                                       bytes.len - 1));
    assert(!mulib_web_notification_show(1, notification, &calls, bytes, invalid));
    assert(!mulib_web_webgl_buffer(webgl, &calls, 0, bytes, bytes.len));
    assert(!mulib_web_webgpu_write(webgpu, &calls, 1, bytes.len, 4, bytes));
    assert(calls == 0);
    assert(reply.data == original.data && reply.len == original.len &&
           reply.cap == original.cap && !memcmp(reply.data, bytes.data, bytes.len));
    assert(position.latitude == 1 && position.longitude == 2 && position.accuracy == 3);

    /* Prove the tripwires are reachable and callback rejection propagates,
     * distinguishing "no dispatch" from "dispatched and rejected". */
    assert(!mulib_web_clipboard_write(1, clipboard, &calls, bytes, bytes.len));
    assert(calls == 1);
    assert(!mulib_web_device_call(1, device, &calls, 1, 0, bytes, &reply));
    assert(calls == 2);
    assert(!mulib_web_geolocation_get(1, geolocation, &calls, &position));
    assert(calls == 3);
    assert(!mulib_web_mediacap_supports(mediacap, &calls, bytes, 1, 1));
    assert(calls == 4);
    assert(!mulib_web_mediacodec_submit(mediacodec, &calls, 0, 0, bytes, bytes.len));
    assert(calls == 5);
    assert(!mulib_web_notification_show(1, notification, &calls, bytes, bytes));
    assert(calls == 6);
    assert(!mulib_web_webgl_buffer(webgl, &calls, 0x8892, bytes, bytes.len));
    assert(calls == 7);
    assert(!mulib_web_webgpu_write(webgpu, &calls, 1, bytes.len, 0, bytes));
    assert(calls == 8);
    assert(reply.data == original.data && reply.len == original.len &&
           reply.cap == original.cap && !memcmp(reply.data, bytes.data, bytes.len));
    assert(position.latitude == 1 && position.longitude == 2 && position.accuracy == 3);

    muba_free(&reply);
    return 0;
}
