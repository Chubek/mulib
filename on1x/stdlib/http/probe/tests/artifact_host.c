#define MULIB_IMPL_WORKSPACE_FS
#include "artifact.h"

#include <string.h>
#include <unistd.h>

static mulib_wfs_t workspace = MULIB_WFS_INIT;

int64_t artifact_fixture_init(void)
{
  return mulib_wfs_open(&workspace, ".") == MULIB_WFS_OK &&
    symlinkat("document.html", workspace.root_fd, "link.html") == 0;
}

int on1x_http_probe_artifact_authorize_v1(const char *slot, size_t length,
                                          on1x_http_probe_artifact_grant_v1 *grant)
{
  const char *filename;
  if (length == 7 && !memcmp(slot, "primary", 7))
    filename = "document.html";
  else if (length == 7 && !memcmp(slot, "symlink", 7))
    filename = "link.html";
  else
    return 0;
  *grant = (on1x_http_probe_artifact_grant_v1){&workspace, filename};
  return 1;
}

int64_t artifact_fixture_check(void)
{
  mulib_wfs_blob_t blob = {0};
  mulib_wfs_stat_t stat = {0};
  int ok = mulib_wfs_stat(&workspace, "document.html", &stat) == MULIB_WFS_OK &&
    (stat.mode & 0777) == 0600 &&
    mulib_wfs_read(&workspace, "document.html", 1024, &blob) == MULIB_WFS_OK &&
    strstr((const char *)blob.data, "<p>hello</p>") != NULL;
  mulib_wfs_blob_free(&blob);
  mulib_wfs_close(&workspace);
  return ok;
}
