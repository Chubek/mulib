#include "artifact.h"

#include <sys/stat.h>
#include <unistd.h>

static int identifier(const char *slot)
{
  int n = 0;
  if (!slot) return -1;
  for (; n < 64 && slot[n]; ++n) {
    const unsigned char b = (unsigned char)slot[n];
    if (!((b >= 'a' && b <= 'z') || (b >= 'A' && b <= 'Z') ||
          (b >= '0' && b <= '9') || (n && (b == '-' || b == '_'))))
      return -1;
  }
  return n > 0 && n < 64 ? n : -1;
}

int64_t on1x_http_probe_artifact_write_v1(const char *slot, const char *html)
{
  const int key_length = identifier(slot);
  size_t length = 0;
  if (key_length < 0 || !html) return -1;
  while (length <= ON1X_HTTP_PROBE_ARTIFACT_MAX_BYTES && html[length]) ++length;
  if (length > ON1X_HTTP_PROBE_ARTIFACT_MAX_BYTES) return -3;
  on1x_http_probe_artifact_grant_v1 grant = {0};
  struct stat info;
  if (on1x_http_probe_artifact_authorize_v1(slot, (size_t)key_length, &grant) != 1 ||
      !grant.workspace || !grant.relative_path ||
      fstat(grant.workspace->root_fd, &info) || !S_ISDIR(info.st_mode) ||
      info.st_uid != geteuid() || (info.st_mode & 0077))
    return -2;
  switch (mulib_wfs_write(grant.workspace, grant.relative_path, html, length, 0600, 0)) {
  case MULIB_WFS_OK: return 0;
  case MULIB_WFS_UNCERTAIN: return 1;
  case MULIB_WFS_INVALID:
  case MULIB_WFS_NOT_REGULAR:
  case MULIB_WFS_DENIED: return -2;
  case MULIB_WFS_LIMIT: return -3;
  case MULIB_WFS_CONFLICT:
  case MULIB_WFS_EXISTS: return -4;
  default: return -5;
  }
}
