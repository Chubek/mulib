# mulib-container-oixboost

## NAME AND INTEGRATION

Compile rootfs files into OIX guests and run them through container lifecycle APIs.
Define `MULIB_IMPL_CONTAINER_OIXBOOST` and the `containerboost`/`oix` closures.
OIX's parser requires the additional `MULIB_IMPL_PARSER` switch; complete switches
are in `examples/container-native-common.h` and [stack](container-stack.md).
Example: `examples/modules/container-oixboost.c`.

## COMPILATION

`compile(root,config,out,error)` copies regular rootfs files into the OIX compiler
configuration and uses caller task/banner settings. config must not already have
files; directory records are omitted. OIX's 64-file, filename, task and native
image limits remain authoritative. Output is an initially empty OIX-owned image
released with `mulib_oix_image_free`, not `mulib_ct_blob_free`.

Compilation is portable and requires no KVM access. Programs and guest behavior
follow [mulib-oix](mulib-oix.md): a small standalone native guest OS with explicit
tasks, console operations and embedded read-only files.

## RUNTIME CONFIGURATION

`init(adapter,image,length)` borrows a compiled image and defaults to one million
KVM exits. Set output callback/context for console bytes. Prepare validates and
copies image bytes, creates private OIX RAM/vCPU state, and probes actual KVM
availability. Each prepared container has an independent guest.

`backend` produces an OIX-architecture descriptor after structural image
validation. Start uses the prepared VM; restart constructs a replacement before
destroying the previous VM. Poll runs bounded exits and preserves state on
LIMIT/INTERRUPTED/AGAIN. Halt returns the guest exit code. Exit quota exhaustion
returns 124; signals 9/15 return 137/143; pause preserves guest state.

`feed(runtime,bytes,length,&written)` copies console input into a started guest.
Partial acceptance returns AGAIN with the accepted count. It rejects stopped,
foreign-adapter or busy runtimes. Output callbacks borrow bytes during the call.

## LIMITS AND RESULTS

Guest memory is the OIX fixed 64 KiB. Smaller requested memory limits, CPU,
process or open-file requests return ENOTSUP. Other process fields are metadata;
runtime does not recompile argv/env into guest tasks. Native execution requires
Linux x86 KVM. A single KVM_RUN may block independently of its exit budget.

Common CT errors describe wrapper failures; compilation's optional OIX diagnostic
provides detailed compiler information. Close destroys VM and copied image.
Tests compile/validate rootfs-based native images without requiring guest execution.
