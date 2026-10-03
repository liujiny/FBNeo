#!/usr/bin/env python3
"""Exercise the production initializer logic and realloc wrapper on the host."""
from pathlib import Path
import subprocess, tempfile, re
src = Path(__file__).resolve().parents[2] / 'src/burner/libretro'
with tempfile.TemporaryDirectory() as temp:
    d = Path(temp)
    text = re.sub(r'__asm__\(.*?\);', '', (src/'ps4_module.c').read_text(), flags=re.S)
    for a,b in [('_init','test_init'),('_fini','test_fini'),('__init_array_start','__start_test_ctors'),('__init_array_end','__stop_test_ctors')]:
        text = re.sub(r'\b'+a+r'\b',b,text)
    text += r"""
#include <assert.h>
#include <stdio.h>
static int first, second;
static void a(void) { first++; test_init(); }
static void b(void) { second++; }
void (*test_callbacks[])(void) __attribute__((section("test_ctors"),used)) = {a,b};
int main(void) {
    assert(test_init() == 0);
    assert(module_start(0,0) == 0);
    assert(test_init() == 0);
    assert(first == 1 && second == 1);
    puts("PASS: DT_INIT path, real table bounds, exactly-once and reentrant initialization");
}
"""
    (d/'init.c').write_text(text)
    subprocess.run(['cc','-std=c99','-O2',str(d/'init.c'),'-o',str(d/'init')],check=True)
    subprocess.run([str(d/'init')],check=True)
    (d/'alloc.c').write_text(r"""
#include <assert.h>
#include <stdlib.h>
#include <malloc.h>
#include <stdio.h>
#include <string.h>
void *__wrap_realloc(void *,size_t);
static int fail_alloc, delegated;
void *test_malloc(size_t n) { return fail_alloc ? NULL : malloc(n); }
void *__real_realloc(void *p,size_t n) { delegated++; return realloc(p,n); }
int main(void) {
    const size_t sizes[] = {64,16384,0x980000};
    for (unsigned i=0;i<3;i++) {
        size_t old=sizes[i], small=i==2?0x400000:old/2;
        unsigned char *p=malloc(old); assert(p); memset(p,0x5a,old);
        fail_alloc=1;
        assert(__wrap_realloc(p,small)==NULL);
        for(size_t j=0;j<old;j++) assert(p[j]==0x5a);
        fail_alloc=0;
        int before=delegated;
        p=__wrap_realloc(p,small); assert(p && delegated==before);
        for(size_t j=0;j<small;j++) assert(p[j]==0x5a);
        size_t grow=malloc_usable_size(p)+65536;
        p=__wrap_realloc(p,grow); assert(p && delegated==before+1);
        for(size_t j=0;j<small;j++) assert(p[j]==0x5a);
        assert(__wrap_realloc(p,0)==NULL);
    }
    void *p=__wrap_realloc(NULL,128); assert(p); free(p);
    puts("PASS: 9.5 MiB to 4 MiB shrink, small blocks, growth, zero/NULL and allocation failure preserve data");
}
""")
    flags=['-std=c99','-g','-O1','-fsanitize=address,undefined']
    subprocess.run(['cc',*flags,'-Dmalloc=test_malloc','-c',str(src/'ps4_realloc.c'),'-o',str(d/'wrapper.o')],check=True)
    subprocess.run(['cc',*flags,str(d/'alloc.c'),str(d/'wrapper.o'),'-o',str(d/'alloc')],check=True)
    subprocess.run([str(d/'alloc')],check=True)
