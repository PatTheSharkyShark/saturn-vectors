#include <stdio.h>
#include <stdint.h>

#define USE_LMUL8 1      /* set 1 to use m8 (256 elements per vector op) */

#if USE_LMUL8
#define N 256
#define VSET_E32_M(vl, avl) \
    asm volatile("vsetvli %0, %1, e32, m8, ta, ma" : "=r"(vl) : "r"(avl))
#define VA "v8"
#define VB "v16"
#define VT "v24"
#define VO "v4"
#define VR "v5"
#define VS "v6"    /* 新增临时寄存器 */
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
#define N 2  /* Single uop per instruction (DLEN=64bits, 2 e32 elements) */
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
static uint32_t c[N];    /* 新增数据源 */
static uint32_t out[N];
static uint32_t tmp[N];

void init_data(void) {
    for (int i = 0; i < N; i++) {
        a[i] = i + 1;
        b[i] = (i + 1) * 2;
        c[i] = i + 100;    /* 初始化 c */
    }
}

#define VSET_E32_M1(vl, avl) do { VSET_E32_M(vl, avl); printf("vsetvli returned vl = %lu\n", (unsigned long)(vl)); } while(0)

int main(void) {
    size_t vl;
    uint64_t s;

    init_data();

    printf("\n=== MASK CHAINING TEST (INCREASED INSTRUCTION COUNT) ===\n");
    printf("Vector length N = %d\n", N);
    /* Test 1: 扩展为 5 条指令链 (原 2 条: vmsne, vadd) */
    printf("\n--- Test 1: 5-instruction chain (vmsne->vadd->vsub->vadd->vmul) ---\n");
    VSET_E32_M1(vl, N);
    asm volatile(
        "vle32.v " VA ", (%0)\n\t"
        "vle32.v " VB ", (%1)\n\t"
        "vle32.v " VS ", (%2)\n\t"
        : : "r"(a), "r"(b), "r"(c) : "memory", "v0", VA, VB, VS
    );

    s = read_cycles();
    asm volatile(
        "vmsne.vv v0, " VA ", " VB "\n\t"
        "vadd.vv " VT ", " VA ", " VB ", v0.t\n\t"
        "vsub.vv " VT ", " VT ", " VA ", v0.t\n\t"
        "vadd.vv " VT ", " VT ", " VS ", v0.t\n\t"
        "vmul.vv " VO ", " VT ", " VA ", v0.t\n\t"
        : : : "v0", VA, VB, VS, VT, VO, "memory"
    );
    uint64_t t1 = read_cycles() - s;

    asm volatile("vse32.v " VO ", (%0)\n\t" : : "r"(out) : "memory", VO);
    printf("cycles (5-inst chain): %lu\n", t1);
    for (int i = 0; i < N/4; i++) {
            printf("out[%d:%d]: %u %u %u %u\n", i*4, i*4+3, out[i*4], out[i*4+1], out[i*4+2], out[i*4+3]);
    }

    /* Test 1.1: 5 条指令链，v0=mask, v1=普通数据输入，第3条起无mask */
    printf("\n--- Test 1.1: 5-instruction chain (vmsne->vadd(v1,masked)->vsub->vadd->vmul) ---\n");
    VSET_E32_M1(vl, N);
    asm volatile(
        "vle32.v " VA ", (%0)\n\t"
        "vle32.v " VB ", (%1)\n\t"
        "vle32.v v1, (%2)\n\t"
        : : "r"(a), "r"(b), "r"(c) : "memory", "v0", VA, VB, "v1"
    );

    s = read_cycles();
    asm volatile(
        "vmsne.vv v0, " VA ", " VB "\n\t"
        "vadd.vv " VT ", " VA ", v1, v0.t\n\t"
        "vsub.vv " VT ", " VT ", " VA "\n\t"
        "vadd.vv " VT ", " VT ", v1\n\t"
        "vmul.vv " VO ", " VT ", " VA "\n\t"
        : : : "v0", "v1", VA, VB, VT, VO, "memory"
    );
    uint64_t t1_1 = read_cycles() - s;  /* 改为 t1_1 */

    asm volatile("vse32.v " VO ", (%0)\n\t" : : "r"(out) : "memory", VO);
    printf("cycles (5-inst, v1=data, no mask after #2): %lu\n", t1_1);

    /* Test 1.2: 5 条指令链，mask在第2和第5条 */
    printf("\n--- Test 1.2: 5-instruction chain (vmsne->vadd(masked)->vsub->vadd->vmul(masked)) ---\n");
    VSET_E32_M1(vl, N);
    asm volatile(
        "vle32.v " VA ", (%0)\n\t"
        "vle32.v " VB ", (%1)\n\t"
        "vle32.v " VS ", (%2)\n\t"
        : : "r"(a), "r"(b), "r"(c) : "memory", "v0", VA, VB, VS
    );

    s = read_cycles();
    asm volatile(
        "vmsne.vv v0, " VA ", " VB "\n\t"
        "vadd.vv " VT ", " VA ", " VB ", v0.t\n\t"
        "vsub.vv " VT ", " VT ", " VA "\n\t"
        "vadd.vv " VT ", " VT ", v1\n\t"
        "vmul.vv " VO ", " VT ", " VA ", v0.t\n\t"
        : : : "v0", VA, VB, VS, VT, VO, "memory"
    );
    uint64_t t1_2 = read_cycles() - s;  /* 改为 t1_2 */

    asm volatile("vse32.v " VO ", (%0)\n\t" : : "r"(out) : "memory", VO);
    printf("cycles (5-inst, mask #2 and #5): %lu\n", t1_2);

    /* Test 2: 扩展为 6 条指令，混合 mask/unmasked */
    printf("\n--- Test 2: 6-instruction chain (unmasked + masked interleaved) ---\n");
    for (int i = 0; i < N; i++) {
        b[i] = (i % 2 == 0) ? a[i] : a[i] + 100;
    }

    VSET_E32_M1(vl, N);
    asm volatile(
        "vle32.v " VA ", (%0)\n\t"
        "vle32.v " VB ", (%1)\n\t"
        "vle32.v " VS ", (%2)\n\t"
        : : "r"(a), "r"(b), "r"(c) : "memory", "v0", VA, VB, VS
    );

    s = read_cycles();
    asm volatile(
        "vmsne.vv v0, " VA ", " VB "\n\t"           /* 1. mask: odd indices */
        "vadd.vv " VT ", " VA ", " VB "\n\t"        /* 2. unmasked add (parallel to mask use) */
        "vsub.vv " VT ", " VT ", " VA ", v0.t\n\t"  /* 3. masked sub (chained on VT) */
        "vmul.vv " VU ", " VT ", " VS "\n\t"        /* 4. unmasked mul (chained) */
        "vadd.vv " VU ", " VU ", " VA ", v0.t\n\t"  /* 5. masked add (chained) */
        "vsub.vv " VO ", " VU ", " VS ", v0.t\n\t"  /* 6. masked sub -> output */
        : : : "v0", VA, VB, VS, VT, VU, VO, "memory"
    );
    uint64_t t2 = read_cycles() - s;

    asm volatile("vse32.v " VO ", (%0)\n\t" : : "r"(out) : "memory", VO);
    printf("cycles (6-inst mixed): %lu\n", t2);



    /* Test 3: 扩展为 7 条指令，多级 mask 依赖 */
    printf("\n--- Test 3: 7-instruction chain (mask->op->mask->op...) ---\n");
    for (int i = 0; i < N; i++) { a[i] = i+1; b[i] = i+101; }

    VSET_E32_M1(vl, N);
    asm volatile(
        "vle32.v " VA ", (%0)\n\t"
        "vle32.v " VB ", (%1)\n\t"
        "vle32.v " VS ", (%2)\n\t"
        : : "r"(a), "r"(b), "r"(c) : "memory", "v0", VA, VB, VS
    );

    s = read_cycles();
asm volatile(
    "vmsgt.vx v0, " VA ", %0\n\t"               /* 1. mask1: a > 8 */
    "vadd.vv " VT ", " VA ", " VB ", v0.t\n\t"  /* 2. masked add */
    "vsub.vv " VT ", " VT ", " VA ", v0.t\n\t"  /* 3. chained sub */
    "vmsgt.vx v0, " VT ", %1\n\t"               /* 4. mask2: temp > 50 */
    "vmul.vv " VU ", " VT ", " VS ", v0.t\n\t"  /* 5. masked mul */
    "vadd.vv " VU ", " VU ", " VA ", v0.t\n\t"  /* 6. masked add */
    "vsub.vv " VO ", " VU ", " VS ", v0.t\n\t"  /* 7. final sub */
    : : "r"(8), "r"(50) : "v0", VA, VB, VS, VT, VU, VO, "memory"
);
    uint64_t t3 = read_cycles() - s;

    asm volatile("vse32.v " VO ", (%0)\n\t" : : "r"(out) : "memory", VO);
    printf("cycles (7-inst dual-mask): %lu\n", t3);
    printf("out[0:3]: %u %u %u %u\n", out[0], out[1], out[2], out[3]);

    /* Test 4: 扩展为 8 条指令，复杂数据流 (原 3 条: vadd,vmsgt,vadd) */
    printf("\n--- Test 4: 8-instruction deep chain (vadd->vmsgt->vadd->vsub->vmul->vadd->vsub->vadd) ---\n");
    for (int i = 0; i < N; i++) { a[i] = i+1; b[i] = i+2; c[i] = i+10; }
    const uint64_t TH = 20;

    VSET_E32_M1(vl, N);
    asm volatile(
        "vle32.v " VA ", (%0)\n\t"
        "vle32.v " VB ", (%1)\n\t"
        "vle32.v " VS ", (%2)\n\t"
        : : "r"(a), "r"(b), "r"(c) : "memory", "v0", VA, VB, VS
    );

    s = read_cycles();
    asm volatile(
        "vadd.vv " VT ", " VA ", " VB "\n\t"        /* 1. temp1 = a + b */
        "vmsgt.vx v0, " VT ", %0\n\t"               /* 2. mask = temp1 > TH */
        "vadd.vv " VT ", " VT ", " VB ", v0.t\n\t"  /* 3. masked add (chained) */
        "vsub.vv " VU ", " VT ", " VA ", v0.t\n\t"  /* 4. masked sub (chained) */
        "vmul.vv " VU ", " VU ", " VS ", v0.t\n\t"  /* 5. masked mul (chained) */
        "vadd.vv " VU ", " VU ", " VB ", v0.t\n\t"  /* 6. masked add (chained) */
        "vsub.vv " VT ", " VU ", " VA ", v0.t\n\t"  /* 7. masked sub (chained, write back VT) */
        "vadd.vv " VO ", " VT ", " VS ", v0.t\n\t"  /* 8. final masked add -> output */
        : : "r"(TH) : "v0", VA, VB, VS, VT, VU, VO, "memory"
    );
    uint64_t t4 = read_cycles() - s;

    asm volatile("vse32.v " VO ", (%0)\n\t" : : "r"(out) : "memory", VO);
    printf("cycles (8-inst deep): %lu\n", t4);
    printf("out[0:3]: %u %u %u %u\n", out[0], out[1], out[2], out[3]);

    /* Test 5: 无 chaining 对照 - 同样 8 条指令但用 store/reload 打断 */
    printf("\n--- Test 5: 8-instruction NO chaining (store/reload between each) ---\n");
    for (int i = 0; i < N; i++) { a[i] = i+1; b[i] = i+2; c[i] = i+10; tmp[i] = 0; }
    const uint64_t TH2 = 20;

    VSET_E32_M1(vl, N);
    /* Preload and break chaining before timing */
    asm volatile(
        "vle32.v " VA ", (%0)\n\t"
        "vle32.v " VB ", (%1)\n\t"
        "vle32.v " VS ", (%2)\n\t"
        : : "r"(a), "r"(b), "r"(c) : "memory", VA, VB, VS
    );

    s = read_cycles();
    /* 每条指令后都 store/reload 到临时内存，强制打断 forwarding */
    asm volatile(
        "vadd.vv " VT ", " VA ", " VB "\n\t"
        "vse32.v " VT ", (%0)\n\t"
        "vle32.v " VT ", (%0)\n\t"
        "vmsgt.vx v0, " VT ", %1\n\t"
        "vadd.vv " VT ", " VT ", " VB ", v0.t\n\t"
        "vse32.v " VT ", (%0)\n\t"
        "vle32.v " VT ", (%0)\n\t"
        "vsub.vv " VU ", " VT ", " VA ", v0.t\n\t"
        "vse32.v " VU ", (%0)\n\t"
        "vle32.v " VU ", (%0)\n\t"
        "vmul.vv " VU ", " VU ", " VS ", v0.t\n\t"
        "vse32.v " VU ", (%0)\n\t"
        "vle32.v " VU ", (%0)\n\t"
        "vadd.vv " VU ", " VU ", " VB ", v0.t\n\t"
        "vse32.v " VU ", (%0)\n\t"
        "vle32.v " VU ", (%0)\n\t"
        "vsub.vv " VT ", " VU ", " VA ", v0.t\n\t"
        "vse32.v " VT ", (%0)\n\t"
        "vle32.v " VT ", (%0)\n\t"
        "vadd.vv " VO ", " VT ", " VS ", v0.t\n\t"
        : : "r"(tmp), "r"(TH2) : "memory", "v0", VA, VB, VS, VT, VU, VO
    );
    uint64_t t5 = read_cycles() - s;

    asm volatile("vse32.v " VO ", (%0)\n\t" : : "r"(out) : "memory", VO);
    printf("cycles (8-inst NO chain): %lu\n", t5);
    printf("out[0:3]: %u %u %u %u\n", out[0], out[1], out[2], out[3]);


    /* Test 6: 扩展为 6 条指令，mask->multiply chain */
    printf("\n--- Test 6: 6-instruction mul chain (vmsne->vmul->vadd->vmul->vsub->vmul) ---\n");
    for (int i = 0; i < N; i++) { a[i] = i+1; b[i] = i+2; c[i] = i+3; }

    VSET_E32_M1(vl, N);
    asm volatile(
        "vle32.v " VA ", (%0)\n\t"
        "vle32.v " VB ", (%1)\n\t"
        "vle32.v " VS ", (%2)\n\t"
        : : "r"(a), "r"(b), "r"(c) : "memory", "v0", VA, VB, VS
    );

    s = read_cycles();
    asm volatile(
        "vmsne.vv v0, " VA ", " VB "\n\t"           /* 1. mask */
        "vmul.vv " VT ", " VA ", " VB ", v0.t\n\t"  /* 2. a * b */
        "vadd.vv " VT ", " VT ", " VS ", v0.t\n\t"  /* 3. + c */
        "vmul.vv " VT ", " VT ", " VA ", v0.t\n\t"  /* 4. * a */
        "vsub.vv " VT ", " VT ", " VB ", v0.t\n\t"  /* 5. - b */
        "vmul.vv " VO ", " VT ", " VS ", v0.t\n\t"  /* 6. * c -> out */
        : : : "v0", VA, VB, VS, VT, VO, "memory"
    );
    uint64_t t6 = read_cycles() - s;

    asm volatile("vse32.v " VO ", (%0)\n\t" : : "r"(out) : "memory", VO);
    printf("cycles (6-inst mul chain): %lu\n", t6);
    printf("out[0:3]: %u %u %u %u\n", out[0], out[1], out[2], out[3]);
    
    
    printf("\n=== SUMMARY ===\n");
    printf("Test 1   (5-inst all masked):    %lu cycles\n", t1);
    printf("Test 1.1 (5-inst mask only #2):  %lu cycles\n", t1_1);
    printf("Test 1.2 (5-inst mask #2,#5):    %lu cycles\n", t1_2);
    printf("Test 2   (6-inst mixed):          %lu cycles\n", t2);
    printf("Test 3 (7-inst dual-mask):%lu cycles\n", t3);
    printf("Test 4 (8-inst deep):     %lu cycles\n", t4);
    printf("Test 5 (8-inst no-chain): %lu cycles (speedup: %.2fx)\n", t5, (double)t5/t4);
    printf("Test 6 (6-inst mul chain):%lu cycles\n", t6);

    return 0;
}