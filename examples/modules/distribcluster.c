#define MULIB_IMPL_DISTRIBCLUSTER
#define MULIB_IMPL_CONTAINER_RUNTIME
#define MULIB_IMPL_CONTAINER
#define MULIB_IMPL_ROOTFS
#define MULIB_IMPL_ARCHIVE
#define MULIB_IMPL_BUFFER
#define MULIB_IMPL_VFS
#define MULIB_IMPL_OCICONTAINER
#define MULIB_IMPL_VMM
#define MULIB_IMPL_VMM_MEMORY
#define MULIB_IMPL_VMMDEV
#define MULIB_IMPL_VIRTIO_OS
#define MULIB_IMPL_VIRTIO
#define MULIB_IMPL_ENDIAN
#include "../distrib-common.h"
#include "mulib-distribcluster.h"
int main(void) {
  mulib_distribcluster_t cluster;
  mulib_ocicontainer_t oci;
  mulib_strbuf_t out;
  mulib_distribcluster_init(&cluster,NULL,NULL);
  assert(mulib_ocicontainer_create(&oci,"app"));
  assert(!mulib_distribcluster_oci(&cluster,"app",&oci));
  mulib_strbuf_init(&out);
  /* Pass this callback and &cluster in the worker options to act over MuMQ. */
  assert(!mulib_distribcluster_execute(&cluster,"ocicontainer","start","app",3,&out));
  assert(oci.state==MULIB_OCICONTAINER_RUNNING);
  mulib_strbuf_free(&out); return 0;
}
