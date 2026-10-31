/* End-to-end portable container contracts, hostile wire input and ownership. */
#include "../examples/container-stack-common.h"
#include <stdio.h>

static void specifications (void)
{
  mulib_container_spec s, copy;
  mulib_container c;
  mulib_ct_blob wire = { 0 };
  assert (mulib_container_spec_init (&s, "../bad") == MULIB_CT_EINVAL);
  assert (!mulib_container_spec_init (&s, "sample"));
  assert (!mulib_container_spec_arg (&s, ""));
  assert (!mulib_container_spec_arg (&s, "/init"));
  assert (!mulib_container_spec_env (&s, "FOO", "a=b"));
  assert (!mulib_container_spec_env (&s, "FOO", "replaced"));
  assert (s.envc == 1 && !strcmp (mulib_container_spec_getenv (&s, "FOO"), "replaced"));
  assert (mulib_container_spec_env (&s, "9BAD", "x") == MULIB_CT_EINVAL);
  assert (mulib_container_spec_cwd (&s, "/a/../b") == MULIB_CT_EINVAL);
  assert (!mulib_container_spec_cwd (&s, "/work"));
  assert (!mulib_container_spec_clone (&s, &copy));
  assert (copy.argv != s.argv && copy.argv[0] != s.argv[0]);
  mulib_container_spec_free (&copy);
  assert (!mulib_imgformat_spec_encode (&s, &wire, 4096));
  assert (!mulib_imgformat_spec_decode (wire.data, wire.length, &copy));
  assert (!strcmp (copy.cwd, "/work") && copy.argc == 2 && copy.envc == 1);
  mulib_container_spec_free (&copy);
  assert (mulib_imgformat_spec_decode (wire.data, wire.length - 1, &copy) < 0);
  assert (!mulib_container_init (&c, &s));
  assert (mulib_container_transition (&c, MULIB_CONTAINER_PAUSED, 1, 0) == MULIB_CT_ESTATE);
  assert (!mulib_container_transition (&c, MULIB_CONTAINER_RUNNING, 1, 0));
  assert (!mulib_container_transition (&c, MULIB_CONTAINER_PAUSED, 2, 0));
  assert (!mulib_container_transition (&c, MULIB_CONTAINER_RUNNING, 3, 0));
  assert (c.starts == 1);
  assert (!mulib_container_transition (&c, MULIB_CONTAINER_EXITED, 4, 7));
  assert (c.exit_code == 7 && c.exited_at == 4);
  s.max_strings = s.argc + s.envc;
  assert (mulib_container_spec_arg (&s, "one-too-many") == MULIB_CT_ELIMIT);
  mulib_container_free (&c); mulib_container_spec_free (&s); mulib_ct_blob_free (&wire);
}

static void filesystem (void)
{
  mulib_rootfs_t root, checkpoint;
  mulib_rootfs_buildspec recipe;
  mulib_container_vfs fs;
  mulib_vfs_stat stat;
  size_t failed, n;
  int detail;
  char data[16];
  assert (!mulib_rootfs_init (&root, 32, 128));
  assert (!mulib_rootfs_put (&root, "/etc/original", "abc", 3, 1));
  assert (!mulib_rootfs_buildspec_init (&recipe, 8, 1024, 128));
  assert (!mulib_rootfs_buildspec_add (&recipe, MULIB_ROOTFS_BUILD_FILE,
      "/new", NULL, "new", 3, 0));
  assert (!mulib_rootfs_buildspec_add (&recipe, MULIB_ROOTFS_BUILD_COPY,
      "/copy", "/absent", NULL, 0, 0));
  assert (mulib_rootfs_buildspec_apply (&recipe, &root, &failed, &detail) == MULIB_CT_EIO);
  assert (failed == 1 && detail == MULIB_VFS_ENOENT);
  assert (mulib_rootfs_stat (&root, "/new", &stat) == MULIB_VFS_ENOENT);
  mulib_rootfs_buildspec_free (&recipe);
  assert (!mulib_rootfs_buildspec_init (&recipe, 8, 1024, 128));
  assert (!mulib_rootfs_buildspec_add (&recipe, MULIB_ROOTFS_BUILD_COPY,
      "/copy", "/etc/original", NULL, 0, 0));
  assert (!mulib_rootfs_buildspec_apply (&recipe, &root, &failed, &detail));
  assert (failed == SIZE_MAX);
  assert (!mulib_rootfs_read (&root, "/copy", 0, data, sizeof (data), &n));
  assert (n == 3 && !memcmp (data, "abc", 3));
  assert (!mulib_container_vfs_init (&fs, &root, 4, 0));
  assert (!mulib_container_vfs_checkpoint (&fs, &checkpoint));
  assert (!mulib_rootfs_put (&root, "/copy", "changed", 7, 0));
  assert (!mulib_container_vfs_restore (&fs, &checkpoint));
  assert (!mulib_vfs_read (&fs.router, "/copy", 0, data, sizeof (data), &n) && n == 3);
  assert (mulib_container_vfs_unbind (&fs, "/") == MULIB_VFS_EINVAL);
  assert (!mulib_rootfs_remove (&root, "/etc", 1));
  assert (mulib_rootfs_stat (&root, "/etc/original", &stat) == MULIB_VFS_ENOENT);
  mulib_container_vfs_free (&fs); mulib_rootfs_free (&checkpoint);
  mulib_rootfs_buildspec_free (&recipe); mulib_rootfs_free (&root);
}

static void images (void)
{
  mulib_ct_blob a = { 0 }, b = { 0 }, packed = { 0 }, decoded = { 0 }, elf = { 0 }, overlay = { 0 };
  mulib_imgbuildspec spec;
  mulib_image2container materialized;
  mulib_imgformat_view view;
  mulib_container_packzstd_info info;
  const unsigned char *embedded;
  size_t bytes, i;
  assert (!example_container_image (&a));
  assert (!example_container_image (&b));
  assert (a.length == b.length && !memcmp (a.data, b.data, a.length));
  for (i = 0; i < 64; ++i) assert (mulib_imgformat_open (a.data, i, SIZE_MAX, &view) < 0);
  assert (!mulib_image2container_open (a.data, a.length, "second", NULL, &materialized, NULL));
  assert (!strcmp (materialized.container.spec.id, "second"));
  mulib_image2container_free (&materialized);
  {
    mulib_image2container_limits limits = mulib_image2container_default_limits ();
    limits.total_file_bytes = 5;
    assert (mulib_image2container_open (a.data, a.length, NULL, &limits, &materialized, NULL) == MULIB_CT_ELIMIT);
  }
  assert (!mulib_imgbuildspec_init (&spec, "empty"));
  assert (!mulib_imgbuilder_build (&spec, NULL, &decoded, NULL));
  assert (!mulib_image2container_open (decoded.data, decoded.length, NULL, NULL, &materialized, NULL));
  assert (materialized.rootfs.storage.count == 0);
  mulib_image2container_free (&materialized); mulib_imgbuildspec_free (&spec);
  assert (!mulib_container_packzstd_pack (a.data, a.length, 65536, 65536, &packed, &info));
  assert (!mulib_container_packzstd_unpack (packed.data, packed.length, 65536, 65536, &decoded, &info));
  assert (decoded.length == a.length && !memcmp (decoded.data, a.data, a.length));
  assert (mulib_container_packzstd_unpack (packed.data, packed.length - 1, 65536, 65536, &decoded, NULL) < 0);
  assert (mulib_container_packzstd_unpack (packed.data, packed.length, 65536, 1, &decoded, NULL) == MULIB_CT_ELIMIT);
  assert (!mulib_container_packelf_object (a.data, a.length, 62, &elf, 65536));
  assert (!mulib_container_packelf_open (elf.data, elf.length, 65536, &embedded, &bytes, &view));
  assert (bytes == a.length && !memcmp (embedded, a.data, bytes));
  assert (!mulib_container_packelf_append (elf.data, elf.length, a.data, a.length, &overlay, 65536));
  assert (!memcmp (overlay.data, elf.data, elf.length));
  assert (!mulib_container_packelf_open (overlay.data, overlay.length, 65536, &embedded, &bytes, &view));
  overlay.data[overlay.length - 1] ^= 1;
  assert (mulib_container_packelf_open (overlay.data, overlay.length, 65536, &embedded, &bytes, &view) == MULIB_CT_ECHECKSUM);
  b.data[b.length - 1] ^= 1;
  assert (mulib_imgformat_open (b.data, b.length, SIZE_MAX, &view) == MULIB_CT_ECHECKSUM);
  mulib_ct_blob_free (&a); mulib_ct_blob_free (&b); mulib_ct_blob_free (&packed);
  mulib_ct_blob_free (&decoded); mulib_ct_blob_free (&elf); mulib_ct_blob_free (&overlay);
}

static void frames (void)
{
  mulib_container_ipc_message message, parsed;
  mulib_container_ipc_reader reader;
  mulib_ct_blob wire = { 0 };
  size_t split, used, consumed;
  memset (&message, 0, sizeof (message)); message.opcode = 7;
  message.request_id = UINT64_C(0x123456789abcdef0);
  message.data = (const unsigned char *)"a\0b"; message.length = 3;
  assert (!mulib_container_ipc_encode (&message, &wire, 3));
  for (split = 0; split <= wire.length; ++split)
    {
      int rc;
      assert (!mulib_container_ipc_reader_init (&reader, 3));
      rc = mulib_container_ipc_feed (&reader, wire.data, split, &used);
      assert (used == split && rc == (split == wire.length ? MULIB_CT_DONE : MULIB_CT_AGAIN));
      assert (mulib_container_ipc_feed (&reader, wire.data + split, wire.length - split, &used) == MULIB_CT_DONE);
      assert (!mulib_container_ipc_peek (&reader, &parsed));
      assert (parsed.request_id == message.request_id && parsed.length == 3);
      assert (!memcmp (parsed.data, "a\0b", 3));
      assert (mulib_container_ipc_feed (&reader, wire.data, wire.length, &used) == MULIB_CT_DONE && !used);
      mulib_container_ipc_reader_free (&reader);
    }
  assert (mulib_container_ipc_decode (wire.data, wire.length, 2, &parsed, &consumed) == MULIB_CT_ELIMIT);
  wire.data[wire.length - 1] ^= 1;
  assert (mulib_container_ipc_decode (wire.data, wire.length, 3, &parsed, &consumed) == MULIB_CT_ECHECKSUM && !consumed);
  message.length = 0; message.data = NULL;
  assert (!mulib_container_ipc_encode (&message, &wire, 0));
  assert (!mulib_container_ipc_reader_init (&reader, 0));
  assert (mulib_container_ipc_feed (&reader, wire.data, wire.length, &used) == MULIB_CT_DONE);
  mulib_container_ipc_reader_free (&reader); mulib_ct_blob_free (&wire);
}

static void cache_and_logs (void)
{
  mulib_ct_blob image = { 0 };
  mulib_containerd_cache cache;
  mulib_containerd_cache_lease lease, stale;
  const mulib_ct_blob *borrowed;
  mulib_container_log log;
  const mulib_container_log_record *record;
  uint64_t cursor = 1, seq;
  assert (!example_container_image (&image));
  assert (!mulib_containerd_cache_init (&cache, 2, image.length * 2));
  assert (!mulib_containerd_cache_put (&cache, "a", image.data, image.length, 1, 5));
  assert (!mulib_containerd_cache_put (&cache, "b", image.data, image.length, 1, 0));
  assert (!mulib_containerd_cache_acquire (&cache, "a", 2, &lease, &borrowed));
  assert (!memcmp (borrowed->data, image.data, image.length));
  assert (mulib_containerd_cache_remove (&cache, "a") == MULIB_CT_EBUSY);
  assert (!mulib_containerd_cache_put (&cache, "c", image.data, image.length, 2, 0));
  assert (mulib_containerd_cache_acquire (&cache, "b", 2, &stale, &borrowed) == MULIB_CT_ENOENT);
  assert (mulib_containerd_cache_acquire (&cache, "a", 6, &stale, &borrowed) == MULIB_CT_ENOENT);
  assert (mulib_containerd_cache_sweep (&cache, 7) == 0);
  assert (mulib_containerd_cache_free (&cache) == MULIB_CT_EBUSY);
  stale = lease; assert (!mulib_containerd_cache_release (&cache, lease));
  assert (mulib_containerd_cache_sweep (&cache, 7) == 1);
  assert (mulib_containerd_cache_release (&cache, stale) == MULIB_CT_ESTALE);
  assert (!mulib_containerd_cache_free (&cache)); mulib_ct_blob_free (&image);
  assert (!mulib_container_log_init (&log, 2, 6, 4, 1));
  assert (!mulib_container_log_append (&log, "a", MULIB_CONTAINER_LOG_INFO, 1, 1, "abc", 3, &seq) && seq == 1);
  assert (!mulib_container_log_append (&log, "b", MULIB_CONTAINER_LOG_INFO, 1, 2, "def", 3, NULL));
  assert (!mulib_container_log_append (&log, "a", MULIB_CONTAINER_LOG_ERROR, 2, 3, "xyz", 3, NULL));
  assert (log.dropped == 1);
  assert (mulib_container_log_next (&log, &cursor, NULL, MULIB_CONTAINER_LOG_TRACE, &record) == MULIB_CT_ESTALE);
  assert (!mulib_container_log_next (&log, &cursor, "a", MULIB_CONTAINER_LOG_WARN, &record));
  assert (record->sequence == 3 && record->message.length == 3);
  assert (mulib_container_log_prune (&log, 4) == 2);
  mulib_container_log_free (&log);
}

static void packets (void)
{
  mulib_container_netdev a, b;
  mulib_container_network network;
  mulib_container_network_route route;
  unsigned char packet[28] = { 0x45, 0, 0, 28, 0, 0, 0, 0, 64, 17 }, received[64];
  size_t n;
  packet[12] = 10; packet[15] = 1; packet[16] = 10; packet[19] = 2;
  packet[20] = 1; packet[22] = 2; packet[25] = 8;
  assert (!mulib_container_netdev_init (&a, "eth0", MULIB_CONTAINER_NETDEV_ETHERNET, 1500, 1, 2048));
  assert (!mulib_container_netdev_init (&b, "eth1", MULIB_CONTAINER_NETDEV_ETHERNET, 1500, 1, 2048));
  assert (!mulib_container_netdev_attach (&a, mulib_container_netdev_peer, &b, NULL));
  assert (!mulib_container_netdev_link (&a, 1) && !mulib_container_netdev_link (&b, 1));
  assert (!mulib_container_network_init (&network, 4, 4, 1));
  memset (&route, 0, sizeof (route)); route.family = 4; route.prefix = 24; route.network[0] = 10;
  route.device = &a; route.next_hop_mac[0] = 2; route.next_hop_mac[5] = 2;
  assert (!mulib_container_network_add (&network, &route));
  assert (!mulib_container_network_send (&network, packet, sizeof (packet)));
  assert (mulib_container_network_send (&network, packet, sizeof (packet)) == MULIB_CT_AGAIN);
  assert (mulib_container_netdev_receive (&b, received, 4, &n) == MULIB_CT_ELIMIT && n == 42);
  assert (!mulib_container_netdev_receive (&b, received, sizeof (received), &n));
  assert (n == 42 && received[12] == 8 && !memcmp (received + 14, packet, sizeof (packet)));
  assert (!mulib_container_network_policy (&network, NULL, 0, 0));
  assert (mulib_container_network_send (&network, packet, sizeof (packet)) == MULIB_CT_EACCESS);
  mulib_container_network_free (&network); mulib_container_netdev_free (&a); mulib_container_netdev_free (&b);
}

typedef struct transport_fixture
{
  mulib_containerd_ctrlbridge *bridge;
  uint64_t now;
  size_t chunk;
} transport_fixture;
static int transport_write (void *context, const void *p, size_t n, size_t *done)
{
  transport_fixture *t = (transport_fixture *)context;
  int rc;
  if (n > t->chunk) n = t->chunk;
  rc = mulib_containerd_ctrlbridge_feed (t->bridge, p, n, done, t->now);
  return rc < 0 ? rc : *done ? 0 : MULIB_CT_AGAIN;
}
static int transport_read (void *context, void *p, size_t n, size_t *done)
{
  transport_fixture *t = (transport_fixture *)context;
  int rc;
  if (n > t->chunk) n = t->chunk;
  rc = mulib_containerd_ctrlbridge_drain (t->bridge, p, n, done);
  return rc < 0 ? rc : *done ? 0 : MULIB_CT_AGAIN;
}
static int roundtrip (mulib_container_stub *stub, mulib_conainerd_command *command,
                      mulib_conainerd_status *status)
{
  size_t i;
  int rc;
  assert (!mulib_container_stub_begin (stub, command));
  for (i = 0; i < 1024; ++i)
    {
      rc = mulib_container_stub_pump (stub, 1);
      assert (rc == MULIB_CT_AGAIN || rc == MULIB_CT_DONE);
      if (rc == MULIB_CT_DONE) return mulib_container_stub_take (stub, status);
    }
  assert (0 && "client failed to make progress");
  return -1;
}
static void daemon_roundtrip (void)
{
  mulib_containerd daemon;
  mulib_container_qvmboost qvm;
  mulib_ct_blob image = { 0 }, retry = { 0 };
  mulib_containerd_accessstub policy;
  mulib_containerd_access_rule rule;
  mulib_containerd_principal principal = { 42, 1000, 1000, 1 };
  mulib_containerd_ctrlbridge bridge;
  mulib_container_stub stub;
  mulib_container_stub_transport transport;
  transport_fixture fixture;
  mulib_conainerd_command command;
  mulib_conainerd_status status;
  mulib_containerd_mount_lease lease;
  mulib_container_vfs *fs;
  mulib_fuse_request request = { 7, MULIB_FUSE_READ, "/etc/message", 0, 6, NULL };
  mulib_fuse_response response;
  size_t used;
  unsigned char bytes[128];
  assert (!example_container_image (&image));
  assert (!example_container_daemon (&daemon, &qvm));
  assert (!mulib_containerd_cache_put (&daemon.cache, "base", image.data, image.length, 0, 0));
  assert (!mulib_containerd_create_cached (&daemon, "base", "instance", "qvm", MULIB_CONTAINER_CAP_BUDGET, 1));
  assert (mulib_containerd_create (&daemon, image.data, image.length, "instance", "qvm", 0, 1) == MULIB_CT_EEXIST);
  assert (!mulib_containerd_mount_acquire (&daemon.mounts, "instance", &lease, &fs));
  assert (!mulib_containerd_mount_dispatch (&daemon.mounts, lease, &request,
      bytes, sizeof (bytes), NULL, NULL, &response));
  assert (response.transferred == 6 && !memcmp (bytes, "hello\n", 6));
  assert (!mulib_containerd_accessstub_init (&policy, 4, 0));
  memset (&rule, 0, sizeof (rule)); rule.subject = 42; rule.allow = 1;
  rule.operations = MULIB_CONTAINERD_ACCESS_ALL;
  assert (!mulib_containerd_accessstub_add (&policy, &rule));
  assert (!mulib_containerd_ctrlbridge_init (&bridge, &daemon, &policy, &principal));
  fixture.bridge = &bridge; fixture.now = 10; fixture.chunk = 3;
  transport.write = transport_write; transport.read = transport_read; transport.user = &fixture;
  assert (!mulib_container_stub_init (&stub, &transport, 1));
  memset (&command, 0, sizeof (command)); strcpy (command.id, "instance");
  command.opcode = MULIB_CONAINERD_START; command.expected_generation = 999;
  assert (roundtrip (&stub, &command, &status) == MULIB_CT_ESTALE);
  command.expected_generation = 1;
  assert (!roundtrip (&stub, &command, &status));
  assert (status.state == MULIB_CONTAINER_RUNNING && status.generation == 2 && status.started_at == 10);
  /* Replay the exact start frame: it must not start or increment generation twice. */
  command.request_id = 2;
  assert (!mulib_conainerd_ipc_command (&command, &retry));
  assert (mulib_containerd_ctrlbridge_feed (&bridge, retry.data, retry.length, &used, 20) == MULIB_CT_DONE);
  assert (mulib_containerd_ctrlbridge_drain (&bridge, bytes, sizeof (bytes), &used) == MULIB_CT_DONE);
  command.expected_generation = 0; command.opcode = MULIB_CONAINERD_PAUSE;
  assert (!roundtrip (&stub, &command, &status) && status.state == MULIB_CONTAINER_PAUSED);
  command.opcode = MULIB_CONAINERD_RESUME;
  assert (!roundtrip (&stub, &command, &status));
  command.opcode = MULIB_CONAINERD_STEP; command.value = 1;
  assert (!roundtrip (&stub, &command, &status) && status.state == MULIB_CONTAINER_RUNNING);
  assert (!roundtrip (&stub, &command, &status) && status.state == MULIB_CONTAINER_EXITED && status.exit_code == 42);
  command.opcode = MULIB_CONAINERD_DELETE; command.value = 0;
  assert (roundtrip (&stub, &command, &status) == MULIB_CT_EBUSY);
  assert (!mulib_containerd_mount_release (&daemon.mounts, lease));
  assert (!roundtrip (&stub, &command, &status) && status.state == MULIB_CONTAINER_DELETED);
  assert (mulib_containerd_mount_release (&daemon.mounts, lease) == MULIB_CT_ESTALE);
  principal.authenticated = 0;
  assert (mulib_containerd_accessstub_check (&policy, &principal, &command, NULL) == MULIB_CT_EACCESS);
  mulib_container_stub_free (&stub); mulib_containerd_ctrlbridge_free (&bridge);
  mulib_containerd_accessstub_free (&policy); assert (!mulib_containerd_free (&daemon, 30));
  mulib_ct_blob_free (&image); mulib_ct_blob_free (&retry);
}

static void quota_vm (void)
{
  mulib_qvm vm;
  mulib_qvm_instruction code[] = {
    { MULIB_QVM_CONST, 0, 0, 0, 9 }, { MULIB_QVM_STORE, 0, 1, 0, 0 },
    { MULIB_QVM_LOAD, 2, 1, 0, 0 }, { MULIB_QVM_HALT, 2, 0, 0, 0 }
  };
  assert (!mulib_qvm_init (&vm, code, 4, 8, 4096, 10, NULL, NULL));
  assert (mulib_qvm_run (&vm, 2) == MULIB_CT_AGAIN && vm.pc == 2);
  assert (mulib_qvm_run (&vm, 2) == MULIB_CT_DONE && vm.exit_code == 9);
  assert (!mulib_qvm_reset (&vm));
  vm.registers[1] = 1;
  assert (mulib_qvm_run (&vm, 4) == MULIB_CT_ELIMIT);
  assert (vm.pc == 1 && vm.memory.data[0] == 0);
  mulib_qvm_free (&vm);
  code[0].opcode = MULIB_QVM_JUMP; code[0].immediate = 0;
  assert (!mulib_qvm_init (&vm, code, 4, 8, 4096, 3, NULL, NULL));
  assert (mulib_qvm_run (&vm, 10) == MULIB_CT_ELIMIT && vm.steps == 3);
  mulib_qvm_free (&vm);
}
static void paused_exit (void)
{
  mulib_qvm_instruction code = { MULIB_QVM_JUMP, 0, 0, 0, 0 };
  mulib_container_qvmboost config;
  mulib_container_runtime_ops ops = mulib_container_qvmboost_ops ();
  mulib_container_runtime runtime;
  mulib_container_spec spec;
  mulib_container container;
  mulib_container_qvmboost_init (&config, &code, 1);
  config.memory_bytes = 0;
  assert (!mulib_container_spec_init (&spec, "paused"));
  assert (!mulib_container_init (&container, &spec));
  assert (!mulib_container_runtime_init (&runtime, &container, NULL, &ops, &config));
  assert (!mulib_container_runtime_start (&runtime, 1));
  assert (!mulib_container_runtime_pause (&runtime, 1, 2));
  assert (mulib_container_runtime_step (&runtime, 10, 3) == MULIB_CT_AGAIN);
  assert (!mulib_container_runtime_signal (&runtime, 15));
  assert (mulib_container_runtime_step (&runtime, 1, 4) == MULIB_CT_DONE);
  assert (container.state == MULIB_CONTAINER_EXITED && container.exit_code == 143);
  assert (!mulib_container_runtime_close (&runtime, 5));
  mulib_container_free (&container); mulib_container_spec_free (&spec);
}
static void zstd_profiles (void)
{
  unsigned char *data = (unsigned char *)malloc (300000);
  unsigned char rle[] = { 0x28, 0xb5, 0x2f, 0xfd, 0x60, 0, 0, 3, 8, 0, 'x' };
  mulib_ct_blob packed = { 0 }, decoded = { 0 };
  size_t i;
  assert (data);
  for (i = 0; i < 300000; ++i) data[i] = i < 150000 ? 0 : (unsigned char)(i * 37 + i / 17);
  assert (!mulib_zstd_raw_encode (NULL, 0, &packed, 16));
  assert (!mulib_zstd_raw_decode (packed.data, packed.length, &decoded, 0) && !decoded.length);
  assert (!mulib_zstd_raw_encode (data, 300000, &packed, 400000));
  assert (!mulib_zstd_raw_decode (packed.data, packed.length, &decoded, 300000));
  assert (decoded.length == 300000 && !memcmp (decoded.data, data, 300000));
  for (i = 0; i < 16; ++i) assert (mulib_zstd_raw_decode (packed.data, i, &decoded, 300000) < 0);
  assert (!mulib_zstd_raw_decode (rle, sizeof (rle), &decoded, 256));
  assert (decoded.length == 256 && decoded.data[0] == 'x' && decoded.data[255] == 'x');
  rle[7] = 5;
  assert (mulib_zstd_raw_decode (rle, sizeof (rle), &decoded, 256) == MULIB_CT_ENOTSUP);
  rle[4] |= 8;
  assert (mulib_zstd_raw_decode (rle, sizeof (rle), &decoded, 256) == MULIB_CT_EFORMAT);
  free (data); mulib_ct_blob_free (&packed); mulib_ct_blob_free (&decoded);
}
typedef struct backend_failures
{
  int start_error, poll_error, close_second, pin_on_close, cache_on_close;
  size_t starts, closes, live;
  mulib_containerd *daemon;
  mulib_containerd_mount_lease lease;
  mulib_containerd_cache_lease cache_lease;
} backend_failures;
typedef struct backend_handle
{ backend_failures *failures; int second, stopped; } backend_handle;
static int failure_prepare (void *ctx, const mulib_container_spec *s,
                            const mulib_rootfs_t *root, void **out)
{
  backend_handle *h = (backend_handle *)malloc (sizeof (*h));
  (void)root;
  if (!h) return MULIB_CT_ENOMEM;
  h->failures = (backend_failures *)ctx; h->second = !strcmp (s->id, "second"); h->stopped = 0;
  ++h->failures->live; *out = h;
  return 0;
}
static int failure_start (void *ctx)
{
  backend_handle *h = (backend_handle *)ctx;
  ++h->failures->starts;
  if (h->failures->start_error) return MULIB_CT_EIO;
  h->stopped = 0;
  return 0;
}
static int failure_poll (void *ctx, size_t budget, int *code)
{
  backend_handle *h = (backend_handle *)ctx;
  (void)budget;
  if (h->failures->poll_error) return MULIB_CT_EIO;
  if (h->stopped) { *code = 137; return MULIB_CT_DONE; }
  return MULIB_CT_AGAIN;
}
static int failure_signal (void *ctx, unsigned sig)
{ (void)sig; ((backend_handle *)ctx)->stopped = 1; return 0; }
static int failure_destroy (void *ctx)
{
  backend_handle *h = (backend_handle *)ctx;
  if (h->second && h->failures->close_second) return MULIB_CT_EIO;
  if (h->failures->pin_on_close)
    {
      mulib_containerd *d = h->failures->daemon;
      mulib_containterpool_handle handle;
      mulib_containterpool_entry *entry;
      mulib_container_vfs *fs;
      h->failures->pin_on_close = 0;
      assert (!mulib_containterpool_find (&d->pool, "pinned", &handle));
      assert (mulib_containterpool_acquire (&d->pool, handle, &entry) == MULIB_CT_EBUSY);
      assert (!mulib_containerd_mount_acquire (&d->mounts, "pinned", &h->failures->lease, &fs));
    }
  if (h->failures->cache_on_close)
    {
      const mulib_ct_blob *image;
      h->failures->cache_on_close = 0;
      assert (!mulib_containerd_cache_acquire (&h->failures->daemon->cache,
          "cached", 0, &h->failures->cache_lease, &image));
    }
  ++h->failures->closes; --h->failures->live; free (h);
  return 0;
}
static void backend_failure_paths (void)
{
  backend_failures failures = { 0 };
  mulib_container_runtime_ops ops = { MULIB_CONTAINER_CAP_SIGNAL,
    failure_prepare, failure_start, failure_poll, failure_signal, NULL, failure_destroy };
  mulib_container_spec spec;
  mulib_container container;
  mulib_container_runtime runtime;
  mulib_containerd daemon;
  mulib_containerd_options options = mulib_containerd_default_options ();
  mulib_containerboost_backend backend;
  mulib_ct_blob image = { 0 };
  mulib_containerd_mount_lease lease;
  mulib_container_vfs *fs;
  mulib_vfs_stat stat;
  assert (!mulib_container_spec_init (&spec, "first"));
  spec.limits.wall_deadline = 5;
  assert (!mulib_container_init (&container, &spec));
  assert (!mulib_container_runtime_init (&runtime, &container, NULL, &ops, &failures));
  failures.start_error = 1;
  assert (mulib_container_runtime_start (&runtime, 1) == MULIB_CT_EIO);
  assert (container.state == MULIB_CONTAINER_CREATED && container.generation == 1);
  failures.start_error = 0;
  assert (!mulib_container_runtime_start (&runtime, 2));
  failures.poll_error = 1;
  assert (mulib_container_runtime_step (&runtime, 1, 3) == MULIB_CT_EIO);
  assert (container.state == MULIB_CONTAINER_RUNNING && container.generation == 2);
  failures.poll_error = 0;
  assert (mulib_container_runtime_step (&runtime, 1, 5) == MULIB_CT_DONE);
  assert (container.exit_code == 137 && runtime.deadline_sent);
  assert (!mulib_container_runtime_close (&runtime, 6));
  mulib_container_free (&container); mulib_container_spec_free (&spec);
  assert (!failures.live);
  /* Progressive shutdown must never leave exports pointing at already freed roots. */
  options.containers = 2;
  assert (!mulib_containerd_init (&daemon, &options));
  memset (&backend, 0, sizeof (backend)); strcpy (backend.name, "failures");
  backend.architecture_mask = 1; backend.ops = ops; backend.context = &failures;
  backend.ops.capabilities |= MULIB_CONTAINER_CAP_PAUSE;
  assert (mulib_containerboost_register (&daemon.backends, &backend) == MULIB_CT_EINVAL);
  assert (!daemon.backends.count);
  backend.ops = ops;
  assert (!mulib_containerboost_register (&daemon.backends, &backend));
  assert (!example_container_image (&image));
  assert (!mulib_containerd_create (&daemon, image.data, image.length, "first", NULL, 0, 0));
  assert (!mulib_containerd_create (&daemon, image.data, image.length, "second", NULL, 0, 0));
  failures.close_second = 1;
  assert (mulib_containerd_free (&daemon, 1) == MULIB_CT_EIO);
  assert (daemon.pool.count == 1 && daemon.mounts.count == 1 && failures.live == 1);
  assert (mulib_containerd_mount_acquire (&daemon.mounts, "first", &lease, &fs) == MULIB_CT_ENOENT);
  assert (!mulib_containerd_mount_acquire (&daemon.mounts, "second", &lease, &fs));
  assert (!mulib_vfs_getstat (&fs->router, "/etc/message", &stat) && stat.size == 6);
  assert (!mulib_containerd_mount_release (&daemon.mounts, lease));
  failures.close_second = 0;
  assert (!mulib_containerd_free (&daemon, 2) && !failures.live);
  /* A destructor can acquire an export after shutdown's initial preflight. */
  assert (!mulib_containerd_init (&daemon, &options));
  assert (!mulib_containerboost_register (&daemon.backends, &backend));
  assert (!mulib_containerd_create (&daemon, image.data, image.length, "pinned", NULL, 0, 0));
  failures.daemon = &daemon; failures.pin_on_close = 1;
  assert (mulib_containerd_free (&daemon, 1) == MULIB_CT_EBUSY);
  assert (daemon.pool.count == 1 && daemon.mounts.count == 1 && !failures.live);
  fs = &daemon.mounts.entries[failures.lease.slot].filesystem;
  assert (!mulib_vfs_getstat (&fs->router, "/etc/message", &stat) && stat.size == 6);
  assert (!mulib_containerd_mount_release (&daemon.mounts, failures.lease));
  assert (!mulib_containerd_free (&daemon, 2));
  /* Cache leases acquired by a destructor remain releasable after partial shutdown. */
  assert (!mulib_containerd_init (&daemon, &options));
  assert (!mulib_containerboost_register (&daemon.backends, &backend));
  assert (!mulib_containerd_cache_put (&daemon.cache, "cached", image.data, image.length, 0, 0));
  assert (!mulib_containerd_create (&daemon, image.data, image.length, "first", NULL, 0, 0));
  failures.cache_on_close = 1;
  assert (mulib_containerd_free (&daemon, 1) == MULIB_CT_EBUSY);
  assert (daemon.initialized && daemon.cache.count == 1 && !failures.live);
  assert (!mulib_containerd_cache_release (&daemon.cache, failures.cache_lease));
  assert (!mulib_containerd_free (&daemon, 2));
  mulib_ct_blob_free (&image);
}
static void generation_exhaustion (void)
{
  backend_failures failures = { 0 };
  mulib_container_runtime_ops ops = { MULIB_CONTAINER_CAP_SIGNAL,
    failure_prepare, failure_start, failure_poll, failure_signal, NULL, failure_destroy };
  mulib_container_spec spec;
  mulib_container container;
  mulib_container_runtime runtime;
  assert (!mulib_container_spec_init (&spec, "generation"));
  assert (!mulib_container_init (&container, &spec));
  assert (!mulib_container_runtime_init (&runtime, &container, NULL, &ops, &failures));
  container.generation = UINT64_MAX - 2;
  assert (mulib_container_runtime_start (&runtime, 1) == MULIB_CT_ELIMIT);
  assert (!failures.starts && container.state == MULIB_CONTAINER_CREATED);
  container.generation = UINT64_MAX - 3;
  assert (!mulib_container_runtime_start (&runtime, 2));
  assert (mulib_container_transition (&container, MULIB_CONTAINER_PAUSED, 3, 0) == MULIB_CT_ELIMIT);
  assert (!mulib_container_runtime_signal (&runtime, 9));
  assert (mulib_container_runtime_step (&runtime, 1, 4) == MULIB_CT_DONE);
  assert (container.generation == UINT64_MAX - 1);
  assert (!mulib_container_runtime_close (&runtime, 5));
  assert (container.state == MULIB_CONTAINER_DELETED && container.generation == UINT64_MAX);
  assert (!failures.live && failures.starts == 1 && failures.closes == 1);
  mulib_container_free (&container); mulib_container_spec_free (&spec);
}
int main (void)
{
  specifications (); filesystem (); images (); frames (); cache_and_logs ();
  packets (); quota_vm (); zstd_profiles (); daemon_roundtrip (); backend_failure_paths ();
  paused_exit (); generation_exhaustion ();
  puts ("container stack: ownership, images, IPC, policy, networking and daemon passed");
  return 0;
}
