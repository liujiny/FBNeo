// PS5 r21 lifecycle coverage adapted to the production PS4 POSIX worker.
#include <pthread.h>
#include <atomic>
#include <cassert>
#include <cstdio>
#include <cerrno>
#include <sched.h>
typedef int INT32;
typedef unsigned long long UINT64;
static int failure, cond_calls;
static int test_mutex(pthread_mutex_t *m, const pthread_mutexattr_t *a) { return failure == 1 ? ENOMEM : pthread_mutex_init(m,a); }
static int test_cond(pthread_cond_t *c, const pthread_condattr_t *a) { return failure == ++cond_calls + 1 ? ENOMEM : pthread_cond_init(c,a); }
static int test_create(pthread_t *t, const pthread_attr_t *a, void *(*f)(void*), void *p) { return failure == 4 ? EAGAIN : pthread_create(t,a,f,p); }
#define pthread_mutex_init test_mutex
#define pthread_cond_init test_cond
#define pthread_create test_create
#define FBNEO_RENDER_THREADS_TEST
#define SCAN_VAR(x) ((void)(x))
#include "../../src/burn/devices/epic12_thread.h"
#undef pthread_mutex_init
#undef pthread_cond_init
#undef pthread_create
INT32 nBurnRenderCores = 2;
static std::atomic<int> count(0), entered(0);
static std::atomic<bool> gate(false), release(false);
static pthread_t callback_id;
static void callback() {
 entered=1;
 while(gate && !release) sched_yield();
 callback_id=pthread_self(); ++count;
}
int main() {
 const pthread_t main_id=pthread_self();
 for(failure=1;failure<=4;failure++) {
  cond_calls=0; thready.init(callback); assert(!thready.available);
  int before=count; thready.notify(); thready.notify_wait();
  assert(count==before+1 && pthread_equal(callback_id,main_id)); thready.exit();
 }
 failure=0;
 for(int pass=0;pass<3;pass++) {
  count=0; thready.init(callback); assert(thready.available); thready.reset();
  for(int i=0;i<180;i++) thready.notify();
  assert(count==180 && pthread_equal(callback_id,main_id));
  for(int i=0;i<2000;i++) thready.notify();
  thready.notify_wait(); assert(count==2180 && !pthread_equal(callback_id,main_id));
  entered=0;gate=true;release=false;thready.notify();
  while(!entered) sched_yield();
  assert(count==2180);release=true;thready.scan();assert(count==2181);gate=false;
  thready.notify();thready.set_threading(0);assert(count==2182);
  thready.notify();assert(count==2183 && pthread_equal(callback_id,main_id));
  thready.set_threading(1);thready.notify();thready.reset();assert(count==2184);
  for(int i=0;i<180;i++) thready.notify();
  thready.notify();thready.init(callback);assert(count==2365);
  thready.notify();thready.exit();assert(count==2366);thready.exit();
  nBurnRenderCores=1;thready.init(callback);thready.set_threading(1);thready.notify();
  assert(pthread_equal(callback_id,main_id));thready.exit();nBurnRenderCores=2;
 }
 puts("PASS: 6000 ordered jobs; overlap; warmup; init failures; disable/scan/reset/reinit/exit drains; 1-core fallback");
}
