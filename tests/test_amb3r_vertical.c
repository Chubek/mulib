/* Multi-subsystem functional integration test */
#define MULIB_IMPL_ECAD_SCHEMATIC
#define MULIB_IMPL_ECAD_NETLIST
#define MULIB_IMPL_SPICE_LEXER
#define MULIB_IMPL_SPICE_NETLIST
#define MULIB_IMPL_SPICE_STAMP
#define MULIB_IMPL_SPICE_R
#define MULIB_IMPL_SPICE_C
#define MULIB_IMPL_SPICE_INDEPENDENT
#define MULIB_IMPL_NUM_DENSE
#define MULIB_IMPL_SPICE_TRAN
#define MULIB_IMPL_WAVE_SIGNAL
#define MULIB_IMPL_WAVE_MEASURE
#define MULIB_IMPL_PCB_GEOMETRY
#define MULIB_IMPL_PCB_DRC
#define MULIB_IMPL_PCB_GERBER
#define MULIB_IMPL_DSIM_LOGIC4
#define MULIB_IMPL_HWIL_MODULE
#define MULIB_IMPL_HWIL_VERIFY
#define MULIB_IMPL_FMT_SV
#define MULIB_IMPL_SYNTH_AIG
#define MULIB_IMPL_FMT_AIGER
#include "mulib-ecad-schematic.h"
#include "mulib-ecad-netlist.h"
#include "mulib-spice-lexer.h"
#include "mulib-spice-netlist.h"
#include "mulib-spice-stamp.h"
#include "mulib-spice-r.h"
#include "mulib-spice-c.h"
#include "mulib-spice-independent.h"
#include "mulib-num-dense.h"
#include "mulib-spice-tran.h"
#include "mulib-wave-signal.h"
#include "mulib-wave-measure.h"
#include "mulib-pcb-geometry.h"
#include "mulib-pcb-drc.h"
#include "mulib-pcb-gerber.h"
#include "mulib-dsim-logic4.h"
#include "mulib-hwil-module.h"
#include "mulib-hwil-verify.h"
#include "mulib-fmt-sv.h"
#include "mulib-synth-aig.h"
#include "mulib-fmt-aiger.h"
#include <assert.h>
#include <string.h>
#include <math.h>
#include <stdio.h>
int main(void){
  mulib_ecad_schematic_t s={0};
  const mulib_ecad_component_t items[]={
    {"V1",'V',"vin","0",1.0},
    {"R1",'R',"vin","out",1000.0},
    {"C1",'C',"out","0",0.000001}};
  for(size_t i=0;i<3;i++)assert(mulib_ecad_schematic_add(&s,&items[i]));
  char text[1024]={0};size_t used;
  assert(mulib_ecad_netlist_write(&s,text,sizeof text,&used));
  mulib_spice_netlist_t n={0};assert(mulib_spice_netlist_parse(text,&n,NULL));
  int p=mulib_spice_netlist_find_node(&n,"out");assert(p>0);
  double voltages[9]={0};
  assert(mulib_spice_tran_fixed(&n,.001,9,(uint16_t)p,voltages));
  mulib_wave_signal_t wave={0};
  /* R*C=dt=1 ms, with a 1 V source and zero initial voltage.
   * Backward Euler gives V[k]=1-2^-k, not the continuous-time result. */
  double prev=0, sum_sq=0;
  for(int i=0;i<9;i++){
    double expected=1-ldexp(1.0,-(i+1));
    assert(fabs(voltages[i]-expected)<1e-12);
    /* KCL residual in amperes: resistor current plus capacitor current. */
    assert(fabs((voltages[i]-1)/1000.0+1e-6*(voltages[i]-prev)/.001)<1e-14);
    prev=voltages[i];
    sum_sq+=expected*expected;
    assert(mulib_wave_signal_push(&wave,(i+1)*.001,voltages[i]));
  }
  double rms;assert(mulib_wave_measure_rms(&wave,&rms)&&fabs(rms-sqrt(sum_sq/9))<1e-12);
  mulib_wave_signal_free(&wave);
  mulib_pcb_segment_t traces[]={{{0,0},{10,0},.3,1},{{5,-5},{5,5},.3,2}};
  assert(mulib_pcb_drc_segments(traces,2,.2,NULL,0)==1);
  FILE *f=tmpfile();assert(f);assert(mulib_pcb_gerber_write(f,traces,2));fclose(f);
  mulib_hwil_module_t module={0};uint16_t a,b,c;size_t bad;
  assert(mulib_hwil_module_add(&module,MULIB_HWIL_INPUT,0,0,0,&a));
  assert(mulib_hwil_module_add(&module,MULIB_HWIL_INPUT,0,0,0,&b));
  assert(mulib_hwil_module_add(&module,MULIB_HWIL_AND,a,b,0,&c));
  assert(mulib_hwil_module_verify(&module,&bad));
  f=tmpfile();assert(f);assert(mulib_fmt_sv_emit(f,&module,c));fclose(f);
  mulib_synth_aig_t aig={0};uint16_t aa,bb,and_lit;
  assert(mulib_synth_aig_input(&aig,&aa));
  assert(mulib_synth_aig_input(&aig,&bb));
  assert(mulib_synth_aig_and(&aig,aa,bb,&and_lit));
  f=tmpfile();assert(f);assert(mulib_fmt_aiger_write_ascii(f,&aig,and_lit));fclose(f);
  puts("PASS Amb3r vertical integration: schematic / transient / wave / PCB / HWIL / AIG");
  return 0;
}
