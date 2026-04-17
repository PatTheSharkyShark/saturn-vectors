#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "util.h"

#define N 128
#define SEGMENT_N 16 //实际元素数

uint8_t  src8[N]  __attribute__((aligned(128)));
uint16_t src16[N] __attribute__((aligned(128)));
uint32_t src32[N] __attribute__((aligned(128)));
uint64_t src64[N] __attribute__((aligned(128)));

uint8_t  dest8[N]  __attribute__((aligned(128)));
uint16_t dest16[N] __attribute__((aligned(128)));
uint32_t dest32[N] __attribute__((aligned(128)));
uint64_t dest64[N] __attribute__((aligned(128)));

uint32_t idx32[N] __attribute__((aligned(128)));
uint16_t idx16[N] __attribute__((aligned(128)));

struct pixel { uint8_t r, g, b, a; };
struct pixel pixels[SEGMENT_N] __attribute__((aligned(128)));
uint8_t rgba_separated[4][SEGMENT_N] __attribute__((aligned(128)));

#define TCM_BASE 0x70000000U

uint32_t *tcm_src32, *tcm_dest32;
uint16_t *tcm_src16, *tcm_dest16;
uint8_t  *tcm_src8,  *tcm_dest8;
struct pixel *tcm_pixels;
uint8_t (*tcm_rgba)[SEGMENT_N];

void init_tcm_pointers() {
    tcm_src32  = (uint32_t *)TCM_BASE;
    tcm_dest32 = (uint32_t *)(TCM_BASE + 0x1000);
    tcm_src16  = (uint16_t *)(TCM_BASE + 0x2000);
    tcm_dest16 = (uint16_t *)(TCM_BASE + 0x2800);
    tcm_src8   = (uint8_t  *)(TCM_BASE + 0x3000);
    tcm_dest8  = (uint8_t  *)(TCM_BASE + 0x3400);
    tcm_pixels = (struct pixel *)(TCM_BASE + 0x4000);
    tcm_rgba   = (uint8_t (*)[SEGMENT_N])(TCM_BASE + 0x4200);
}

void init_indices(int scale) {
    for (int i = 0; i < N; i++) {
        idx32[i] = i * scale;
        idx16[i] = i;
    }
}

void init_data() {
    for (int i = 0; i < N; i++) {
        src8[i]  = i + 1;
        src16[i] = i + 100;
        src32[i] = i + 1000;
        src64[i] = i + 10000ULL;
    }
    for (int i = 0; i < SEGMENT_N; i++) {
        pixels[i].r = i; pixels[i].g = i+1; 
        pixels[i].b = i+2; pixels[i].a = i+3;
    }
}

void copy_to_tcm() {
    for (int i = 0; i < N; i++) {
        tcm_src32[i] = src32[i];
        tcm_src16[i] = src16[i];
        tcm_src8[i]  = src8[i];
    }
    for (int i = 0; i < SEGMENT_N; i++) {
        tcm_pixels[i] = pixels[i];
    }
}

void flush_cache(void *addr, size_t len) {
    volatile uint8_t *p = (volatile uint8_t *)0x80000000;
    for (int i = 0; i < 65536; i += 64) p[i] = 0;
}

/*===========================================
 * 1. UNIT-STRIDE
 *===========================================*/

void test_unit_stride() {
    size_t vl;
    
    printf("\n=== 1. Unit-Stride ===\n");
    init_data();
    copy_to_tcm();
    
    asm volatile("vsetvli %0, %1, e32, m1, ta, ma" : "=r"(vl) : "r"(N)); // 设置向量配置：元素宽度 e32，组大小 m1，尾部/掩码策略 ta/ma，并返回 VL
    asm volatile("vle32.v v0, (%0)" : : "r"(src32) : "v0"); // 将内存中连续的32位元素加载到向量寄存器 v0
    asm volatile("fence"); // 内存屏障，确保之前的内存操作完成
    asm volatile("vle32.v v0, (%0)" : : "r"(tcm_src32) : "v0"); // 从 TCM 地址加载连续32位元素到 v0
    asm volatile("fence"); // 内存屏障
    
    asm volatile("vsetvli %0, %1, e16, m1, ta, ma" : "=r"(vl) : "r"(N)); // 设置向量：元素宽度 e16
    asm volatile("vle16.v v2, (%0)" : : "r"(src16) : "v2"); // 加载连续的16位元素到向量寄存器 v2
    asm volatile("fence"); // 内存屏障
    asm volatile("vle16.v v2, (%0)" : : "r"(tcm_src16) : "v2"); // 从 TCM 加载16位向量数据到 v2
    asm volatile("fence"); // 内存屏障
    
    asm volatile("vsetvli %0, %1, e8, m1, ta, ma" : "=r"(vl) : "r"(N)); // 设置向量：元素宽度 e8
    asm volatile("vle8.v v4, (%0)" : : "r"(src8) : "v4"); // 加载连续的8位元素到向量寄存器 v4
    asm volatile("fence"); // 内存屏障
    asm volatile("vle8.v v4, (%0)" : : "r"(tcm_src8) : "v4"); // 从 TCM 加载8位向量数据到 v4
    asm volatile("fence"); // 内存屏障
    
    int val10 = 10;
    asm volatile("vsetvli %0, %1, e32, m1, ta, ma" : "=r"(vl) : "r"(N)); // 设置向量：元素宽度 e32
    asm volatile("vmv.v.x v6, %0" :: "r"(val10)); // 将标量 val10 广播到向量寄存器 v6 的每个元素
    asm volatile("vse32.v v6, (%0)" : : "r"(dest32) : "memory"); // 将 v6 中的32位元素存回内存 dest32
    asm volatile("fence"); // 内存屏障
    asm volatile("vse32.v v6, (%0)" : : "r"(tcm_dest32) : "memory"); // 将 v6 存到 TCM 的目标地址
    asm volatile("fence"); // 内存屏障

    /*
     * Stress the memory queues: issue several same-type vector memory ops
     * back-to-back (loads and stores) targeting TCM to try to fill internal
     * queues (VLIQ/VSIQ, segment buffers, LMU/SMU). Use v8..v15 as target
     * registers and varying base offsets so they are distinct requests.
     */
    printf("\n=== STRESS: emit multiple concurrent loads/stores to TCM ===\n");
    asm volatile("vsetvli %0, %1, e32, m1, ta, ma" : "=r"(vl) : "r"(SEGMENT_N));
    /* issue 8 loads to different vector registers */
    asm volatile("vle32.v v8, (%0)"  : : "r"(tcm_src32 + 0*SEGMENT_N)  : "v8");
    asm volatile("vle32.v v9, (%0)"  : : "r"(tcm_src32 + 1*SEGMENT_N)  : "v9");
    asm volatile("vle32.v v10, (%0)" : : "r"(tcm_src32 + 2*SEGMENT_N)  : "v10");
    asm volatile("vle32.v v11, (%0)" : : "r"(tcm_src32 + 3*SEGMENT_N)  : "v11");
    asm volatile("vle32.v v12, (%0)" : : "r"(tcm_src32 + 4*SEGMENT_N)  : "v12");
    asm volatile("vle32.v v13, (%0)" : : "r"(tcm_src32 + 5*SEGMENT_N)  : "v13");
    asm volatile("vle32.v v14, (%0)" : : "r"(tcm_src32 + 6*SEGMENT_N)  : "v14");
    asm volatile("vle32.v v15, (%0)" : : "r"(tcm_src32 + 7*SEGMENT_N)  : "v15");

    /* issue 8 stores from a broadcast vector to different TCM dest offsets */
    asm volatile("vmv.v.x v8, %0" :: "r"(val10));
    asm volatile("vse32.v v8, (%0)"  : : "r"(tcm_dest32 + 0*SEGMENT_N)  : "memory");
    asm volatile("vse32.v v9, (%0)"  : : "r"(tcm_dest32 + 1*SEGMENT_N)  : "memory");
    asm volatile("vse32.v v10, (%0)" : : "r"(tcm_dest32 + 2*SEGMENT_N)  : "memory");
    asm volatile("vse32.v v11, (%0)" : : "r"(tcm_dest32 + 3*SEGMENT_N)  : "memory");
    asm volatile("vse32.v v12, (%0)" : : "r"(tcm_dest32 + 4*SEGMENT_N)  : "memory");
    asm volatile("vse32.v v13, (%0)" : : "r"(tcm_dest32 + 5*SEGMENT_N)  : "memory");
    asm volatile("vse32.v v14, (%0)" : : "r"(tcm_dest32 + 6*SEGMENT_N)  : "memory");
    asm volatile("vse32.v v15, (%0)" : : "r"(tcm_dest32 + 7*SEGMENT_N)  : "memory");
    /* leave a fence after stress block to observe completion */
    asm volatile("fence");
}

/*===========================================
 * 2. STRIDED
 *===========================================*/

void test_strided() {
    size_t vl;
    
    printf("\n=== 2. Strided ===\n");
    
    asm volatile("vsetvli %0, %1, e32, m1, ta, ma" : "=r"(vl) : "r"(N/16)); // 设置向量：元素宽度 e32，VL = N/16
    asm volatile("vlse32.v v0, (%0), %1" : : "r"(src32), "r"(64) : "v0"); // 带步长加载：以字节步长 64 从 src32 加载32位元素到 v0
    asm volatile("fence"); // 内存屏障
    asm volatile("vlse32.v v0, (%0), %1" : : "r"(tcm_src32), "r"(64) : "v0"); // 从 TCM 以步长 64 加载到 v0
    asm volatile("fence"); // 内存屏障
    
    int val5 = 5;
    asm volatile("vmv.v.x v2, %0" :: "r"(val5)); // 将标量 val5 广播到向量寄存器 v2
    asm volatile("vsse32.v v2, (%0), %1" : : "r"(dest32), "r"(32) : "memory"); // 带步长存储：以字节步长 32 将 v2 存到 dest32
    asm volatile("fence"); // 内存屏障
    asm volatile("vsse32.v v2, (%0), %1" : : "r"(tcm_dest32), "r"(32) : "memory"); // 将 v2 存到 TCM，步长 32
    asm volatile("fence"); // 内存屏障
}

/*===========================================
 * 3. INDEXED
 *===========================================*/

void test_indexed() {
    size_t vl;
    
    printf("\n=== 3. Indexed ===\n");
    init_indices(4);
    
    asm volatile("vsetvli %0, %1, e32, m1, ta, ma" : "=r"(vl) : "r"(N/4)); // 设置向量：元素宽度 e32，VL = N/4
    asm volatile("vle32.v v0, (%0)" : : "r"(idx32) : "v0"); // 将索引数组加载到向量寄存器 v0

    asm volatile("vluxei32.v v2, (%0), v0" : : "r"(src32) : "v2", "v0"); // 使用 v0 作为字节索引向量，从 base(src32) 无序加载32位元素到 v2 (索引式加载)
    asm volatile("fence"); // 内存屏障
    asm volatile("vluxei32.v v2, (%0), v0" : : "r"(tcm_src32) : "v2", "v0"); // 对 TCM 地址执行相同的索引加载
    asm volatile("fence"); // 内存屏障

    asm volatile("vloxei32.v v4, (%0), v0" : : "r"(src32) : "v4", "v0"); // 使用 v0 索引进行有序（按索引）加载32位元素到 v4
    asm volatile("fence"); // 内存屏障
    asm volatile("vloxei32.v v4, (%0), v0" : : "r"(tcm_src32) : "v4", "v0"); // 从 TCM 根据索引加载到 v4
    asm volatile("fence"); // 内存屏障
    
    int val7 = 7;
    asm volatile("vmv.v.x v6, %0" :: "r"(val7)); // 将标量 val7 广播到向量寄存器 v6
    asm volatile("vsuxei32.v v6, (%0), v0" : : "r"(dest32) : "memory", "v0"); // 根据索引向量 v0 将 v6 的元素按索引存回 dest32（无序索引存储）
    asm volatile("fence"); // 内存屏障
    asm volatile("vsuxei32.v v6, (%0), v0" : : "r"(tcm_dest32) : "memory", "v0"); // 将 v6 按索引存到 TCM
    asm volatile("fence"); // 内存屏障
}

/*===========================================
 * 4. SEGMENT
 *===========================================*/

void test_segment() {
    size_t vl;
    
    printf("\n=== 4. Segment ===\n");
    
    asm volatile("vsetvli %0, %1, e8, m1, ta, ma" : "=r"(vl) : "r"(SEGMENT_N)); // 设置向量：元素宽度 e8，VL = SEGMENT_N
    asm volatile("vlseg4e8.v v0, (%0)" : : "r"(pixels) : "v0", "v1", "v2", "v3"); // 分段加载：将内存中连续的4通道8位数据加载到 v0-v3
    asm volatile("fence"); // 内存屏障
    asm volatile("vlseg4e8.v v0, (%0)" : : "r"(tcm_pixels) : "v0", "v1", "v2", "v3"); // 从 TCM 分段加载到 v0-v3
    asm volatile("fence"); // 内存屏障

    asm volatile("vsseg4e8.v v0, (%0)" : : "r"(rgba_separated) : "memory"); // 分段存储：将 v0-v3 存为 4 通道 8 位数组
    asm volatile("fence"); // 内存屏障
    asm volatile("vsseg4e8.v v0, (%0)" : : "r"(tcm_rgba) : "memory"); // 将 v0-v3 存到 TCM 的分段数组
    asm volatile("fence"); // 内存屏障

    asm volatile("vlsseg4e8.v v8, (%0), %1" : : "r"(pixels), "r"(4) : "v8", "v9", "v10", "v11"); // 带步长分段加载：步长为4，加载到 v8-v11
    asm volatile("fence"); // 内存屏障
    asm volatile("vlsseg4e8.v v8, (%0), %1" : : "r"(tcm_pixels), "r"(4) : "v8", "v9", "v10", "v11"); // 从 TCM 以步长4加载到 v8-v11
    asm volatile("fence"); // 内存屏障
}

/*===========================================
 * 5. WIDTH CONVERSION
 *===========================================*/

void test_width_conv() {
    size_t vl;
    
    printf("\n=== 5. Width Conversion ===\n");
    
    asm volatile("vsetvli %0, %1, e16, m1, ta, ma" : "=r"(vl) : "r"(N)); // 设置向量：元素宽度 e16
    asm volatile("vle16.v v0, (%0)\n\tvsext.vf2 v2, v0" : : "r"(src16) : "v0", "v2"); // 加载16位到 v0，随后将 v0 按位拓宽并执行符号扩展到 v2
    asm volatile("fence"); // 内存屏障
    asm volatile("vle16.v v0, (%0)\n\tvsext.vf2 v2, v0" : : "r"(tcm_src16) : "v0", "v2"); // 在 TCM 上执行相同的加载+符号扩展
    asm volatile("fence"); // 内存屏障
    
    asm volatile("vsetvli %0, %1, e8, m1, ta, ma" : "=r"(vl) : "r"(N)); // 设置向量：元素宽度 e8
    asm volatile("vle8.v v4, (%0)\n\tvzext.vf4 v6, v4" : : "r"(src8) : "v4", "v6"); // 加载8位到 v4，然后零扩展并拓宽到 v6
    asm volatile("fence"); // 内存屏障
    asm volatile("vle8.v v4, (%0)\n\tvzext.vf4 v6, v4" : : "r"(tcm_src8) : "v4", "v6"); // 在 TCM 上执行相同的加载+零扩展
    asm volatile("fence"); // 内存屏障
}

/*===========================================
 * 6. MASKED BYTE
 *===========================================*/

void test_masked_byte() {
    size_t vl;
    
    printf("\n=== 6. Masked Byte ===\n");
    
    uint32_t mask_pat[8] = {1,1,1,1,0,0,0,0};
    asm volatile("vsetvli %0, %1, e32, m1, ta, ma" : "=r"(vl) : "r"(8)); // 设置向量：元素宽度 e32，VL=8 用于掩码加载
    asm volatile("vle32.v v0, (%0)" : : "r"(mask_pat) : "v0"); // 将掩码模式加载到 v0
    asm volatile("vmsne.vx v0, v0, x0" : : : "v0"); // 比较 v0 与寄存器 x0，生成掩码 (非等于时置1)

    int val15 = 15;
    asm volatile("vsetvli %0, %1, e8, m1, ta, ma" : "=r"(vl) : "r"(32)); // 设置向量：元素宽度 e8，VL=32
    asm volatile("vmv.v.x v1, %0" :: "r"(val15)); // 将标量 val15 广播到向量 v1

    asm volatile("vse8.v v1, (%0), v0.t" : : "r"(dest8) : "memory"); // 按掩码 v0 的真位将 v1 的字节存储到 dest8
    asm volatile("fence"); // 内存屏障
    asm volatile("vse8.v v1, (%0), v0.t" : : "r"(tcm_dest8) : "memory"); // 在 TCM 上按掩码存储
    asm volatile("fence"); // 内存屏障
}

int main() {
    init_tcm_pointers();
    
    printf("RVV Memory Test (DRAM vs TCM)\n");
    test_unit_stride();
    test_strided();
    test_indexed();
    test_segment();
    test_width_conv();
    test_masked_byte();
    
    printf("\nDone\n");
    return 0;
}