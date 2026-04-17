#include <stdio.h>
#include <stdint.h>

#define USE_LMUL8 1      /* set 1 to use m8 (128 elements per vector op) */

#if USE_LMUL8
#define N 128
#define VSET_E32_M(vl, avl) \
    asm volatile("vsetvli %0, %1, e32, m8, ta, ma" : "=r"(vl) : "r"(avl))
#define VA "v8"
#define VB "v16"
#define VT "v24"
#define VO "v4"
#define VR "v5"
#define VS "v6"
#define VU "v7"
#elif USE_LMUL4
#define N 64
#define VSET_E32_M(vl, avl) \
    asm volatile("vsetvli %0, %1, e32, m4, ta, ma" : "=r"(vl) : "r"(avl))
#define VA "v8"
#define VB "v12"
#define VT "v16"
#define VO "v20"
#define VR "v24"
#define VS "v28"
#define VU "v4"
#elif USE_LMUL_TINY
#define N 2
#define VSET_E32_M(vl, avl) \
    asm volatile("vsetvli %0, %1, e32, m1, ta, ma" : "=r"(vl) : "r"(avl))
#define VA "v8"
#define VB "v9"
#define VT "v10"
#define VO "v16"
#define VR "v11"
#define VS "v12"
#define VU "v13"
#else
#define N 16
#define VSET_E32_M(vl, avl) \
    asm volatile("vsetvli %0, %1, e32, m1, ta, ma" : "=r"(vl) : "r"(avl))
#define VA "v8"
#define VB "v9"
#define VT "v10"
#define VO "v16"
#define VR "v11"
#define VS "v12"
#define VU "v13"
#endif
#define TCM_BASE  0x70000000U

static inline uint64_t read_cycles(void) {
    uint64_t c; 
    asm volatile("rdcycle %0" : "=r"(c)); 
    return c;
}

static uint32_t a[N];
static uint32_t b[N];
static uint32_t c[N];
static uint32_t out[N];
static uint32_t tmp[N];

void init_data(void) {
    for (int i = 0; i < N; i++) {
        a[i] = i + 1;
        b[i] = (i + 1) * 2;
        c[i] = i + 100;
    }
}

#define VSET_E32_M1 VSET_E32_M

int main(void) {
    size_t vl;
    uint64_t s;

    init_data();

    printf("\n=== NO-MASK CONTROL TEST (SAME INSTRUCTION COUNT) ===\n");
    printf("Vector length N = %d\n", N);

    /* Test 1: 5 条指令链，无 mask */
    printf("\n--- Test 1: 5-instruction chain (NO MASK) ---\n");
    VSET_E32_M1(vl, N);
    asm volatile(
        "vle32.v " VA ", (%0)\n\t"
        "vle32.v " VB ", (%1)\n\t"
        "vle32.v " VS ", (%2)\n\t"
        : : "r"(a), "r"(b), "r"(c) : "memory", VA, VB, VS
    );

    s = read_cycles();
    asm volatile(
        /* 原: vmsne.vv v0, VA, VB (去掉) */
        "vadd.vv " VT ", " VA ", " VB "\n\t"        /* 1. temp = a + b (原masked, 现unmasked) */
        "vsub.vv " VT ", " VT ", " VA "\n\t"        /* 2. temp = temp - a */
        "vadd.vv " VT ", " VT ", " VS "\n\t"        /* 3. temp = temp + c */
        "vmul.vv " VO ", " VT ", " VA "\n\t"        /* 4. out = temp * a */
        "vadd.vv " VO ", " VO ", " VB "\n\t"        /* 5. extra add to match count (原vmul masked) */
        : : : VA, VB, VS, VT, VO, "memory"
    );
    uint64_t t1 = read_cycles() - s;

    asm volatile("vse32.v " VO ", (%0)\n\t" : : "r"(out) : "memory", VO);
    printf("cycles (5-inst, NO mask): %lu\n", t1);

    /* Test 2: 6 条指令，无 mask */
    printf("\n--- Test 2: 6-instruction chain (NO MASK) ---\n");
    for (int i = 0; i < N; i++) {
        b[i] = (i % 2 == 0) ? a[i] : a[i] + 100;
    }

    VSET_E32_M1(vl, N);
    asm volatile(
        "vle32.v " VA ", (%0)\n\t"
        "vle32.v " VB ", (%1)\n\t"
        "vle32.v " VS ", (%2)\n\t"
        : : "r"(a), "r"(b), "r"(c) : "memory", VA, VB, VS
    );

    s = read_cycles();
    asm volatile(
        /* 原: vmsne.vv v0, VA, VB (去掉) */
        "vadd.vv " VT ", " VA ", " VB "\n\t"        /* 1. unmasked add */
        "vsub.vv " VT ", " VT ", " VA "\n\t"        /* 2. sub (原masked) */
        "vmul.vv " VU ", " VT ", " VS "\n\t"        /* 3. mul (原unmasked) */
        "vadd.vv " VU ", " VU ", " VA "\n\t"        /* 4. add (原masked) */
        "vsub.vv " VO ", " VU ", " VS "\n\t"        /* 5. sub (原masked) */
        "vadd.vv " VO ", " VO ", " VB "\n\t"        /* 6. extra add to match count */
        : : : VA, VB, VS, VT, VU, VO, "memory"
    );
    uint64_t t2 = read_cycles() - s;

    asm volatile("vse32.v " VO ", (%0)\n\t" : : "r"(out) : "memory", VO);
    printf("cycles (6-inst, NO mask): %lu\n", t2);

    /* Test 3: 7 条指令，无 mask */
    printf("\n--- Test 3: 7-instruction chain (NO MASK) ---\n");
    for (int i = 0; i < N; i++) { a[i] = i+1; b[i] = i+101; }

    VSET_E32_M1(vl, N);
    asm volatile(
        "vle32.v " VA ", (%0)\n\t"
        "vle32.v " VB ", (%1)\n\t"
        "vle32.v " VS ", (%2)\n\t"
        : : "r"(a), "r"(b), "r"(c) : "memory", VA, VB, VS
    );

    s = read_cycles();
    asm volatile(
        /* 原: vmsgt.vx v0, VA, 8 (去掉) */
        "vadd.vv " VT ", " VA ", " VB "\n\t"        /* 1. add (原masked) */
        "vsub.vv " VT ", " VT ", " VA "\n\t"        /* 2. sub (原masked) */
        /* 原: vmsgt.vx v0, VT, 50 (去掉) */
        "vmul.vv " VU ", " VT ", " VS "\n\t"        /* 3. mul (原masked) */
        "vadd.vv " VU ", " VU ", " VA "\n\t"        /* 4. add (原masked) */
        "vsub.vv " VO ", " VU ", " VS "\n\t"        /* 5. sub (原masked) */
        "vadd.vv " VO ", " VO ", " VB "\n\t"        /* 6. extra */
        "vmul.vv " VO ", " VO ", " VA "\n\t"        /* 7. extra mul to match count */
        : : : VA, VB, VS, VT, VU, VO, "memory"
    );
    uint64_t t3 = read_cycles() - s;

    asm volatile("vse32.v " VO ", (%0)\n\t" : : "r"(out) : "memory", VO);
    printf("cycles (7-inst, NO mask): %lu\n", t3);

    /* Test 4: 8 条指令深层链，无 mask */
    printf("\n--- Test 4: 8-instruction deep chain (NO MASK) ---\n");
    for (int i = 0; i < N; i++) { a[i] = i+1; b[i] = i+2; c[i] = i+10; }

    VSET_E32_M1(vl, N);
    asm volatile(
        "vle32.v " VA ", (%0)\n\t"
        "vle32.v " VB ", (%1)\n\t"
        "vle32.v " VS ", (%2)\n\t"
        : : "r"(a), "r"(b), "r"(c) : "memory", VA, VB, VS
    );

    s = read_cycles();
    asm volatile(
        "vadd.vv " VT ", " VA ", " VB "\n\t"        /* 1. temp1 = a + b */
        /* 原: vmsgt.vx v0, VT, TH (去掉) */
        "vadd.vv " VT ", " VT ", " VB "\n\t"        /* 2. add (原masked) */
        "vsub.vv " VU ", " VT ", " VA "\n\t"        /* 3. sub (原masked) */
        "vmul.vv " VU ", " VU ", " VS "\n\t"        /* 4. mul (原masked) */
        "vadd.vv " VU ", " VU ", " VB "\n\t"        /* 5. add (原masked) */
        "vsub.vv " VT ", " VU ", " VA "\n\t"        /* 6. sub (原masked) */
        "vadd.vv " VO ", " VT ", " VS "\n\t"        /* 7. add (原masked) */
        "vmul.vv " VO ", " VO ", " VA "\n\t"        /* 8. extra mul to match count */
        : : : VA, VB, VS, VT, VU, VO, "memory"
    );
    uint64_t t4 = read_cycles() - s;

    asm volatile("vse32.v " VO ", (%0)\n\t" : : "r"(out) : "memory", VO);
    printf("cycles (8-inst, NO mask): %lu\n", t4);

    /* Test 5: 8 条指令 + store/reload，无 mask */
    printf("\n--- Test 5: 8-instruction NO chaining (store/reload, NO MASK) ---\n");
    for (int i = 0; i < N; i++) { a[i] = i+1; b[i] = i+2; c[i] = i+10; tmp[i] = 0; }

    VSET_E32_M1(vl, N);
    asm volatile(
        "vle32.v " VA ", (%0)\n\t"
        "vle32.v " VB ", (%1)\n\t"
        "vle32.v " VS ", (%2)\n\t"
        : : "r"(a), "r"(b), "r"(c) : "memory", VA, VB, VS
    );

    s = read_cycles();
    asm volatile(
        "vadd.vv " VT ", " VA ", " VB "\n\t"
        "vse32.v " VT ", (%0)\n\t"
        "vle32.v " VT ", (%0)\n\t"
        /* 原: vmsgt.vx v0, VT, TH2 (去掉) */
        "vadd.vv " VT ", " VT ", " VB "\n\t"
        "vse32.v " VT ", (%0)\n\t"
        "vle32.v " VT ", (%0)\n\t"
        "vsub.vv " VU ", " VT ", " VA "\n\t"
        "vse32.v " VU ", (%0)\n\t"
        "vle32.v " VU ", (%0)\n\t"
        "vmul.vv " VU ", " VU ", " VS "\n\t"
        "vse32.v " VU ", (%0)\n\t"
        "vle32.v " VU ", (%0)\n\t"
        "vadd.vv " VU ", " VU ", " VB "\n\t"
        "vse32.v " VU ", (%0)\n\t"
        "vle32.v " VU ", (%0)\n\t"
        "vsub.vv " VT ", " VU ", " VA "\n\t"
        "vse32.v " VT ", (%0)\n\t"
        "vle32.v " VT ", (%0)\n\t"
        "vadd.vv " VO ", " VT ", " VS "\n\t"
        : : "r"(tmp) : "memory", VA, VB, VS, VT, VU, VO
    );
    uint64_t t5 = read_cycles() - s;

    asm volatile("vse32.v " VO ", (%0)\n\t" : : "r"(out) : "memory", VO);
    printf("cycles (8-inst NO chain, NO mask): %lu\n", t5);

    /* Test 6: 6 条指令 mul 链，无 mask */
    printf("\n--- Test 6: 6-instruction mul chain (NO MASK) ---\n");
    for (int i = 0; i < N; i++) { a[i] = i+1; b[i] = i+2; c[i] = i+3; }

    VSET_E32_M1(vl, N);
    asm volatile(
        "vle32.v " VA ", (%0)\n\t"
        "vle32.v " VB ", (%1)\n\t"
        "vle32.v " VS ", (%2)\n\t"
        : : "r"(a), "r"(b), "r"(c) : "memory", VA, VB, VS
    );

    s = read_cycles();
    asm volatile(
        /* 原: vmsne.vv v0, VA, VB (去掉) */
        "vmul.vv " VT ", " VA ", " VB "\n\t"        /* 1. a * b (原masked) */
        "vadd.vv " VT ", " VT ", " VS "\n\t"        /* 2. + c (原masked) */
        "vmul.vv " VT ", " VT ", " VA "\n\t"        /* 3. * a (原masked) */
        "vsub.vv " VT ", " VT ", " VB "\n\t"        /* 4. - b (原masked) */
        "vmul.vv " VO ", " VT ", " VS "\n\t"        /* 5. * c (原masked) */
        "vadd.vv " VO ", " VO ", " VA "\n\t"        /* 6. extra add to match count */
        : : : VA, VB, VS, VT, VO, "memory"
    );
    uint64_t t6 = read_cycles() - s;

    asm volatile("vse32.v " VO ", (%0)\n\t" : : "r"(out) : "memory", VO);
    printf("cycles (6-inst mul, NO mask): %lu\n", t6);

    /* Summary */
    printf("\n=== NO-MASK SUMMARY ===\n");
    printf("Test 1 (5-inst, NO mask):     %lu cycles\n", t1);
    printf("Test 2 (6-inst, NO mask):     %lu cycles\n", t2);
    printf("Test 3 (7-inst, NO mask):     %lu cycles\n", t3);
    printf("Test 4 (8-inst deep, NO mask):%lu cycles\n", t4);
    printf("Test 5 (8-inst no-chain, NO): %lu cycles\n", t5);
    printf("Test 6 (6-inst mul, NO mask): %lu cycles\n", t6);

    return 0;
}