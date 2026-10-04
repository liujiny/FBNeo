// Standalone source test; compile/run only when the user allows compilation.
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <vector>
typedef uint8_t UINT8;
typedef uint16_t UINT16;
typedef uint32_t UINT32;
#define EPIC12_SCREEN_TEST
#include "../../src/burn/devices/epic12_screen_copy.h"
static UINT32 seed=0xe91c12;
static UINT32 rnd(){seed^=seed<<13;seed^=seed>>17;seed^=seed<<5;return seed;}
int main() {
 const int widths[]={0,1,2,3,4,5,6,7,8,9,15,16,17,31,32,33,239,240,241,319,320,321,8191,8192,8193,16401};
 const unsigned offsets[]={0,1,7,8185,8191,8192,0xffffffff};
 const int destinations[]={0,124,128,132,136,40000};
 std::vector<UINT32> original(40000),expected(40000),actual(40000);
 for(unsigned i=0;i<original.size();i++)original[i]=rnd();
 unsigned cases=0;
 for(int bpp=2;bpp<=4;bpp+=2)
 for(unsigned w=0;w<sizeof(widths)/sizeof(widths[0]);w++)
 for(unsigned o=0;o<sizeof(offsets)/sizeof(offsets[0]);o++)
 for(unsigned d=0;d<sizeof(destinations)/sizeof(destinations[0]);d++) {
  expected=original;actual=original;
  UINT8 *ref=(UINT8*)&expected[0],*got=(UINT8*)&actual[0];
  for(int x=0;x<widths[w];x++) {
   UINT32 c;memcpy(&c,ref+128+((offsets[o]+x)&8191)*4,4);
   if(bpp==2) {
    UINT16 color=((c>>8)&0xf800)|((c>>5)&0x07e0)|((c>>3)&31);
    memcpy(ref+destinations[d]+x*2,&color,2);
   } else memcpy(ref+destinations[d]+x*4,&c,4);
  }
  if(bpp==2)epic12_screen_row565(got+destinations[d],(UINT32*)(got+128),offsets[o],widths[w]);
  else epic12_screen_row32((UINT32*)(got+destinations[d]),(UINT32*)(got+128),offsets[o],widths[w]);
  if(actual!=expected){printf("FAIL bpp=%d w=%d offset=%u dst=%d\n",bpp,widths[w],offsets[o],destinations[d]);return 1;}
  ++cases;
 }
#if defined(__SSE2__) && defined(__x86_64__)
 if(!epic12_screen_simd_blocks)return 2;
#endif
 printf("PASS %u output row cases\n",cases);
 return 0;
}
