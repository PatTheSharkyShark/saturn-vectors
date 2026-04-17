.section .text
.p2align 2
.global vec_add_block_traced
.type vec_add_block_traced,@function
// void vec_add_block_traced(double *addr, unsigned long elems, unsigned int bidx)
vec_add_block_traced:
  // a0 = addr, a1 = elems, a2 = bidx
  mv t0, a0
  mv t1, a1
  
  // Calculate offset in bytes (bidx * 8 since long is 64-bit)
  slli a2, a2, 3
  
  la t3, trace_vle_start_c
  la t4, trace_vle_end_c
  la t5, trace_vle_start_i
  la t6, trace_vle_end_i
  add t3, t3, a2
  add t4, t4, a2
  add t5, t5, a2
  add t6, t6, a2

  // load constant 1.0 into fa0
  la a3, 1f
  fld fa0, 0(a3)

  vsetvli t2, t1, e64,m1

  // Marker for waveform: Write unique pattern to mscratch
  li a3, 0x11112222
  csrw mscratch, a3

  // Trace VLE64.V
  rdcycle a4
  rdinstret a5
  sd a4, 0(t3)
  sd a5, 0(t5)
  vle64.v v0, (t0)
  rdcycle a4
  rdinstret a5
  sd a4, 0(t4)
  sd a5, 0(t6)

  // Trace VFADD.VF
  la t3, trace_vfadd_start_c
  la t4, trace_vfadd_end_c
  la t5, trace_vfadd_start_i
  la t6, trace_vfadd_end_i
  add t3, t3, a2
  add t4, t4, a2
  add t5, t5, a2
  add t6, t6, a2
  rdcycle a4
  rdinstret a5
  sd a4, 0(t3)
  sd a5, 0(t5)

  // Clear Marker
  csrw mscratch, zero

  vfadd.vf v0, v0, fa0
  rdcycle a4
  rdinstret a5
  sd a4, 0(t4)
  sd a5, 0(t6)

  // Trace VSE64.V
  la t3, trace_vse_start_c
  la t4, trace_vse_end_c
  la t5, trace_vse_start_i
  la t6, trace_vse_end_i
  add t3, t3, a2
  add t4, t4, a2
  add t5, t5, a2
  add t6, t6, a2

  rdcycle a4
  rdinstret a5
  sd a4, 0(t3)
  sd a5, 0(t5)
  vse64.v v0, (t0)
  rdcycle a4
  rdinstret a5
  sd a4, 0(t4)
  sd a5, 0(t6)

  ret

.section .rodata
.p2align 3
1: .double 1.0
