# GLX Vulkan: headless device and transfer API

`kernels/mulib-glx-vulkan.h` is a C11, header-only Linux helper. The original
`mulib_glx_vulkan_t` and `open`/`symbol`/`close` functions still work without
Vulkan SDK headers. They load `libvulkan.so.1` at runtime and do not create an
instance. Close a successful loader before opening it again.

Define `MULIB_GLX_VULKAN_ENABLE` **before the first inclusion** to expose the
typed API. It uses the official Khronos `<vulkan/vulkan_core.h>` declarations;
no copied Vulkan structure layouts or direct Vulkan linkage are needed. Link
`-ldl` where required. `VK_NO_PROTOTYPES` is optional; the wrapper neither
requires nor defines it. Only applications that use the typed API need
development headers; only applications that run Vulkan need a runtime loader
and an ICD (a hardware or software Vulkan driver).

## Device setup

1. Zero-initialize a `mulib_glx_vulkan_instance_t`, then call
   `mulib_glx_vulkan_instance_open`. With `NULL` create info it requests
   Vulkan 1.0, with no layers, extensions, surfaces or display connection.
   Pass a native `VkInstanceCreateInfo` to request other versions, layers or
   extensions. `VK_ERROR_INCOMPATIBLE_DRIVER` can also indicate an unavailable
   runtime loader; an incomplete loader/dispatch is an initialization error.
2. Read available devices with `mulib_glx_vulkan_physical_devices` and queues
   with `mulib_glx_vulkan_queue_families`, using Vulkan's count/capacity
   convention. Only the physical-device call can return `VK_INCOMPLETE` on
   truncation. Use `instance.GetPhysicalDeviceProperties` for device names,
   types and limits. Alternatively, `mulib_glx_vulkan_pick_queue` selects the
   first device/family whose queue flags meet the requirements (graphics and
   compute imply transfer); it does not rank GPUs or validate extensions.
   It retries an incomplete device list up to four total attempts and returns
   `VK_ERROR_FEATURE_NOT_PRESENT` if no family matches.
3. Call `mulib_glx_vulkan_device_open` with a physical device obtained from
   that instance and a family index. It creates one unprotected queue, index
   zero, at priority `1.0`. Optional `mulib_glx_vulkan_device_options_t`
   supplies native device extensions, features and `pNext`. Callers must
   satisfy Vulkan validity rules for native inputs and negotiate support
   before requesting non-core capabilities.
4. Read `device.handle`, `device.queue`, `device.queue_family`, `device.memory`
   and `device.vk` as borrowed handles/properties/dispatch. The
   `mulib_glx_vulkan_instance_proc` and `mulib_glx_vulkan_device_proc` accessors
   resolve additional commands with the corresponding Vulkan dispatcher.
   A non-null entry does not substitute for enabling the applicable API
   version, extension or feature.

## One-queue submissions and buffers

`mulib_glx_vulkan_commands_open` creates a reusable pool, primary command
buffer and fence for the selected queue. `commands_begin` resets and begins
one-time recording. Record via the typed dispatch (or compatible resolved
commands), then `commands_submit` ends recording, resets the fence and submits
the command buffer. `commands_wait` waits in nanoseconds; use zero for polling.
`VK_TIMEOUT` leaves the submission pending. Until a wait completes, a new
recording or `commands_close` returns `VK_NOT_READY`, even if device execution
already finished. A submit that reports `VK_ERROR_DEVICE_LOST` must still
be followed by a wait for lifetime tracking; a wait that reports device loss
permits destruction, not reuse. Raw dispatch must not submit this command
buffer or reset its pool or fence.

`mulib_glx_vulkan_buffer_open` creates an exclusive, unprotected buffer with
Vulkan 1.0 usage bits. It allocates compatible memory and binds it at offset
zero. Among permitted memory types it picks the first that meets both the
required and preferred flags, then falls back to the first match for the
required flags alone. Treat `buffer.size` as the number of usable bytes; the
allocation may be larger. For host-visible memory, `buffer_map` maps the whole
allocation and returns a borrowed pointer; `buffer_unmap` and `buffer_close`
invalidate it. Flush host writes before device use; after GPU-to-host barriers
and a completed wait, invalidate before host reads. The cache helpers operate
on the whole mapped allocation and are no-ops for coherent memory. They do
not insert barriers or wait for device work.

All typed owners must start zero-initialized and stay at stable addresses
without copying. Successful close clears the owner; failed opens leave the
output unchanged. Externally synchronize all access to each owner, its children,
the queue and Vulkan dispatch. Close all command owners and buffers before
closing a device, and close all devices before closing the instance, or the
parent close returns `VK_NOT_READY`. Before destroying a buffer, ensure that
no submitted work can access it; the wrapper does not track references
recorded using the raw dispatch. Destroy separately created raw Vulkan
children before closing their parent. `device_close` waits for idle; it
still destroys the device on `VK_ERROR_DEVICE_LOST` and returns that error.

The wrapper does not provide WSI, frame presentation, images, shader or
pipeline construction, multi-queue scheduling, external-memory import,
suballocation or device-loss recovery. Native Vulkan validation still applies
to every raw dispatch call.

## Validation

The default graphics regression gate tests the loader-only API. Run the live
test explicitly against an installed Vulkan SDK and ICD:

```sh
make check-vulkan
sh tests/run-vulkan.sh
CC=clang SANITIZE=1 sh tests/run-vulkan.sh
```

For an out-of-tree SDK, pass `VULKAN_CPPFLAGS=-I/path/to/include` to Make or
`CPPFLAGS=-I/path/to/include` to the runner. In a native Linux CMake build,
enable `MULIB_BUILD_VULKAN_TESTS`, build the `mulib_test_vulkan_device` target,
and run it through CTest. Optionally set `VK_DRIVER_FILES` to select an ICD.

The [loader-only fixture](../tests/graphics/vulkan-legacy.c) needs no SDK
headers or Vulkan linkage. The [typed fixture](../tests/graphics/vulkan-device.c)
is a worked example of instance/device creation, queue selection, buffer
allocation, mapping and two command submissions through a device-local
buffer. After fence waits it compares every byte copied back to the CPU.
It also checks bad inputs, missing extensions, buffer allocation rollback,
command-state errors and teardown ordering. Small dispatch stubs additionally
check timeout, device-loss and noncoherent cache-range handling without
forcing those conditions on the live device. The test needs no display and
does not take DRM ownership.
Fixture exit `77` means no usable Vulkan loader/ICD or no enumerated physical
devices; missing SDK headers are a build error for this explicitly requested
test. Other Vulkan errors and incomplete dispatch tables fail the test.

When `glslangValidator` is available, `MULIB_BUILD_VULKAN_TESTS=ON` also builds
the `mulib_test_vulkan_compute` target and verifies the shader's storage-buffer
writes on a real device. That fixture uses native pipeline and descriptor calls
resolved through the wrapper; those objects remain caller-owned.
