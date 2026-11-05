#define MULIB_IMPL_STATIC_FILE
#define MULIB_IMPL_VFS
#define MULIB_IMPL_MIME
#define MULIB_IMPL_SECURITY
#define MULIB_IMPL_HTTP_RESPONSE
#define MULIB_IMPL_WEBFRAMEWORK
#define MULIB_IMPL_HTTP
#define MULIB_IMPL_URI
#define MULIB_IMPL_INET4
#define MULIB_IMPL_INET6
#define MULIB_IMPL_INTE6
#define MULIB_IMPL_STRBUF
#define MULIB_IMPL_PARSEC
#define MULIB_IMPL_BUFFER
#include "mulib-static_file.h"
#include <assert.h>
#include <string.h>
int main (void)
{
  mulib_vfs vfs;
  mulib_vfs_mem mem;
  mulib_vfs_ops ops;
  mulib_http_message_t response = { 0 };
  assert (!mulib_vfs_init (&vfs, 1) && !mulib_vfs_mem_init (&mem, 10, 1000));
  ops = mulib_vfs_mem_ops ();
  assert (!mulib_vfs_mount (&vfs, "/", &ops, &mem));
  assert (!mulib_vfs_mem_put (&mem, "/hello.txt", "hello", 5));
  assert (mulib_static_file_reply (&vfs, "/hello.txt", 5, &response) == MULIB_HTTP_OK);
  assert (response.body_length == 5 && !memcmp (response.body, "hello", 5));
  mulib_http_message_free (&response);
  mulib_vfs_free (&vfs);
  mulib_vfs_mem_free (&mem);
  return 0;
}
