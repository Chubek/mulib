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
#include "../examples/distrib-common.h"
#include "mulib-distribcluster.h"
typedef struct backend { unsigned starts, signals, polls, destroyed; } backend;
static int prepare(void *u, const mulib_container_spec *s, const mulib_rootfs_t *r, void **h) {
  (void)s; (void)r; *h=u; return 0;
}
static int start(void *h) { ++((backend *)h)->starts; return 0; }
static int poll_backend(void *h, size_t budget, int *code) {
  assert(budget==1); ++((backend *)h)->polls; *code=0; return MULIB_CT_DONE;
}
static int signal_backend(void *h, unsigned signal) {
  assert(signal==15); ++((backend *)h)->signals; return 0;
}
static int destroy_backend(void *h) { ++((backend *)h)->destroyed; return 0; }
static uint64_t clock_ms(void *p) { return *(uint64_t *)p; }
static int vm_step(void *p, mulib_vmm_exit_t *e) { ++*(unsigned *)p; e->kind=MULIB_VMM_EXIT_HALT; return 0; }
static int vm_reset(void *p) { (void)p; return 0; }
static void action(mulib_distribcluster_t *c, const char *kind, const char *a, const char *id) {
  mulib_strbuf_t out; mulib_strbuf_init(&out);
  assert(!mulib_distribcluster_execute(c,kind,a,id,strlen(id),&out)); assert(out.length);
  mulib_strbuf_free(&out);
}
int main(void) {
  mulib_distribcluster_t registry;
  mulib_container_spec spec; mulib_container container; mulib_container_runtime runtime;
  mulib_container_runtime_ops ops={MULIB_CONTAINER_CAP_SIGNAL,prepare,start,poll_backend,signal_backend,NULL,destroy_backend};
  backend b={0}; uint64_t now=1;
  mulib_ocicontainer_t oci;
  mulib_vmm_t vm=MULIB_VMM_INIT; mulib_vmm_options_t vo={0}; unsigned calls=0;
  unsigned char ram[4096]={0}; mulib_vmmdev_region_t region={0,sizeof ram,ram,3};
  mulib_strbuf_t out;
  assert(!mulib_container_spec_init(&spec,"web")&&!mulib_container_init(&container,&spec));
  assert(!mulib_container_runtime_init(&runtime,&container,NULL,&ops,&b));
  assert(mulib_ocicontainer_create(&oci,"app"));
  vo.ram.regions=&region; vo.ram.count=1; vo.executor.step=vm_step; vo.executor.reset=vm_reset; vo.executor.context=&calls;
  assert(!mulib_vmm_init(&vm,&vo));
  mulib_distribcluster_init(&registry,clock_ms,&now);
  assert(!mulib_distribcluster_container(&registry,"web",&runtime));
  assert(!mulib_distribcluster_oci(&registry,"app",&oci));
  assert(!mulib_distribcluster_vmm(&registry,"guest",&vm));
  assert(mulib_distribcluster_oci(&registry,"web",&oci)==-1);
  action(&registry,"container","start","web"); action(&registry,"container","start","web"); assert(b.starts==1);
  action(&registry,"container","stop","web"); assert(b.signals==1);
  action(&registry,"container","step","web"); assert(b.polls==1&&container.state==MULIB_CONTAINER_EXITED);
  action(&registry,"container","close","web"); action(&registry,"container","close","web");
  action(&registry,"container","status","web"); assert(b.destroyed==1&&container.state==MULIB_CONTAINER_DELETED);
  action(&registry,"ocicontainer","start","app"); action(&registry,"ocicontainer","start","app");
  action(&registry,"ocicontainer","stop","app"); action(&registry,"ocicontainer","close","app");
  action(&registry,"vmm","start","guest"); action(&registry,"vmm","start","guest");
  action(&registry,"vmm","step","guest"); assert(calls==1&&vm.state==MULIB_VMM_HALTED);
  action(&registry,"vmm","reset","guest"); action(&registry,"vmm","close","guest");
  mulib_strbuf_init(&out);
  assert(mulib_distribcluster_execute(&registry,"container","start","web",3,&out)<0);
  assert(mulib_distribcluster_execute(&registry,"vmm","start","missing",7,&out)<0);
  assert(mulib_distribcluster_execute(&registry,"ocicontainer","shell","app",3,&out)<0);
  mulib_strbuf_free(&out); mulib_container_free(&container); mulib_container_spec_free(&spec);
  return 0;
}
