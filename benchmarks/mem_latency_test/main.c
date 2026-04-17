#include <stdio.h>
#include <stdint.h>
#include "util.h"

// Define total number of elements to process
#define N 512

// Align to 128 bytes (1024 bits) to be safe
uint32_t src[N] __attribute__((aligned(128)));
uint32_t dest[N] __attribute__((aligned(128)));
uint32_t indices[N] __attribute__((aligned(128)));

int main() {
    size_t vl;
    // Initialize data
    for (int i = 0; i < N; i++) {
        src[i] = i;
        indices[i] = ((i * 3) % N) * 4; 
    }

    uint64_t start, end;
    uint64_t unit_load = 0, indexed_load = 0, segmented_load = 0;
    uint64_t unit_store = 0, indexed_store = 0, segmented_store = 0;
    
    // We will measure processing the ENTIRE array N elements using strip-mining loop
    
    // ---------------------------------------------------------
    // 2. Unit Stride Load (vle32.v)
    // ---------------------------------------------------------
    start = read_csr(mcycle);
    {
        size_t n = N;
        uint32_t *ptr = src;
        while (n > 0) {
            asm volatile ("vsetvli %0, %1, e32, m2, ta, ma" : "=r"(vl) : "r"(n));
            asm volatile ("vle32.v v2, (%0)" : : "r"(ptr) : "v2", "v3");
            ptr += vl;
            n -= vl;
        }
    }
    asm volatile ("fence"); 
    end = read_csr(mcycle);
    unit_load = end - start;

    // ---------------------------------------------------------
    // 3. Indexed Load (vluxei32.v) - Gather
    // ---------------------------------------------------------
    start = read_csr(mcycle);
    {
        size_t n = N;
        uint32_t *i_ptr = indices; // Pointer to current batch of indices
        while (n > 0) {
            asm volatile ("vsetvli %0, %1, e32, m2, ta, ma" : "=r"(vl) : "r"(n));
            
            // Load indices into vector register v4 (m2 means v4, v5)
            // Note: input indices array is pre-calculated byte offsets.
            // But are they global offsets or relative to current ptr?
            // Global offsets: indices[i] = ((i * 3) % N) * 4.
            // When we process batch 2 (e.g., elements 32-63), i_ptr points to indices[32].
            // These indices are e.g. ((32*3)%N)*4 ...
            // These are strictly valid offsets into 'src'.
            // So base register for vluxei32 MUST be 'src'.
            
            asm volatile ("vle32.v v4, (%0)" : : "r"(i_ptr) : "v4", "v5");
            
            // Perform indexed load into v2 (m2 means v2, v3)
            asm volatile ("vluxei32.v v2, (%0), v4" : : "r"(src) : "v2", "v3"); 
            
            i_ptr += vl;
            n -= vl;
        }
    }
    asm volatile ("fence");
    end = read_csr(mcycle);
    indexed_load = end - start;

    // ---------------------------------------------------------
    // 4. Segmented Load (vlseg2e32.v)
    // ---------------------------------------------------------
    start = read_csr(mcycle);
    {
        // For segmented load of 2 fields, we process N/2 "structures".
        size_t n_structs = N / 2;
        uint32_t *ptr = src;
        while (n_structs > 0) {
            // Use LMUL=1 for each field. Total consumption is 2 * LMUL = 2 registers.
            // Using v2 (field 0), v3 (field 1).
            asm volatile ("vsetvli %0, %1, e32, m1, ta, ma" : "=r"(vl) : "r"(n_structs));
            
            asm volatile ("vlseg2e32.v v2, (%0)" : : "r"(ptr) : "v2", "v3");
            
            ptr += vl * 2; // Advance by 2 fields * VL
            n_structs -= vl;
        }
    }
    asm volatile ("fence");
    end = read_csr(mcycle);
    segmented_load = end - start;

    // ---------------------------------------------------------
    // 5. Unit Stride Store (vse32.v)
    // ---------------------------------------------------------
    start = read_csr(mcycle);
    {
        size_t n = N;
        uint32_t *ptr = dest;
        while (n > 0) {
            asm volatile ("vsetvli %0, %1, e32, m2, ta, ma" : "=r"(vl) : "r"(n));
            asm volatile ("vse32.v v2, (%0)" : : "r"(ptr) : "memory");
            ptr += vl;
            n -= vl;
        }
    }
    asm volatile ("fence");
    end = read_csr(mcycle);
    unit_store = end - start;

    // ---------------------------------------------------------
    // 6. Indexed Store (vsoxei32.v) - Scatter
    // ---------------------------------------------------------
    start = read_csr(mcycle);
    {
        size_t n = N;
        uint32_t *i_ptr = indices;
        
        while (n > 0) {
            asm volatile ("vsetvli %0, %1, e32, m2, ta, ma" : "=r"(vl) : "r"(n));
            
             // Load indices
            asm volatile ("vle32.v v4, (%0)" : : "r"(i_ptr) : "v4", "v5");
            
            // Scatter store to dest + indices
            asm volatile ("vsoxei32.v v2, (%0), v4" : : "r"(dest) : "memory");
            
            i_ptr += vl;
            n -= vl;
        }
    }
    asm volatile ("fence");
    end = read_csr(mcycle);
    indexed_store = end - start;

    // ---------------------------------------------------------
    // 7. Segmented Store (vsseg2e32.v)
    // ---------------------------------------------------------
    start = read_csr(mcycle);
    {
        size_t n_structs = N / 2;
        uint32_t *ptr = dest;
        while (n_structs > 0) {
            asm volatile ("vsetvli %0, %1, e32, m1, ta, ma" : "=r"(vl) : "r"(n_structs));
            asm volatile ("vsseg2e32.v v2, (%0)" : : "r"(ptr) : "memory");
            ptr += vl * 2;
            n_structs -= vl;
        }
    }
    asm volatile ("fence");
    end = read_csr(mcycle);
    segmented_store = end - start;

    // Print results
    printf("CYCLES (Elem=%d):\n", N);
    printf("UnitLoad: %lu\n", unit_load);
    printf("IdxLoad:  %lu\n", indexed_load);
    printf("SegLoad:  %lu\n", segmented_load); // Note: processing same amount of DATA, but loop is slightly different
    printf("UnitStore: %lu\n", unit_store);
    printf("IdxStore:  %lu\n", indexed_store);
    printf("SegStore:  %lu\n", segmented_store);

    return 0;
}
