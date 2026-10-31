/* Example-only implementation TU support. Applications define just the
 * transitive switches they use; this fixture intentionally demonstrates the
 * complete portable stack in one translation unit. */
#ifndef MULIB_EXAMPLE_CONTAINER_STACK_COMMON_H
#define MULIB_EXAMPLE_CONTAINER_STACK_COMMON_H
#define MULIB_IMPL_CONTAINER
#define MULIB_IMPL_ROOTFS
#define MULIB_IMPL_VFS
#define MULIB_IMPL_ARCHIVE
#define MULIB_IMPL_BUFFER
#define MULIB_IMPL_ROOTFS_BUILDSPEC
#define MULIB_IMPL_IMGFORMAT
#define MULIB_IMPL_CRC32
#define MULIB_IMPL_IMGBUILDSPEC
#define MULIB_IMPL_IMGBUILDER
#define MULIB_IMPL_ROOTFSBUILDER
#define MULIB_IMPL_IMAGE2CONTAINER
#define MULIB_IMPL_CONTAINER_RUNTIME
#define MULIB_IMPL_CONTAINERBOOST
#define MULIB_IMPL_CONTAINER_QVMBOOST
#define MULIB_IMPL_CONTAINER_IPC
#define MULIB_IMPL_CONAINERD_IPC
#define MULIB_IMPL_CONTAINER_LOG
#define MULIB_IMPL_CONTAINER_NETDEV
#define MULIB_IMPL_CONTAINER_NETWORK
#define MULIB_IMPL_PACKETRULES
#define MULIB_IMPL_PACKETFITER
#define MULIB_IMPL_CONTAINER_VFS
#define MULIB_IMPL_FUSE
#define MULIB_IMPL_CONTAINERD_CACHE
#define MULIB_IMPL_CONTAINERD_MOUNT
#define MULIB_IMPL_CONTAINTERPOOL
#define MULIB_IMPL_CONTAINERD_ACCESSSTUB
#define MULIB_IMPL_CONTAINERD
#define MULIB_IMPL_CONTAINERD_CTRLBRIDGE
#define MULIB_IMPL_CONTAINER_STUB
#define MULIB_IMPL_CONTAINER_PACKELF
#define MULIB_IMPL_ELF
#define MULIB_IMPL_ENDIAN
#define MULIB_IMPL_CONTAINER_PACKZSTD
#include "mulib-imgbuilder.h"
#include "mulib-container-qvmboost.h"
#include "mulib-containerd-ctrlbridge.h"
#include "mulib-container-stub.h"
#include "mulib-container-network.h"
#include "mulib-container-packelf.h"
#include "mulib-container-packzstd.h"
#include <assert.h>

static inline int
example_container_image (mulib_ct_blob *out)
{
  mulib_imgbuildspec spec;
  int rc = mulib_imgbuildspec_init (&spec, "demo");
  if (rc) return rc;
  rc = mulib_container_spec_arg (&spec.process, "/init");
  if (!rc) rc = mulib_container_spec_env (&spec.process, "MODE", "demo");
  if (!rc) rc = mulib_imgbuildspec_file (&spec, "/etc/message", "hello\n", 6);
  if (!rc) rc = mulib_imgbuilder_build (&spec, NULL, out, NULL);
  mulib_imgbuildspec_free (&spec);
  return rc;
}
static inline void
example_container_qvm (mulib_container_qvmboost *config)
{
  static const mulib_qvm_instruction code[] = {
    { MULIB_QVM_CONST, 0, 0, 0, 42 }, { MULIB_QVM_HALT, 0, 0, 0, 0 }
  };
  mulib_container_qvmboost_init (config, code, sizeof (code) / sizeof (*code));
  config->memory_bytes = 64;
}
static inline int
example_container_daemon (mulib_containerd *daemon, mulib_container_qvmboost *config)
{
  mulib_containerd_options options = mulib_containerd_default_options ();
  mulib_containerboost_backend backend;
  int rc;
  options.containers = 4; options.cache_entries = 4; options.backends = 4;
  options.log_records = 16; options.log_bytes = 4096; options.log_message_bytes = 256;
  example_container_qvm (config);
  rc = mulib_containerd_init (daemon, &options);
  if (!rc) rc = mulib_container_qvmboost_backend (config, "qvm", 10, &backend);
  if (!rc) rc = mulib_containerboost_register (&daemon->backends, &backend);
  if (rc) (void)mulib_containerd_free (daemon, 0);
  return rc;
}
#endif
