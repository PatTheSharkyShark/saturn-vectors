.section .text
.p2align 2
.global copy_to_tcm
.type copy_to_tcm,@function
// void copy_to_tcm(void *dst, const void *src, unsigned long n)
copy_to_tcm:
  // a0 = dst, a1 = src, a2 = n (bytes)
  mv t0, a0
  mv t1, a1
  mv t2, a2

  // copy 16-byte chunks
  srli t3, t2, 4        // t3 = n / 16
  beqz t3, .Ltail_to
.Lloop16_to:
  ld t4, 0(t1)
  ld t5, 8(t1)
  sd t4, 0(t0)
  sd t5, 8(t0)
  addi t1, t1, 16
  addi t0, t0, 16
  addi t3, t3, -1
  bnez t3, .Lloop16_to

.Ltail_to:
  andi t4, t2, 15       // t4 = rem bytes (0..15)
  li t6, 8
  blt t4, t6, .Lbytes_to
  // one 8-byte chunk
  ld t5, 0(t1)
  sd t5, 0(t0)
  addi t1, t1, 8
  addi t0, t0, 8
  addi t4, t4, -8
.Lbytes_to:
  beqz t4, .Ldone_to
.Lbyte_loop_to:
  lb t5, 0(t1)
  sb t5, 0(t0)
  addi t1, t1, 1
  addi t0, t0, 1
  addi t4, t4, -1
  bnez t4, .Lbyte_loop_to
.Ldone_to:
  ret

.p2align 2
.global copy_from_tcm
.type copy_from_tcm,@function
// void copy_from_tcm(void *dst, const void *src, unsigned long n)
copy_from_tcm:
  // a0 = dst, a1 = src, a2 = n
  mv t0, a0
  mv t1, a1
  mv t2, a2

  // copy 16-byte chunks
  srli t3, t2, 4
  beqz t3, .Ltail_from
.Lloop16_from:
  ld t4, 0(t1)
  ld t5, 8(t1)
  sd t4, 0(t0)
  sd t5, 8(t0)
  addi t1, t1, 16
  addi t0, t0, 16
  addi t3, t3, -1
  bnez t3, .Lloop16_from

.Ltail_from:
  andi t4, t2, 15
  li t6, 8
  blt t4, t6, .Lbytes_from
  ld t5, 0(t1)
  sd t5, 0(t0)
  addi t1, t1, 8
  addi t0, t0, 8
  addi t4, t4, -8
.Lbytes_from:
  beqz t4, .Ldone_from
.Lbyte_loop_from:
  lb t5, 0(t1)
  sb t5, 0(t0)
  addi t1, t1, 1
  addi t0, t0, 1
  addi t4, t4, -1
  bnez t4, .Lbyte_loop_from
.Ldone_from:
  ret
