/* EXAMPLE ONLY. Requires trusted approval by a separate host before invocation.
 * Build from package root:
 * cc -std=c11 -D_GNU_SOURCE -D_POSIX_C_SOURCE=200809L -Iinclude \
 *   examples/apply_unidiff.c -o /tmp/apply-unidiff
 * Usage: /tmp/apply-unidiff WORKSPACE_DIR DIFF_FILE
 */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#define MULIB_IMPL_WORKSPACE_FS
#define MULIB_IMPL_UNIDIFF
#define MULIB_IMPL_WORKSPACE_EDIT
#define MULIB_IMPL_FS
#include "mulib-workspace-edit.h"
#include "mulib-fs.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
int main(int argc,char **argv){
    if(argc!=3){fprintf(stderr,"usage: %s ROOT PATCH\n",argv[0]);return 2;}
    size_t len=0;
    unsigned char *source=mulib_fs_read_bounded(argv[2],16u*1024*1024,&len);
    if(!source){perror("read patch");return 1;}
    mulib_ud_patch_t parsed={0};mulib_ud_error_t error={0};
    int rc=mulib_ud_parse(source,len,NULL,&parsed,&error);
    free(source);
    if(rc){fprintf(stderr,"patch parse error=%d line=%zu\n",rc,error.line);return 1;}
    mulib_wfs_t workspace=MULIB_WFS_INIT;
    rc=mulib_wfs_open(&workspace,argv[1]);
    if(rc){fprintf(stderr,"workspace open failed=%d\n",rc);mulib_ud_free(&parsed);return 1;}
    /* Real harness calls permission/approval engine before applying. */
    mulib_we_report_t report={0};
    rc=mulib_we_apply_unidiff(&workspace,&parsed,NULL,&report);
    fprintf(stderr,"result=%d applied=%zu restored=%zu failures=%d\n",
        rc,report.applied,report.rolled_back,report.rollback_errors);
    mulib_wfs_close(&workspace);
    mulib_ud_free(&parsed);
    return rc?1:0;
}
