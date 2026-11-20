/* MuLIB: eda-job. C11 single-header kernel.
 * Define MULIB_IMPL_EDA_JOB in exactly one implementation translation unit.
 * Unless stated otherwise, 1 indicates success, 0 indicates failure.
 */
#ifndef MULIB_EDA_JOB_H
#define MULIB_EDA_JOB_H
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>
#include <float.h>
#include <limits.h>
#include <ctype.h>
#include <errno.h>
#include "mulib-storage-core.h"
#ifdef __cplusplus
extern "C" {
#endif
enum {MULIB_EDA_JOB_PENDING,MULIB_EDA_JOB_RUNNING,MULIB_EDA_JOB_DONE,MULIB_EDA_JOB_FAILED,MULIB_EDA_JOB_CANCELLED,MULIB_EDA_JOB_CANCELLING};
typedef struct {uint64_t id,revision;unsigned state;double progress;} mulib_eda_job;
int mulib_eda_job_transition(mulib_eda_job *j,unsigned next);
int mulib_eda_job_progress(mulib_eda_job *j,double progress);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_EDA_JOB) && !defined(MULIB_EDA_JOB_H_IMPLEMENTED)
#define MULIB_EDA_JOB_H_IMPLEMENTED
#ifdef __cplusplus
extern "C" {
#endif
int mulib_eda_job_transition(mulib_eda_job *j,unsigned next){if(!j||next>5)return 0;unsigned old=j->state;if(!((old==0&&(next==1||next==4))||(old==1&&(next==2||next==3||next==4||next==5))||(old==5&&(next==3||next==4))))return 0;j->state=next;if(next==2)j->progress=1;return 1;}
int mulib_eda_job_progress(mulib_eda_job *j,double progress){if(!j||j->state!=1||!isfinite(progress)||progress<j->progress||progress>1)return 0;j->progress=progress;return 1;}
#ifdef __cplusplus
}
#endif
#endif
