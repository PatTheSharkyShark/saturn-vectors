// ============================================================
// Saturn Vector TCM Memory Access Benchmark
// Compares: Main Memory vs Vector TCM (0x70000000, 64KB)
// for Unit-stride, Segmented, and Indexed vector accesses
//
// DSPV512D128ShuttleConfig:
//   VLEN=512, DLEN=128, Vector TCM=0x70000000 (64KB, 4 banks)
// ============================================================
#include <stdio.h>
#include <stdint.h>
#include "util.h"

// Vector TCM base address (WithTCM default in ShuttleConfigs)
#define TCM_BASE   0x70000000UL
// TCM layout (each array = 256 x 4bytes = 1KB):
//   [0x000 - 0x3FF]: src    (256 elements)
//   [0x400 - 0x7FF]: indices (256 elements)
//   [0x800 - 0xBFF]: dest   (256 elements)
#define TCM_SRC  ((volatile uint32_t *)(TCM_BASE + 0x000))
#define TCM_IDX  ((volatile uint32_t *)(TCM_BASE + 0x400))
#define TCM_DEST ((volatile uint32_t *)(TCM_BASE + 0x800))

#define N     512   // Main memory elements
#define TCM_N 256   // TCM elements (16 vectors @ LMUL=1 with VLEN=512)

uint32_t src[N]     __attribute__((aligned(128)));
uint32_t dest[N]    __attribute__((aligned(128)));
uint32_t indices[N] __attribute__((aligned(128)));

int main() {
    size_t vl;

    // Initialize main memory
    for (int i = 0; i < N; i++) {
        src[i]     = i;
        indices[i] = ((i * 3) % N) * 4;
    }

    // Initialize TCM using scalar stores
    for (int i = 0; i < TCM_N; i++) {
        TCM_SRC[i]  = i;
        TCM_IDX[i]  = ((i * 3) % TCM_N) * 4;
        TCM_DEST[i] = 0;
    }
    asm volatile ("fence");

    uint64_t s, e;
    uint64_t mm_ul, mm_sl, mm_il, mm_us, mm_ss, mm_is;
    uint64_t tcm_ul, tcm_sl, tcm_il, tcm_us, tcm_ss, tcm_is;

    // ========================================================
    // MAIN MEMORY TESTS (N=512)
    // ========================================================

    // Unit Stride Load
    s = read_csr(mcycle);
    { size_t n = N; uint32_t *p = src;
      while (n > 0) {
        asm volatile ("vsetvli %0, %1, e32, m1, ta, ma" : "=r"(vl) : "r"(n));
        asm volatile ("vle32.v v2, (%0)" :: "r"(p) : "v2");
        p += vl; n -= vl; } }
    asm volatile ("fence"); e = read_csr(mcycle); mm_ul = e - s;

    // Segmented Load (nf=2)
    s = read_csr(mcycle);
    { size_t ns = N / 2; uint32_t *p = src;
      while (ns > 0) {
        asm volatile ("vsetvli %0, %1, e32, m1, ta, ma" : "=r"(vl) : "r"(ns));
        asm volatile ("vlseg2e32.v v2, (%0)" :: "r"(p) : "v2", "v3");
        p += vl * 2; ns -= vl; } }
    asm volatile ("fence"); e = read_csr(mcycle); mm_sl = e - s;

    // Indexed Load
    s = read_csr(mcycle);
    { size_t n = N; uint32_t *ip = indices;
      while (n > 0) {
        asm volatile ("vsetvli %0, %1, e32, m1, ta, ma" : "=r"(vl) : "r"(n));
        asm volatile ("vle32.v v4, (%0)" :: "r"(ip) : "v4");
        asm volatile ("vluxei32.v v2, (%0), v4" :: "r"(src) : "v2");
        ip += vl; n -= vl; } }
    asm volatile ("fence"); e = read_csr(mcycle); mm_il = e - s;

    // Unit Stride Store
    s = read_csr(mcycle);
    { size_t n = N; uint32_t *p = dest;
      while (n > 0) {
        asm volatile ("vsetvli %0, %1, e32, m1, ta, ma" : "=r"(vl) : "r"(n));
        asm volatile ("vse32.v v2, (%0)" :: "r"(p) : "memory");
        p += vl; n -= vl; } }
    asm volatile ("fence"); e = read_csr(mcycle); mm_us = e - s;

    // Segmented Store (nf=2)
    s = read_csr(mcycle);
    { size_t ns = N / 2; uint32_t *p = dest;
      while (ns > 0) {
        asm volatile ("vsetvli %0, %1, e32, m1, ta, ma" : "=r"(vl) : "r"(ns));
        asm volatile ("vsseg2e32.v v2, (%0)" :: "r"(p) : "memory");
        p += vl * 2; ns -= vl; } }
    asm volatile ("fence"); e = read_csr(mcycle); mm_ss = e - s;

    // Indexed Store
    s = read_csr(mcycle);
    { size_t n = N; uint32_t *ip = indices;
      while (n > 0) {
        asm volatile ("vsetvli %0, %1, e32, m1, ta, ma" : "=r"(vl) : "r"(n));
        asm volatile ("vle32.v v4, (%0)" :: "r"(ip) : "v4");
        asm volatile ("vsoxei32.v v2, (%0), v4" :: "r"(dest) : "memory");
        ip += vl; n -= vl; } }
    asm volatile ("fence"); e = read_csr(mcycle); mm_is = e - s;

    // ========================================================
    // VECTOR TCM TESTS (TCM_N=256, base=0x70000000)
    // ========================================================

    // Unit Stride Load from TCM
    s = read_csr(mcycle);
    { size_t n = TCM_N; volatile uint32_t *p = TCM_SRC;
      while (n > 0) {
        asm volatile ("vsetvli %0, %1, e32, m1, ta, ma" : "=r"(vl) : "r"(n));
        asm volatile ("vle32.v v2, (%0)" :: "r"(p) : "v2");
        p += vl; n -= vl; } }
    asm volatile ("fence"); e = read_csr(mcycle); tcm_ul = e - s;

    // Segmented Load from TCM (nf=2)
    s = read_csr(mcycle);
    { size_t ns = TCM_N / 2; volatile uint32_t *p = TCM_SRC;
      while (ns > 0) {
        asm volatile ("vsetvli %0, %1, e32, m1, ta, ma" : "=r"(vl) : "r"(ns));
        asm volatile ("vlseg2e32.v v2, (%0)" :: "r"(p) : "v2", "v3");
        p += vl * 2; ns -= vl; } }
    asm volatile ("fence"); e = read_csr(mcycle); tcm_sl = e - s;

    // Indexed Load from TCM
    s = read_csr(mcycle);
    { size_t n = TCM_N; volatile uint32_t *ip = TCM_IDX;
      while (n > 0) {
        asm volatile ("vsetvli %0, %1, e32, m1, ta, ma" : "=r"(vl) : "r"(n));
        asm volatile ("vle32.v v4, (%0)" :: "r"(ip) : "v4");
        asm volatile ("vluxei32.v v2, (%0), v4" :: "r"(TCM_SRC) : "v2");
        ip += vl; n -= vl; } }
    asm volatile ("fence"); e = read_csr(mcycle); tcm_il = e - s;

    // Unit Stride Store to TCM
    s = read_csr(mcycle);
    { size_t n = TCM_N; volatile uint32_t *p = TCM_DEST;
      while (n > 0) {
        asm volatile ("vsetvli %0, %1, e32, m1, ta, ma" : "=r"(vl) : "r"(n));
        asm volatile ("vse32.v v2, (%0)" :: "r"(p) : "memory");
        p += vl; n -= vl; } }
    asm volatile ("fence"); e = read_csr(mcycle); tcm_us = e - s;

    // Segmented Store to TCM (nf=2)
    s = read_csr(mcycle);
    { size_t ns = TCM_N / 2; volatile uint32_t *p = TCM_DEST;
      while (ns > 0) {
        asm volatile ("vsetvli %0, %1, e32, m1, ta, ma" : "=r"(vl) : "r"(ns));
        asm volatile ("vsseg2e32.v v2, (%0)" :: "r"(p) : "memory");
        p += vl * 2; ns -= vl; } }
    asm volatile ("fence"); e = read_csr(mcycle); tcm_ss = e - s;

    // Indexed Store to TCM
    s = read_csr(mcycle);
    { size_t n = TCM_N; volatile uint32_t *ip = TCM_IDX;
      while (n > 0) {
        asm volatile ("vsetvli %0, %1, e32, m1, ta, ma" : "=r"(vl) : "r"(n));
        asm volatile ("vle32.v v4, (%0)" :: "r"(ip) : "v4");
        asm volatile ("vsoxei32.v v2, (%0), v4" :: "r"(TCM_DEST) : "memory");
        ip += vl; n -= vl; } }
    asm volatile ("fence"); e = read_csr(mcycle); tcm_is = e - s;

    // ========================================================
    // Print results
    // ========================================================
    printf("=== Main Memory (N=%d) ===\n", N);
    printf("UnitLoad:  %lu cyc (%lu/elem)\n", mm_ul,  mm_ul/N);
    printf("SegLoad:   %lu cyc (%lu/elem)\n", mm_sl,  mm_sl/N);
    printf("IdxLoad:   %lu cyc (%lu/elem)\n", mm_il,  mm_il/N);
    printf("UnitStore: %lu cyc (%lu/elem)\n", mm_us,  mm_us/N);
    printf("SegStore:  %lu cyc (%lu/elem)\n", mm_ss,  mm_ss/N);
    printf("IdxStore:  %lu cyc (%lu/elem)\n", mm_is,  mm_is/N);
    printf("=== Vector TCM 0x70000000 (N=%d) ===\n", TCM_N);
    printf("UnitLoad:  %lu cyc (%lu/elem)\n", tcm_ul, tcm_ul/TCM_N);
    printf("SegLoad:   %lu cyc (%lu/elem)\n", tcm_sl, tcm_sl/TCM_N);
    printf("IdxLoad:   %lu cyc (%lu/elem)\n", tcm_il, tcm_il/TCM_N);
    printf("UnitStore: %lu cyc (%lu/elem)\n", tcm_us, tcm_us/TCM_N);
    printf("SegStore:  %lu cyc (%lu/elem)\n", tcm_ss, tcm_ss/TCM_N);
    printf("IdxStore:  %lu cyc (%lu/elem)\n", tcm_is, tcm_is/TCM_N);

    return 0;
}
