#include "mulib-workspace-fs.h"
#include "mulib-unidiff.h"
#include "mulib-workspace-edit.h"
#include "mulib-process-stream.h"
#include "mulib-fs.h"
int main(){
  mulib_wfs_t w=MULIB_WFS_INIT;
  mulib_ud_patch_t p{};
  mulib_ps_t ps=MULIB_PS_INIT;
  mulib_wfs_close(&w);
  mulib_ud_init(&p);
  mulib_ps_init(&ps);
  mulib_we_report_t result{};
  return mulib_we_apply(&w, nullptr, 0, nullptr, &result) == MULIB_WFS_INVALID &&
         mulib_fs_read_bounded(nullptr, 0, nullptr) == nullptr ? 0 : 1;
}
