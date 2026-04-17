/*
 * Vector TCM transfer test (C entry)
 * 1) scalar core copies data from main memory to TCM (0x70000000)
 * 2) vector unit performs element-wise add using small VL to match bus
 */

#include <stdio.h>
#include <string.h>
#include <stddef.h>
#include <riscv_vector.h>
#include "util.h"

#define N 1024

double src[N];
double dst[N];
// tracing buffers for correlating vector blocks with cycles/PCs
// reduce to 128 entries to speed up simulation
volatile unsigned long trace_vle_start_c[128], trace_vle_end_c[128], trace_vle_start_i[128], trace_vle_end_i[128];
volatile unsigned long trace_vfadd_start_c[128], trace_vfadd_end_c[128], trace_vfadd_start_i[128], trace_vfadd_end_i[128];
volatile unsigned long trace_vse_start_c[128], trace_vse_end_c[128], trace_vse_start_i[128], trace_vse_end_i[128];
volatile unsigned int trace_idx = 0;

void copy_to_tcm(void *dst, const void *src, unsigned long n);
void copy_from_tcm(void *dst, const void *src, unsigned long n);

int main(int argc, char *argv[])
{
  // initialize source
  for (size_t i = 0; i < N; ++i) src[i] = (double)i;

  // TCM base (default in Shuttle WithTCM)
  volatile double *tcm = (volatile double *)0x70000000UL;

  // scalar core: copy from main memory to TCM using assembly helper
  unsigned long c0 = read_csr(mcycle);
  printf("COPY_TO_TCM_START cycle=%lu\n", c0);
  copy_to_tcm((void *)tcm, (const void *)src, (unsigned long)(N * sizeof(double)));
  unsigned long c1 = read_csr(mcycle);
  printf("COPY_TO_TCM_END cycle=%lu elapsed=%lu\n", c1, c1 - c0);

  // vector unit: perform vector add inside TCM via hand-written assembly
  extern void vec_add_block_traced(double *addr, unsigned long elems, unsigned int idx);
  unsigned long v0 = read_csr(mcycle);
  printf("VEC_START cycle=%lu\n", v0);
  // iterate in larger blocks so we record only 128 entries
  const unsigned long BLOCK_ELEMS = 8; // 8 * 8B = 64B per block
  unsigned int bidx = 0;
  for (size_t i = 0; i < N; i += BLOCK_ELEMS) {
    if (bidx >= 128) break;
    unsigned int elems = (N - i) < BLOCK_ELEMS ? (N - i) : BLOCK_ELEMS;
    unsigned long addr = (unsigned long)&tcm[i];
    vec_add_block_traced((double *)addr, elems, bidx);
    bidx++;
  }
  trace_idx = bidx;
  unsigned long v1 = read_csr(mcycle);
  printf("VEC_END cycle=%lu elapsed=%lu\n", v1, v1 - v0);

  // print trace buffer entries collected by vec_ops.S
  unsigned int tidx = trace_idx;
  printf("TRACE_ENTRIES=%u\n", tidx);
  for (unsigned int i = 0; i < tidx; ++i) {
    unsigned long vle_d_c = trace_vle_end_c[i] - trace_vle_start_c[i];
    unsigned long vf_d_c = trace_vfadd_end_c[i] - trace_vfadd_start_c[i];
    unsigned long vse_d_c = trace_vse_end_c[i] - trace_vse_start_c[i];

    unsigned long vle_d_i = trace_vle_end_i[i] - trace_vle_start_i[i];
    unsigned long vf_d_i = trace_vfadd_end_i[i] - trace_vfadd_start_i[i];
    unsigned long vse_d_i = trace_vse_end_i[i] - trace_vse_start_i[i];
    
    printf("TRACE[%u] (Block Addr: %p)\n", i, (void*)&tcm[i*8]);
    printf("  VLE:   cyc=%lu, ins=%lu @start=%lu\n", vle_d_c, vle_d_i, trace_vle_start_c[i]);
    printf("  VFADD: cyc=%lu, ins=%lu @start=%lu\n", vf_d_c,  vf_d_i,  trace_vfadd_start_c[i]);
    printf("  VSE:   cyc=%lu, ins=%lu @start=%lu\n", vse_d_c, vse_d_i, trace_vse_start_c[i]);

    if (vle_d_c > 40 && vle_d_i == 1) 
      printf("      !!! STALL detected in VLE\n");
    if (vse_d_c > 40 && vse_d_i == 1)
      printf("      !!! STALL detected in VSE\n");
  }

  // scalar core: copy back from TCM to DRAM using assembly helper
  unsigned long r0 = read_csr(mcycle);
  printf("COPY_FROM_TCM_START cycle=%lu\n", r0);
  copy_from_tcm((void *)dst, (const void *)tcm, (unsigned long)(N * sizeof(double)));
  unsigned long r1 = read_csr(mcycle);
  printf("COPY_FROM_TCM_END cycle=%lu elapsed=%lu\n", r1, r1 - r0);

  // verify
  for (size_t i = 0; i < N; ++i) {
    double expect = (double)i + 1.0;
    if (dst[i] != expect) {
      printf("FAIL idx=%zu got=%f exp=%f\n", i, dst[i], expect);
      return 1;
    }
  }

  printf("vec-tcm-transfer: PASS\n");
  return 0;
}
