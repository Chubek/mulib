#define MULIB_IMPL_BUFFER
#define MULIB_IMPL_BYCUTILS
#define MULIB_IMPL_CRC32
#define MULIB_IMPL_STRBUF
#define MULIB_IMPL_UTF8
#define MULIB_IMPL_PARSEC
#define MULIB_IMPL_JSON
#define MULIB_IMPL_ARCHIVE
#define MULIB_IMPL_VFS
#define MULIB_IMPL_ROOTFS
#define MULIB_IMPL_ROOTFSBUILDER
#define MULIB_IMPL_OCIIMAGE
#define MULIB_IMPL_OCIIMAGEBUILDER
#define MULIB_IMPL_OCICONTAINER
#define MULIB_IMPL_OCICONTAINERVFS
#define MULIB_IMPL_OCICONTAINERVOLUME
#define MULIB_IMPL_OCICONTAINERNETWORK
#define MULIB_IMPL_OCICONTAINERNETWRORK
#define MULIB_IMPL_INET4
#define MULIB_IMPL_OCICONTAINERMETADATA
#define MULIB_IMPL_CONTAINER2DAEMONPROTO
#define MULIB_IMPL_OCICONTAINERIPC
#define MULIB_IMPL_CONTAINERSTUB
#define MULIB_IMPL_CONTAINERINJECTOR
#define MULIB_IMPL_OCICONTAINERRUNTIME
#define MULIB_IMPL_CONTAINERSERVERD
#define MULIB_IMPL_IMGTRACKERD
#define MULIB_IMPL_MEMTABLE
#include "mulib-containerinjector.h"
#include "mulib-containerserverd.h"
#include "mulib-containerstub.h"
#include "mulib-imgtrackerd.h"
#include "mulib-ocicontaineripc.h"
#include "mulib-ocicontainernetwrork.h"
#include "mulib-ocicontainervfs.h"
#include "mulib-ocicontainervolume.h"
#include "mulib-ociimagebuilder.h"
#include "mulib-rootfsbuilder.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>
static int
backend (void *ctx, const mulib_ocicontainer_t *c, int next, int code)
{
  (void)c;
  (void)next;
  (void)code;
  return *(int *)ctx;
}
static void
rootfs (void)
{
  mulib_rootfs_t root, loaded;
  mulib_buffer_t tar = { 0 };
  mulib_vfs router;
  char data[8];
  size_t got;
  assert (!mulib_rootfs_init (&root, 16, 1024));
  assert (!mulib_vfs_mem_mkdir (&root.storage, "/etc"));
  assert (!mulib_containerinjector_put (&root, "/etc/config", "abc", 3));
  assert (!mulib_rootfsbuilder_export (&root, &tar));
  assert (!mulib_rootfs_import (tar.data, tar.length, 16, 1024, &loaded));
  assert (!mulib_vfs_init (&router, 2));
  assert (!mulib_ocicontainervfs_mount (&router, "/", &loaded, 1));
  assert (
      !mulib_vfs_read (&router, "/etc/config", 0, data, sizeof (data), &got)
      && got == 3 && !memcmp (data, "abc", 3));
  assert (mulib_vfs_write (&router, "/etc/config", 0, "x", 1, &got)
          == MULIB_VFS_EROFS);
  mulib_vfs_free (&router);
  mulib_rootfs_free (&loaded);
  assert (mulib_rootfs_import (tar.data, tar.length - 1, 16, 1024, &loaded)
          < 0);
  assert (mulib_rootfs_import (tar.data, tar.length, 1, 1024, &loaded) < 0);
  tar.data[148] ^= 1;
  assert (mulib_rootfs_import (tar.data, tar.length, 16, 1024, &loaded) < 0);
  mulib_rootfs_free (&root);
  mulib_buffer_free (&tar);
}

static void
rootfs_conflicts (void)
{
  mulib_archive_writer_t archive;
  mulib_rootfs_t root = { 0 };
  mulib_archive_writer_init (&archive);
  /* An explicit directory after its implicit creation is allowed once. */
  assert (!mulib_archive_add (&archive, "dir/file", "x", 1, 0644, 0));
  assert (!mulib_archive_add_directory (&archive, "dir/", 0755, 0));
  assert (!mulib_archive_finish (&archive));
  assert (!mulib_rootfs_import (archive.bytes.data, archive.bytes.length, 4, 4,
                                &root));
  assert (root.storage.count == 2);
  mulib_rootfs_free (&root);
  mulib_archive_writer_free (&archive);

  mulib_archive_writer_init (&archive);
  assert (!mulib_archive_add_directory (&archive, "dir/", 0755, 0));
  assert (!mulib_archive_add_directory (&archive, "dir", 0755, 0));
  assert (!mulib_archive_finish (&archive));
  assert (mulib_rootfs_import (archive.bytes.data, archive.bytes.length, 4, 4,
                               &root)
          < 0);
  assert (!root.storage.nodes && !root.storage.count);
  mulib_archive_writer_free (&archive);

  mulib_archive_writer_init (&archive);
  assert (!mulib_archive_add (&archive, "dir", "x", 1, 0644, 0));
  assert (!mulib_archive_add (&archive, "dir/file", "x", 1, 0644, 0));
  assert (!mulib_archive_finish (&archive));
  assert (mulib_rootfs_import (archive.bytes.data, archive.bytes.length, 4, 4,
                               &root)
          < 0);
  assert (!root.storage.nodes);
  mulib_archive_writer_free (&archive);
}
static void
images (void)
{
  const char *digest
      = "sha256:"
        "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef";
  mulib_json_t *config, *layer, *manifest, *parsed;
  char *text;
  mulib_imgtrackerd_t tracker = { 0 };
  assert (mulib_ociimage_digest (digest)
          && !mulib_ociimage_digest ("sha256:bad"));
  config = mulib_ociimagebuilder_descriptor (
      "application/vnd.oci.image.config.v1+json", digest, 3);
  layer = mulib_ociimagebuilder_descriptor (
      "application/vnd.oci.image.layer.v1.tar", digest, 1024);
  assert (config && layer);
  manifest = mulib_ociimagebuilder_create (config);
  assert (manifest);
  assert (mulib_ociimagebuilder_layer (manifest, layer)
          && mulib_ociimage_validate (manifest));
  text = mulib_json_stringify (manifest, 0);
  assert (text);
  parsed = mulib_ociimage_parse (text, strlen (text), NULL);
  assert (parsed);
  free (text);
  mulib_json_destroy (parsed);
  mulib_json_destroy (config);
  mulib_json_destroy (layer);
  mulib_json_destroy (manifest);
  assert (!mulib_ociimage_parse ("{}", 2, NULL));
  assert (!mulib_imgtrackerd_retain (&tracker, digest)
          && !mulib_imgtrackerd_retain (&tracker, digest));
  assert (mulib_imgtrackerd_count (&tracker, digest) == 2);
  assert (!mulib_imgtrackerd_release (&tracker, digest)
          && !mulib_imgtrackerd_release (&tracker, digest));
  assert (mulib_imgtrackerd_release (&tracker, digest) == 1);
  mulib_imgtrackerd_free (&tracker);
}
static void
runtime (void)
{
  mulib_ocicontainerruntime_t r;
  mulib_buffer_t request = { 0 }, reply = { 0 }, ipc = { 0 };
  int fail = 0;
  mulib_container2daemonproto_frame_t f;
  const void *payload;
  size_t n, used, i, old;
  uint64_t channel;
  mulib_ocicontainervolume_t volume;
  mulib_ocicontainernetwrork_t network;
  assert (!mulib_ocicontainerruntime_init (&r, "../bad", backend, &fail));
  assert (mulib_ocicontainerruntime_init (&r, "test", backend, &fail));
  assert (mulib_containerstub_request (&request, 1, 55, "test"));
  for (i = 0; i < request.length; ++i)
    assert (mulib_container2daemonproto_decode (request.data, i, &f, &used)
            == 0);
  fail = -99;
  assert (mulib_containerserverd_dispatch (&r, request.data, request.length,
                                           &reply)
          == -99);
  assert (!reply.length && r.container.state == MULIB_OCICONTAINER_CREATED);
  fail = 0;
  assert (!mulib_containerserverd_dispatch (&r, request.data, request.length,
                                            &reply));
  assert (r.container.state == MULIB_OCICONTAINER_RUNNING);
  assert (
      mulib_container2daemonproto_decode (reply.data, reply.length, &f, &used)
          == 1
      && f.opcode == 129 && f.request_id == 55);
  old = reply.length;
  assert (mulib_containerserverd_dispatch (&r, request.data, request.length,
                                           &reply)
              < 0
          && reply.length == old);
  request.length = 0;
  assert (mulib_containerstub_request (&request, 2, 56, "test"));
  assert (!mulib_containerserverd_dispatch (&r, request.data, request.length,
                                            &reply));
  assert (r.container.state == MULIB_OCICONTAINER_STOPPED);
  request.data[20] ^= 1;
  assert (mulib_containerserverd_dispatch (&r, request.data, request.length,
                                           &reply)
          < 0);
  assert (mulib_ocicontaineripc_send (&ipc, 8, "a\0b", 3));
  assert (mulib_ocicontaineripc_receive (ipc.data, ipc.length, &channel,
                                         &payload, &n, &used)
              == 1
          && channel == 8 && n == 3);
  assert (!memcmp (payload, "a\0b", 3));
  ipc.data[ipc.length - 1] ^= 1;
  assert (mulib_ocicontaineripc_receive (ipc.data, ipc.length, &channel,
                                         &payload, &n, &used)
          < 0);
  assert (mulib_ocicontainervolume_init (&volume, "/host/a", "/data", 1));
  assert (!mulib_ocicontainervolume_init (&volume, "/host", "/", 0));
  assert (
      mulib_ocicontainernetwrork_init (&network, "10.0.0.2", 24, "10.0.0.1"));
  assert (
      !mulib_ocicontainernetwrork_init (&network, "10.0.0.2", 24, "10.1.0.1"));
  mulib_buffer_free (&request);
  mulib_buffer_free (&reply);
  mulib_buffer_free (&ipc);
}
int
main (void)
{
  rootfs ();
  rootfs_conflicts ();
  images ();
  runtime ();
  return 0;
}
