/*************************************************************************
* Axpy Kernel
* Author: Jesus Labarta
* Barcelona Supercomputing Center
*************************************************************************/

#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <assert.h>
#include <riscv_vector.h>
#include "util.h"

void axpy_intrinsics(double a, double *dx, double *dy, size_t n) {
  static int count = 0;
  for (size_t i = 0; i < n;) {
    long gvl = __riscv_vsetvl_e64m8(n - i);
    
    unsigned long c1 = read_csr(mcycle);
    unsigned long i1 = read_csr(minstret);
    vfloat64m8_t v_dx = __riscv_vle64_v_f64m8(&dx[i], gvl);
    unsigned long c2 = read_csr(mcycle);
    unsigned long i2 = read_csr(minstret);
    
    vfloat64m8_t v_dy = __riscv_vle64_v_f64m8(&dy[i], gvl);
    unsigned long c3 = read_csr(mcycle);
    unsigned long i3 = read_csr(minstret);
    
    vfloat64m8_t v_res = __riscv_vfmacc_vf_f64m8(v_dy, a, v_dx, gvl);
    
    unsigned long c4 = read_csr(mcycle);
    unsigned long i4 = read_csr(minstret);
    __riscv_vse64_v_f64m8(&dy[i], v_res, gvl);
    unsigned long c5 = read_csr(mcycle);
    unsigned long i5 = read_csr(minstret);

    if (count < 10) {
      printf("DAXPY block %d (addr_dx=0x%lx): VLE_DX=%lu cyc (%lu ins) | VLE_DY=%lu cyc (%lu ins) | VSE=%lu cyc (%lu ins)\n", 
             count, (unsigned long)&dx[i], c2-c1, i2-i1, c3-c2, i3-i2, c5-c4, i5-i4);
      count++;
    }
    i += gvl;
  }
}


#define N (30 * 1024)
double dx[N];
double dy[N];

int main(int argc, char *argv[])
{
  double a=1.0;

  // warmup
  axpy_intrinsics(a, dx, dy, N);

  // Start instruction and cycles count of the region of interest
  unsigned long cycles1, cycles2, instr2, instr1;
  instr1 = read_csr(minstret);
  cycles1 = read_csr(mcycle);

  axpy_intrinsics(a, dx, dy, N);

  asm volatile("fence");
  // End instruction and cycles count of the region of interest
  instr2 = read_csr(minstret);
  cycles2 = read_csr(mcycle);
  // Instruction and cycles count of the region of interest
  printf("-CSR   NUMBER OF EXEC CYCLES :%lu\n", cycles2 - cycles1);
  printf("-CSR   NUMBER OF INSTRUCTIONS EXECUTED :%lu\n", instr2 - instr1);

  return 0;
}
