#!/usr/bin/env python3
"""Compare stopped and running SH3 timers to the original scalar implementation."""
from pathlib import Path
import subprocess
import tempfile
root = Path(__file__).resolve().parents[2]
source = (root/'src/cpu/sh4/sh4.cpp').read_text()
old = subprocess.check_output(['git', 'show', '311b4efb1b9b47028e68eb337ecf835af2b6137e:src/cpu/sh4/sh4.cpp'], cwd=root, text=True)
def timer(text, name):
    start = text.index('struct sh4_dtimer')
    end = text.index('\n};', start)+3
    return text[start:end].replace('struct sh4_dtimer', 'struct '+name)
fixture = r"""
#include <cstdint>
#include <cstring>
#include <cassert>
#include <cstdio>
typedef int32_t INT32;
typedef uint32_t UINT32;
static int ratio_multi=100000, m_ratio=1;
#define SCAN_VAR(x) ((void)(x))
"""+timer(old, 'Reference')+'\n'+timer(source, 'Candidate')+r"""
static Reference *reference;
static Candidate *candidate;
static unsigned calls[2], mode;
static uint64_t trace[2];
static uint32_t rng=0x859173;
static uint32_t next() { rng^=rng<<13; rng^=rng>>17; rng^=rng<<5; return rng; }
template<class T> void callback(T *t, unsigned slot, int param) {
 // Capture state at each callback, not just its final count. In particular,
 // batching must leave the same divider remainder visible to the callback.
 const uint32_t fields[]={t->prescale_counter,t->time_current,t->time_trig,
                         (uint32_t)t->timer_prescaler,(uint32_t)t->running,
                         (uint32_t)t->retrig,(uint32_t)param};
 for(unsigned k=0;k<sizeof(fields)/sizeof(fields[0]);k++)
  trace[slot]=(trace[slot]^fields[k])*UINT64_C(1099511628211);
 unsigned n=++calls[slot];
 if(mode==0) t->stop();
 if(mode==1) t->set_prescaler(50+(n*31)%2000);
 if(mode==2) t->start(1+n%113,param,1,0);
 if(mode==3) {t->reset();t->set_prescaler(117);}
 if(mode==4) {t->running=1;t->time_current=0;t->time_trig=1+n%127;t->set_prescaler(75+n%337);}
}
static void ref_cb(int p) {callback(reference,0,p);}
static void opt_cb(int p) {callback(candidate,1,p);}
static void compare(const Reference &a,const Candidate &b) {
 assert(a.running==b.running && a.time_trig==b.time_trig && a.time_current==b.time_current);
 assert(a.timer_param==b.timer_param && a.timer_prescaler==b.timer_prescaler);
 assert(a.prescale_counter==b.prescale_counter && a.retrig==b.retrig && calls[0]==calls[1]);
 assert(trace[0]==trace[1]);
}
int main() {
 for(unsigned i=0;i<40000;i++) {
  Reference a={};Candidate b={};reference=&a;candidate=&b;
  calls[0]=calls[1]=0;trace[0]=trace[1]=0;mode=i%6;
  a.running=b.running=next()%2;
  a.time_trig=b.time_trig=1+next()%1500;
  a.time_current=b.time_current=next()%1600;
  if(i%11==0) a.time_current=b.time_current=UINT32_MAX-next()%3;
  if(i%13==0) a.time_trig=b.time_trig=0;
  if(i%17==0) a.time_trig=b.time_trig=UINT32_MAX;
  a.timer_param=b.timer_param=next()%3;
  a.timer_prescaler=b.timer_prescaler=50+next()%2000;
  a.prescale_counter=b.prescale_counter=next();
  // Limit reference iterations while still exercising unsigned wrap.
  if(i%100) a.prescale_counter=b.prescale_counter=next()%100000;
  else a.prescale_counter=b.prescale_counter=UINT32_MAX-100;
  a.retrig=b.retrig=next()%2;
  a.timer_exec=ref_cb;b.timer_exec=opt_cb;
  for(unsigned j=0;j<8;j++) {
   m_ratio=1+next()%20;int cycles=101+next()%3000;
   a.run_prescale(cycles);b.run_prescale(cycles);compare(a,b);
  }
 }
 // Defined edge cases: zero divider whose first callback repairs it, signed
 // negative divider (legacy unsigned comparison), and absent callbacks.
 for(unsigned i=0;i<24;i++) {
  Reference a={};Candidate b={};reference=&a;candidate=&b;
  calls[0]=calls[1]=0;trace[0]=trace[1]=0;mode=1;m_ratio=1;
  a.running=b.running=1;a.retrig=b.retrig=1;
  a.time_trig=b.time_trig=1;
  a.timer_prescaler=b.timer_prescaler=i%3==0?0:(i%3==1?-1:4);
  a.prescale_counter=b.prescale_counter=i%3==1?UINT32_MAX:123;
  a.timer_exec=ref_cb;b.timer_exec=opt_cb;
  if(i%3==2) a.timer_exec=b.timer_exec=NULL;
  a.run_prescale(0);b.run_prescale(0);compare(a,b);
 }
 puts("PASS: 320024 timer steps and callback-state traces; trigger/wrap boundaries, prescaler/reset/rearm/stop callbacks, null callback, zero/negative divider edges");
}
"""
with tempfile.TemporaryDirectory() as d:
    src=Path(d)/'timer.cpp';src.write_text(fixture);exe=Path(d)/'timer'
    subprocess.run(['c++','-O2','-fsanitize=address,undefined','-fno-omit-frame-pointer',str(src),'-o',str(exe)],check=True)
    subprocess.run([str(exe)],check=True,timeout=120)
