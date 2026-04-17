// Copyright 2020 ETH Zurich and University of Bologna.
//
// SPDX-License-Identifier: Apache-2.0
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//    http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

// Author: Matheus Cavalcante, ETH Zurich
//         Samuel Riedel, ETH Zurich

#include <stdio.h>
#include "imatmul.h"
#include "util.h"

#define MIN(a, b) ((a) < (b) ? (a) : (b))

// Profiling buffers for TCM access tracking
#ifndef MAX_TRACE
#define MAX_TRACE 512
#endif

typedef struct {
  unsigned long start_c, end_c;
  unsigned long start_i, end_i;
  unsigned long addr;
  const char *name;
} tcm_trace_t;

volatile tcm_trace_t tcm_trace_data[MAX_TRACE];
volatile int trace_idx = 0;

static inline void record_instr(const char *name, unsigned long addr) {
  if (trace_idx < MAX_TRACE) {
    int idx = trace_idx++;
    tcm_trace_data[idx].name = name;
    tcm_trace_data[idx].addr = addr;
    tcm_trace_data[idx].start_c = read_csr(mcycle);
    tcm_trace_data[idx].start_i = read_csr(minstret);
  }
}

static inline void stop_instr() {
  int idx = trace_idx - 1;
  if (idx >= 0 && idx < MAX_TRACE) {
    tcm_trace_data[idx].end_c = read_csr(mcycle);
    tcm_trace_data[idx].end_i = read_csr(minstret);
  }
}

void imatmul_vec_4x4(int64_t *c, const int64_t *a, const int64_t *b,
                     const unsigned long int N, const unsigned long int P);
void imatmul_vec_4x4_slice_init();
void imatmul_vec_8x8(int64_t *c, const int64_t *a, const int64_t *b,
                     const unsigned long int N, const unsigned long int P);
void imatmul_vec_8x8_slice_init();

void imatmul_4x4(int64_t *c, const int64_t *a, const int64_t *b,
                 const unsigned long int M, const unsigned long int N,
                 const unsigned long int P);
void imatmul_8x8(int64_t *c, const int64_t *a, const int64_t *b,
                 const unsigned long int M, const unsigned long int N,
                 const unsigned long int P);

void imatmul(int64_t *c, const int64_t *a, const int64_t *b,
             const unsigned long int M, const unsigned long int N,
             const unsigned long int P) {
  if (M <= 4) {
    imatmul_4x4(c, a, b, M, N, P);
  } else if (M <= 128) {
    imatmul_8x8(c, a, b, M, N, P);
  } else {
    imatmul_4x4(c, a, b, M, N, P);
  }
}

// ---------------
// 4x4
// ---------------

void imatmul_4x4(int64_t *c, const int64_t *a, const int64_t *b,
                 const unsigned long int M, const unsigned long int N,
                 const unsigned long int P) {
  const unsigned long int block_size = 4;
  unsigned long int block_size_p;
  asm volatile("vsetvli %0, %1, e64, m4, ta, ma" : "=r"(block_size_p) : "r"(P));

  for (unsigned long int p = 0; p < P; p += block_size_p) {
    const unsigned long int p_ = MIN(P - p, block_size_p);
    const int64_t *b_ = b + p;
    int64_t *c_ = c + p;
    asm volatile("vsetvli zero, %0, e64, m4, ta, ma" ::"r"(p_));

    for (unsigned long int m = 0; m < M; m += block_size) {
      const int64_t *a_ = a + m * N;
      int64_t *c__ = c_ + m * P;
      imatmul_vec_4x4_slice_init();
      imatmul_vec_4x4(c__, a_, b_, N, P);
    }
  }
}

void imatmul_vec_4x4_slice_init() {
  asm volatile("vmv.v.i v0,  0");
  asm volatile("vmv.v.i v4,  0");
  asm volatile("vmv.v.i v8,  0");
  asm volatile("vmv.v.i v12, 0");
}

void imatmul_vec_4x4(int64_t *c, const int64_t *a, const int64_t *b,
                     const unsigned long int N, const unsigned long int P) {
  int64_t t0, t1, t2, t3;
  const int64_t *a_ = a;
  write_csr(mscratch, 0x1111); record_instr("VLE_PREFETCH", (unsigned long)b);
  asm volatile("vle64.v v16, (%0);" ::"r"(b));
  stop_instr(); write_csr(mscratch, 0); write_csr(mscratch, 0);
  b += P;

  t0 = *a, a += N;
  t1 = *a, a += N;
  t2 = *a, a += N;
  t3 = *a;

  unsigned long int n = 0;
  while (n < N) {
    a = a_ + ++n;
    write_csr(mscratch, 0x2222); write_csr(mscratch, 0x2222); record_instr("VMACC_0", 0);
    asm volatile("vmacc.vx v0, %0, v16" ::"r"(t0));
    stop_instr(); write_csr(mscratch, 0); write_csr(mscratch, 0);
    t0 = *a, a += N;

    write_csr(mscratch, 0x1111); record_instr("VLE_B", (unsigned long)b);
    asm volatile("vle64.v v20, (%0);" ::"r"(b));
    stop_instr(); write_csr(mscratch, 0); write_csr(mscratch, 0);
    b += P;

    write_csr(mscratch, 0x2222); write_csr(mscratch, 0x2222); record_instr("VMACC_1_3", 0);
    asm volatile("vmacc.vx v4, %0, v16" ::"r"(t1));
    asm volatile("vmacc.vx v8, %0, v16" ::"r"(t2));
    asm volatile("vmacc.vx v12, %0, v16" ::"r"(t3));
    stop_instr(); write_csr(mscratch, 0); write_csr(mscratch, 0);
    t3 = *a;

    a = a_ + ++n;
    if (n == N) break;

    write_csr(mscratch, 0x2222); write_csr(mscratch, 0x2222); record_instr("VMACC_0_ALT", 0);
    asm volatile("vmacc.vx v0, %0, v20" ::"r"(t0));
    stop_instr(); write_csr(mscratch, 0); write_csr(mscratch, 0);
    t0 = *a, a += N;

    write_csr(mscratch, 0x1111); record_instr("VLE_B_ALT", (unsigned long)b);
    asm volatile("vle64.v v16, (%0);" ::"r"(b));
    stop_instr(); write_csr(mscratch, 0); write_csr(mscratch, 0);
    b += P;

    write_csr(mscratch, 0x2222); write_csr(mscratch, 0x2222); record_instr("VMACC_1_3_ALT", 0);
    asm volatile("vmacc.vx v4, %0, v20" ::"r"(t1));
    asm volatile("vmacc.vx v8, %0, v20" ::"r"(t2));
    asm volatile("vmacc.vx v12, %0, v20" ::"r"(t3));
    stop_instr(); write_csr(mscratch, 0); write_csr(mscratch, 0);
    t3 = *a;
  }

  write_csr(mscratch, 0x2222); write_csr(mscratch, 0x2222); record_instr("VMACC_FINAL", 0);
  asm volatile("vmacc.vx v4, %0, v20" ::"r"(t1));
  asm volatile("vmacc.vx v8, %0, v20" ::"r"(t2));
  asm volatile("vmacc.vx v12, %0, v20" ::"r"(t3));
  stop_instr(); write_csr(mscratch, 0); write_csr(mscratch, 0);

  write_csr(mscratch, 0x1111); record_instr("VSE_0", (unsigned long)c);
  asm volatile("vse64.v v0, (%0);" ::"r"(c));
  stop_instr(); write_csr(mscratch, 0); write_csr(mscratch, 0);
  c += P;

  write_csr(mscratch, 0x2222); write_csr(mscratch, 0x2222); record_instr("VMACC_V4", 0);
  asm volatile("vmv.v.v v0, v4");
  stop_instr(); write_csr(mscratch, 0); write_csr(mscratch, 0);

  write_csr(mscratch, 0x1111); record_instr("VSE_1", (unsigned long)c);
  asm volatile("vse64.v v0, (%0);" ::"r"(c));
  stop_instr(); write_csr(mscratch, 0); write_csr(mscratch, 0);
  c += P;

  write_csr(mscratch, 0x2222); write_csr(mscratch, 0x2222); record_instr("VMACC_V8", 0);
  asm volatile("vmv.v.v v0, v8");
  stop_instr(); write_csr(mscratch, 0); write_csr(mscratch, 0);

  write_csr(mscratch, 0x1111); record_instr("VSE_2", (unsigned long)c);
  asm volatile("vse64.v v0, (%0);" ::"r"(c));
  stop_instr(); write_csr(mscratch, 0); write_csr(mscratch, 0);
  c += P;

  write_csr(mscratch, 0x2222); write_csr(mscratch, 0x2222); record_instr("VMACC_V12", 0);
  asm volatile("vmv.v.v v0, v12");
  stop_instr(); write_csr(mscratch, 0); write_csr(mscratch, 0);

  write_csr(mscratch, 0x1111); record_instr("VSE_3", (unsigned long)c);
  asm volatile("vse64.v v0, (%0);" ::"r"(c));
  stop_instr(); write_csr(mscratch, 0); write_csr(mscratch, 0);
}

// ---------------
// 8x8
// ---------------

void imatmul_8x8(int64_t *c, const int64_t *a, const int64_t *b,
                 const unsigned long int M, const unsigned long int N,
                 const unsigned long int P) {
  const unsigned long int block_size = 8;
  unsigned long int block_size_p;
  asm volatile("vsetvli %0, %1, e64, m2, ta, ma" : "=r"(block_size_p) : "r"(P));

  for (unsigned long int p = 0; p < P; p += block_size_p) {
    const unsigned long int p_ = MIN(P - p, block_size_p);
    const int64_t *b_ = b + p;
    int64_t *c_ = c + p;
    asm volatile("vsetvli zero, %0, e64, m2, ta, ma" ::"r"(p_));
    for (unsigned long int m = 0; m < M; m += block_size) {
      const int64_t *a_ = a + m * N;
      int64_t *c__ = c_ + m * P;
      imatmul_vec_8x8_slice_init();
      imatmul_vec_8x8(c__, a_, b_, N, P);
    }
  }
}

void imatmul_vec_8x8_slice_init() {
  asm volatile("vmv.v.i v0,  0");
  asm volatile("vmv.v.i v2,  0");
  asm volatile("vmv.v.i v4,  0");
  asm volatile("vmv.v.i v6,  0");
  asm volatile("vmv.v.i v8,  0");
  asm volatile("vmv.v.i v10, 0");
  asm volatile("vmv.v.i v12, 0");
  asm volatile("vmv.v.i v14, 0");
}

void imatmul_vec_8x8(int64_t *c, const int64_t *a, const int64_t *b,
                     const unsigned long int N, const unsigned long int P) {
  int64_t t0, t1, t2, t3, t4, t5, t6, t7;
  const int64_t *a_ = a;
  write_csr(mscratch, 0x1111); record_instr("VLE_8x8_PRE", (unsigned long)b);
  asm volatile("vle64.v v18, (%0);" ::"r"(b));
  stop_instr(); write_csr(mscratch, 0); write_csr(mscratch, 0);
  b += P;

  t0 = *a, a += N;
  t1 = *a, a += N;
  t2 = *a, a += N;
  t3 = *a, a += N;
  t4 = *a, a += N;
  t5 = *a, a += N;
  t6 = *a, a += N;
  t7 = *a;

  unsigned long int n = 0;
  while (n < N) {
    a = a_ + ++n;
    write_csr(mscratch, 0x2222); write_csr(mscratch, 0x2222); record_instr("VMACC_8x8_0", 0);
    asm volatile("vmacc.vx v0, %0, v18" ::"r"(t0));
    stop_instr(); write_csr(mscratch, 0); write_csr(mscratch, 0);
    t0 = *a, a += N;

    write_csr(mscratch, 0x1111); record_instr("VLE_8x8_B", (unsigned long)b);
    asm volatile("vle64.v v20, (%0);" ::"r"(b));
    stop_instr(); write_csr(mscratch, 0); write_csr(mscratch, 0);
    b += P;

    write_csr(mscratch, 0x2222); write_csr(mscratch, 0x2222); record_instr("VMACC_8x8_1_7", 0);
    asm volatile("vmacc.vx v2, %0, v18" ::"r"(t1));
    asm volatile("vmacc.vx v4, %0, v18" ::"r"(t2));
    asm volatile("vmacc.vx v6, %0, v18" ::"r"(t3));
    asm volatile("vmacc.vx v8, %0, v18" ::"r"(t4));
    asm volatile("vmacc.vx v10, %0, v18" ::"r"(t5));
    asm volatile("vmacc.vx v12, %0, v18" ::"r"(t6));
    asm volatile("vmacc.vx v14, %0, v18" ::"r"(t7));
    stop_instr(); write_csr(mscratch, 0); write_csr(mscratch, 0);
    t7 = *a;

    a = a_ + ++n;
    if (n == N) break;

    write_csr(mscratch, 0x2222); write_csr(mscratch, 0x2222); record_instr("VMACC_8x8_0_A", 0);
    asm volatile("vmacc.vx v0, %0, v20" ::"r"(t0));
    stop_instr(); write_csr(mscratch, 0); write_csr(mscratch, 0);
    t0 = *a, a += N;

    write_csr(mscratch, 0x1111); record_instr("VLE_8x8_B_A", (unsigned long)b);
    asm volatile("vle64.v v18, (%0);" ::"r"(b));
    stop_instr(); write_csr(mscratch, 0); write_csr(mscratch, 0);
    b += P;

    write_csr(mscratch, 0x2222); write_csr(mscratch, 0x2222); record_instr("VMACC_8x8_1_7_A", 0);
    asm volatile("vmacc.vx v2, %0, v20" ::"r"(t1));
    asm volatile("vmacc.vx v4, %0, v20" ::"r"(t2));
    asm volatile("vmacc.vx v6, %0, v20" ::"r"(t3));
    asm volatile("vmacc.vx v8, %0, v20" ::"r"(t4));
    asm volatile("vmacc.vx v10, %0, v20" ::"r"(t5));
    asm volatile("vmacc.vx v12, %0, v20" ::"r"(t6));
    asm volatile("vmacc.vx v14, %0, v20" ::"r"(t7));
    stop_instr(); write_csr(mscratch, 0); write_csr(mscratch, 0);
    t7 = *a;
  }

  write_csr(mscratch, 0x2222); write_csr(mscratch, 0x2222); record_instr("VMACC_8x8_FINAL", 0);
  asm volatile("vmacc.vx v2, %0, v20" ::"r"(t1));
  asm volatile("vmacc.vx v4, %0, v20" ::"r"(t2));
  asm volatile("vmacc.vx v6, %0, v20" ::"r"(t3));
  asm volatile("vmacc.vx v8, %0, v20" ::"r"(t4));
  asm volatile("vmacc.vx v10, %0, v20" ::"r"(t5));
  asm volatile("vmacc.vx v12, %0, v20" ::"r"(t6));
  asm volatile("vmacc.vx v14, %0, v20" ::"r"(t7));
  stop_instr(); write_csr(mscratch, 0); write_csr(mscratch, 0);

  write_csr(mscratch, 0x1111); record_instr("VSE_8x8_0", (unsigned long)c);
  asm volatile("vse64.v v0, (%0);" ::"r"(c));
  stop_instr(); write_csr(mscratch, 0); write_csr(mscratch, 0);
  c += P;

  write_csr(mscratch, 0x1111); record_instr("VSE_8x8_1", (unsigned long)c);
  asm volatile("vse64.v v2, (%0);" ::"r"(c));
  stop_instr(); write_csr(mscratch, 0); write_csr(mscratch, 0);
  c += P;

  write_csr(mscratch, 0x1111); record_instr("VSE_8x8_2", (unsigned long)c);
  asm volatile("vse64.v v4, (%0);" ::"r"(c));
  stop_instr(); write_csr(mscratch, 0); write_csr(mscratch, 0);
  c += P;

  write_csr(mscratch, 0x1111); record_instr("VSE_8x8_3", (unsigned long)c);
  asm volatile("vse64.v v6, (%0);" ::"r"(c));
  stop_instr(); write_csr(mscratch, 0); write_csr(mscratch, 0);
  c += P;

  write_csr(mscratch, 0x1111); record_instr("VSE_8x8_4", (unsigned long)c);
  asm volatile("vse64.v v8, (%0);" ::"r"(c));
  stop_instr(); write_csr(mscratch, 0); write_csr(mscratch, 0);
  c += P;

  write_csr(mscratch, 0x1111); record_instr("VSE_8x8_5", (unsigned long)c);
  asm volatile("vse64.v v10, (%0);" ::"r"(c));
  stop_instr(); write_csr(mscratch, 0); write_csr(mscratch, 0);
  c += P;

  write_csr(mscratch, 0x1111); record_instr("VSE_8x8_6", (unsigned long)c);
  asm volatile("vse64.v v12, (%0);" ::"r"(c));
  stop_instr(); write_csr(mscratch, 0); write_csr(mscratch, 0);
  c += P;

  write_csr(mscratch, 0x1111); record_instr("VSE_8x8_7", (unsigned long)c);
  asm volatile("vse64.v v14, (%0);" ::"r"(c));
  stop_instr(); write_csr(mscratch, 0); write_csr(mscratch, 0);
}
