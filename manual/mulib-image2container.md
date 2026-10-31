# mulib-image2container

## NAME

Atomic materialization of image bytes into owned container metadata and rootfs.

## INTEGRATION

Define `MULIB_IMPL_IMAGE2CONTAINER` and transitive `imgformat`/`rootfs` switches.
Example: `examples/modules/image2container.c`.

## IMPORT

`open(image, length, replacement_id, limits, out, detail)` validates the image
envelope, decodes the process spec, optionally substitutes a new validated ID,
initializes a CREATED container, then imports the USTAR root. It owns all resulting
paths, file bytes and strings; callers may release input immediately on success.

NULL limits selects defaults: 128 MiB image, 4096 nodes, 16 MiB per file, 64 MiB
aggregate file bytes. Aggregate archive bytes are preflighted before materializing
filesystem storage. Node limits include synthesized parent directories but not
the implicit root. File/directory syntax, duplicates, links, whiteouts and archive
checksums follow rootfs import contracts.

Output must be uninitialized. Failure cleans up temporary owners and leaves
output unchanged. detail is initialized to zero and receives a VFS result when
rootfs import fails. The object records architecture and the image
payload CRC for metadata; neither authenticates the producer.

## CLONING AND RELEASE

`clone(source, new_id, out)` deep-copies rootfs and process metadata, starting a
fresh CREATED container at generation one. It does not copy runtime handles,
execution state, mounts or open transport connections. The source can remain
live; externally synchronize its filesystem during cloning.

`free` releases container metadata and rootfs and resets the owner. First close
any runtime and unexport any mount borrowing its components. Do not shallow-copy
live materialized images.

## RESULTS

Common CT errors distinguish bad arguments, invalid image/config, corruption,
allocation and quotas. Native import detail may be more specific than the common
EFORMAT result. Allocation errors from rootfs import/clone remain ENOMEM, and
quota failures remain ELIMIT. Architecture is a routing label; importing never starts code or
extracts into the host filesystem. Use [containterpool](mulib-containterpool.md)
or [containerd](mulib-containerd.md) to prepare and manage an execution backend.
